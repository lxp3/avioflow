import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import { AudioDecoderService } from './AudioDecoderService';

interface WebviewErrorMessage {
    type: 'error';
    title: string;
    message: string;
    code?: string;
}

export class AudioPreviewProvider implements vscode.CustomReadonlyEditorProvider {
    private static currentPanel?: vscode.WebviewPanel;
    private static currentPanelReady = false;
    private static currentRequestId = 0;


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
        if (AudioPreviewProvider.currentPanel && AudioPreviewProvider.currentPanel !== webviewPanel) {
            await this.reuseExistingEditor(document, webviewPanel);
            return;
        }

        AudioPreviewProvider.currentPanel = webviewPanel;
        AudioPreviewProvider.currentPanelReady = false;
        webviewPanel.title = path.basename(document.uri.fsPath);

        webviewPanel.webview.options = {
            enableScripts: true,
            localResourceRoots: [
                vscode.Uri.file(path.join(this.context.extensionPath, 'out')),
                vscode.Uri.file(path.join(this.context.extensionPath, 'assets'))
            ]
        };

        webviewPanel.webview.html = this.getHtmlForWebview(webviewPanel.webview);

        webviewPanel.webview.onDidReceiveMessage(async e => {
            if (e.type === 'ready') {
                AudioPreviewProvider.currentPanelReady = true;
                await this.loadAndSendData(document, webviewPanel);
            } else if (e.type === 'selectFile') {
                await vscode.commands.executeCommand('avioflow.openAudioFile');
            }
        });

        webviewPanel.onDidDispose(() => {
            if (AudioPreviewProvider.currentPanel === webviewPanel) {
                AudioPreviewProvider.currentPanel = undefined;
                AudioPreviewProvider.currentPanelReady = false;
            }
        });
    }

    private async loadAndSendData(document: vscode.CustomDocument, webviewPanel: vscode.WebviewPanel) {
        const requestId = ++AudioPreviewProvider.currentRequestId;

        try {
            const filePath = document.uri.fsPath;
            webviewPanel.title = path.basename(filePath);
            webviewPanel.webview.postMessage({
                type: 'loading',
                filePath
            });

            if (!fs.existsSync(filePath)) {
                const error = this.createError(
                    'Audio file not found',
                    `The file does not exist: ${filePath}`,
                    'FILE_NOT_FOUND'
                );
                this.reportError(webviewPanel, error);
                return;
            }

            const stats = fs.statSync(filePath);
            if (!stats.isFile()) {
                const error = this.createError(
                    'Invalid path',
                    `The selected path is not a file: ${filePath}`,
                    'INVALID_PATH'
                );
                this.reportError(webviewPanel, error);
                return;
            }

            const service = AudioDecoderService.getInstance();

            // Phase 1: Quick metadata loading - show UI immediately
            console.log('[Avioflow] Phase 1: Loading metadata...');
            const { metadata, decoder } = await service.getMetadata(filePath);
            if (!this.isCurrentRequest(webviewPanel, requestId)) {
                return;
            }

            // Send metadata immediately - UI can show info while samples load
            webviewPanel.webview.postMessage({
                type: 'metadata',
                filePath: filePath,
                metadata: metadata
            });

            // Phase 2: Async sample decoding - runs in background
            console.log('[Avioflow] Phase 2: Decoding samples...');
            const { samples, decodeTimeMs } = await service.getSamples(decoder);
            if (!this.isCurrentRequest(webviewPanel, requestId)) {
                return;
            }

            // Send samples when ready - UI can now draw waveform
            webviewPanel.webview.postMessage({
                type: 'samples',
                samples: samples,
                decodeTimeMs: decodeTimeMs
            });

            console.log(`[Avioflow] Complete. Decode time: ${decodeTimeMs}ms`);

        } catch (e: any) {
            const error = this.normalizeError(e, document.uri.fsPath);
            this.reportError(webviewPanel, error);
        }
    }

    private async reuseExistingEditor(document: vscode.CustomDocument, transientPanel: vscode.WebviewPanel) {
        const panel = AudioPreviewProvider.currentPanel;
        if (!panel) {
            return;
        }

        panel.title = path.basename(document.uri.fsPath);
        panel.reveal(panel.viewColumn);
        transientPanel.dispose();

        if (AudioPreviewProvider.currentPanelReady) {
            await this.loadAndSendData(document, panel);
        }
    }

    private isCurrentRequest(webviewPanel: vscode.WebviewPanel, requestId: number): boolean {
        return (
            AudioPreviewProvider.currentPanel === webviewPanel &&
            AudioPreviewProvider.currentRequestId === requestId
        );
    }

    private reportError(webviewPanel: vscode.WebviewPanel, error: WebviewErrorMessage) {
        console.error('[Avioflow] loadAndSendData error:', error);
        vscode.window.showErrorMessage(`Avioflow: ${error.title}. ${error.message}`);
        webviewPanel.webview.postMessage(error);
    }

    private createError(title: string, message: string, code?: string): WebviewErrorMessage {
        return {
            type: 'error',
            title,
            message,
            code
        };
    }

    private normalizeError(error: unknown, filePath: string): WebviewErrorMessage {
        const rawMessage = error instanceof Error ? error.message : String(error);

        if (rawMessage.includes('WASM module not found') || rawMessage.includes('WASM binary not found')) {
            return this.createError(
                'Decoder runtime is missing',
                rawMessage,
                'WASM_MISSING'
            );
        }

        if (rawMessage.includes('Audio file not found')) {
            return this.createError(
                'Audio file not found',
                rawMessage,
                'FILE_NOT_FOUND'
            );
        }

        return this.createError(
            'Failed to load audio',
            `Could not decode ${path.basename(filePath)}. ${rawMessage}`,
            'DECODE_FAILED'
        );
    }

    private getHtmlForWebview(webview: vscode.Webview): string {
        const scriptUri = webview.asWebviewUri(vscode.Uri.file(
            path.join(this.context.extensionPath, 'out', 'webview', 'main.js')
        ));
        const styleUri = webview.asWebviewUri(vscode.Uri.file(
            path.join(this.context.extensionPath, 'out', 'webview', 'avioflow.css')
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
