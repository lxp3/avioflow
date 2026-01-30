import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';

// Lazy load avioflow to avoid crashes if module fails to load
let avioflowModule: any = null;
let avioflowLoadError: Error | null = null;

function getAvioflow() {
    if (avioflowModule) {
        return avioflowModule;
    }
    if (avioflowLoadError) {
        throw avioflowLoadError;
    }
    try {
        // Use dynamic require to catch loading errors
        avioflowModule = require('avioflow').default || require('avioflow');
        console.log('[Avioflow] Native module loaded successfully');
        return avioflowModule;
    } catch (error: any) {
        avioflowLoadError = error;
        console.error('[Avioflow] Failed to load native module:', error.message);
        console.error('[Avioflow] Stack trace:', error.stack);
        throw error;
    }
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
        console.log('[Avioflow] openCustomDocument called for:', uri.fsPath);
        return { uri, dispose: () => { } };
    }

    public async resolveCustomEditor(
        document: vscode.CustomDocument,
        webviewPanel: vscode.WebviewPanel,
        token: vscode.CancellationToken
    ): Promise<void> {
        console.log('[Avioflow] resolveCustomEditor called for:', document.uri.fsPath);

        webviewPanel.webview.options = {
            enableScripts: true,
            localResourceRoots: [
                vscode.Uri.file(path.join(this.context.extensionPath, 'out')),
                vscode.Uri.file(path.join(this.context.extensionPath, 'assets'))
            ]
        };

        console.log('[Avioflow] Setting webview HTML...');
        webviewPanel.webview.html = this.getHtmlForWebview(webviewPanel.webview);
        console.log('[Avioflow] Webview HTML set, waiting for ready message...');

        webviewPanel.webview.onDidReceiveMessage(e => {
            console.log('[Avioflow] Received message from webview:', e.type);
            if (e.type === 'ready') {
                console.log('[Avioflow] Webview ready, loading audio data...');
                this.loadAndSendData(document, webviewPanel);
            }
        });
    }

    private async loadAndSendData(document: vscode.CustomDocument, webviewPanel: vscode.WebviewPanel) {
        console.log('[Avioflow] loadAndSendData started for:', document.uri.fsPath);
        try {
            console.log('[Avioflow] Calling getAvioflow()...');
            const avioflow = getAvioflow();
            console.log('[Avioflow] getAvioflow() returned:', avioflow ? 'module loaded' : 'null');

            if (!avioflow) {
                throw new Error('Avioflow library not loaded');
            }

            console.log('[Avioflow] About to call avioflow.load()...');
            // Use the convenience load() function which returns { metadata, samples }
            // samples is Float32Array[] where each element is one channel's data
            const { metadata, samples } = avioflow.load(document.uri.fsPath);
            console.log('[Avioflow] avioflow.load() completed successfully');
            console.log('[Avioflow] Metadata:', JSON.stringify(metadata));
            console.log('[Avioflow] Samples channels:', samples.length);

            webviewPanel.webview.postMessage({
                type: 'init',
                filePath: document.uri.fsPath,
                metadata,
                samples: samples.map((s: Float32Array) => Array.from(s)) // Convert to array for message passing
            });
            console.log('[Avioflow] Data sent to webview');

        } catch (e: any) {
            console.error('[Avioflow] loadAndSendData error:', e);
            console.error('[Avioflow] Error stack:', e.stack);
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
