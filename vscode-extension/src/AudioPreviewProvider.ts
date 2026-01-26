import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';

// Use the avioflow library
// In a proper extension build, we might need to handle native deps more carefully
let avioflow: any;
try {
    avioflow = require('avioflow').default || require('avioflow');
    console.log('[Avioflow] Library loaded successfully');
} catch (e: any) {
    console.error('[Avioflow] Failed to load avioflow library:', e);
}

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
            if (!avioflow) {
                throw new Error('Avioflow library not loaded');
            }

            const decoder = new avioflow.AudioDecoder();
            decoder.open(document.uri.fsPath);

            const metadata = decoder.getMetadata();

            // Collect all samples
            const allSamples: Float32Array[][] = [];
            while (!decoder.isFinished()) {
                const frame = decoder.decodeNext();
                if (frame && frame.data) {
                    allSamples.push(frame.data);
                }
            }

            // Merge frames into continuous buffers per channel
            const numChannels = metadata.numChannels;
            const samples: Float32Array[] = [];

            for (let c = 0; c < numChannels; c++) {
                const totalLength = allSamples.reduce((sum, frame) => sum + frame[c].length, 0);
                const channelData = new Float32Array(totalLength);
                let offset = 0;
                for (const frame of allSamples) {
                    channelData.set(frame[c], offset);
                    offset += frame[c].length;
                }
                samples.push(channelData);
            }

            webviewPanel.webview.postMessage({
                type: 'init',
                filePath: document.uri.fsPath,
                metadata,
                samples: samples.map(s => Array.from(s)) // Buffer to array for message passing
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
