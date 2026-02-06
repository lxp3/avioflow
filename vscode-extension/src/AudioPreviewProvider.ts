import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import { AudioDecoderService } from './AudioDecoderService';

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
            const filePath = document.uri.fsPath;

            if (!fs.existsSync(filePath)) {
                const errorMsg = `Audio file not found: ${path.basename(filePath)}`;
                console.error(`[Avioflow] ${errorMsg}`);
                vscode.window.showErrorMessage(`Avioflow: ${errorMsg}`);
                webviewPanel.webview.postMessage({ type: 'error', message: errorMsg });
                return;
            }

            const stats = fs.statSync(filePath);
            if (!stats.isFile()) {
                const errorMsg = `Path is not a file: ${path.basename(filePath)}`;
                console.error(`[Avioflow] ${errorMsg}`);
                webviewPanel.webview.postMessage({ type: 'error', message: errorMsg });
                return;
            }

            const service = AudioDecoderService.getInstance();

            // Phase 1: Quick metadata loading - show UI immediately
            console.log('[Avioflow] Phase 1: Loading metadata...');
            const { metadata, decoder } = await service.getMetadata(filePath);

            // Send metadata immediately - UI can show info while samples load
            webviewPanel.webview.postMessage({
                type: 'metadata',
                filePath: filePath,
                metadata: metadata
            });

            // Phase 2: Async sample decoding - runs in background
            console.log('[Avioflow] Phase 2: Decoding samples...');
            const { samples, decodeTimeMs } = await service.getSamples(decoder);

            // Send samples when ready - UI can now draw waveform
            webviewPanel.webview.postMessage({
                type: 'samples',
                samples: samples,
                decodeTimeMs: decodeTimeMs
            });

            console.log(`[Avioflow] Complete. Decode time: ${decodeTimeMs}ms`);

        } catch (e: any) {
            console.error('[Avioflow] loadAndSendData error:', e);
            vscode.window.showErrorMessage(`Avioflow Error: ${e.message}`);
            webviewPanel.webview.postMessage({ type: 'error', message: e.message });
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
