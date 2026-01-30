import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import * as avioflow from 'avioflow';

export class AudioPreviewProvider implements vscode.CustomReadonlyEditorProvider {

    public static register(context: vscode.ExtensionContext): vscode.Disposable {
        return vscode.window.registerCustomEditorProvider(
            AudioPreviewProvider.viewType,
            new AudioPreviewProvider(context),
            {
                webviewOptions: {
                    retainContextWhenHidden: true,
                },
                supportsMultipleEditorsPerDocument: false,
            }
        );
    }

    private static readonly viewType = 'avioflow.audioPreview';

    constructor(
        private readonly context: vscode.ExtensionContext
    ) { }

    public async openCustomDocument(
        uri: vscode.Uri,
        openContext: vscode.CustomDocumentOpenContext,
        token: vscode.CancellationToken
    ): Promise<vscode.CustomDocument> {
        return { uri, dispose: () => { } };
    }

    public async resolveCustomEditor(
        document: vscode.CustomDocument,
        webviewPanel: vscode.WebviewPanel,
        token: vscode.CancellationToken
    ): Promise<void> {
        webviewPanel.webview.options = {
            enableScripts: true,
            localResourceRoots: [
                vscode.Uri.file(path.join(this.context.extensionPath, 'out')),
                vscode.Uri.file(path.join(this.context.extensionPath, 'assets'))
            ]
        };

        webviewPanel.webview.html = this.getHtmlForWebview(webviewPanel.webview);

        webviewPanel.webview.onDidReceiveMessage(e => {
            if (e.type === 'ready') {
                this.loadAndSendData(document, webviewPanel);
            }
        });
    }

    private async loadAndSendData(document: vscode.CustomDocument, webviewPanel: vscode.WebviewPanel) {
        try {
            // @ts-ignore
            const decoder = new avioflow.AudioDecoder();
            // load() returns metadata with camelCase properties
            const metadata = decoder.load(document.uri.fsPath);

            // Since get_all_samples is not in the provided bindings, we collect frames manually
            // and merge them into channel buffers.
            const frames: Float32Array[][] = [];
            while (!decoder.isFinished()) {
                const frame = decoder.decodeNext();
                if (frame) {
                    frames.push(frame);
                }
            }

            // Merge frames into continuous buffers per channel
            const numChannels = metadata.numChannels;
            const samples: Float32Array[] = [];
            for (let c = 0; c < numChannels; c++) {
                const totalLength = frames.reduce((sum, f) => sum + f[c].length, 0);
                const channelData = new Float32Array(totalLength);
                let offset = 0;
                for (const frame of frames) {
                    channelData.set(frame[c], offset);
                    offset += frame[c].length;
                }
                samples.push(channelData);
            }

            webviewPanel.webview.postMessage({
                type: 'init',
                filePath: document.uri.fsPath,
                metadata,
                samples: samples.map(s => Array.from(s))
            });

        } catch (e: any) {
            vscode.window.showErrorMessage(`Avioflow Error: ${e.message}`);
        }
    }

    private getHtmlForWebview(webview: vscode.Webview): string {
        const scriptUri = webview.asWebviewUri(vscode.Uri.file(
            path.join(this.context.extensionPath, 'out', 'webview', 'main.js')
        ));
        const styleUri = webview.asWebviewUri(vscode.Uri.file(
            path.join(this.context.extensionPath, 'out', 'webview', 'style.css')
        ));

        return `
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <link href="${styleUri}" rel="stylesheet" />
                <title>Audio Preview</title>
            </head>
            <body>
                <script src="${scriptUri}"></script>
            </body>
            </html>
        `;
    }
}
