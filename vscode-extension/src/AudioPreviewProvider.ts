import * as vscode from 'vscode';
import * as path from 'path';
import { AvioflowWorkerService } from './AvioflowWorkerService';

/**
 * Interface for the message received from the worker
 */
interface WorkerResponse {
    type: 'done' | 'error' | 'ready';
    metadata?: any;
    samples?: any[];
    message?: string;
    stack?: string;
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
            const service = AvioflowWorkerService.getInstance();

            const start = Date.now();
            const { metadata, samples } = await service.load(document.uri.fsPath);
            const duration = Date.now() - start;

            console.log(`[Avioflow] Decoding complete in ${duration}ms, sending to webview`);
            webviewPanel.webview.postMessage({
                type: 'init',
                filePath: document.uri.fsPath,
                metadata,
                samples
            });

        } catch (e: any) {
            console.error('[Avioflow] loadAndSendData error:', e);
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
