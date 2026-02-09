import * as vscode from 'vscode';
import * as path from 'path';

export class WelcomeEditorProvider {

    private static panel: vscode.WebviewPanel | undefined;

    constructor(
        private readonly context: vscode.ExtensionContext
    ) {}

    public static register(context: vscode.ExtensionContext): vscode.Disposable {
        const provider = new WelcomeEditorProvider(context);

        return vscode.commands.registerCommand('avioflow.openWelcome', () => {
            provider.openWelcomePanel();
        });
    }

    public openWelcomePanel() {
        if (WelcomeEditorProvider.panel) {
            WelcomeEditorProvider.panel.reveal(vscode.ViewColumn.One);
            return;
        }

        WelcomeEditorProvider.panel = vscode.window.createWebviewPanel(
            'avioflow.welcome',
            'Avioflow',
            vscode.ViewColumn.One,
            {
                enableScripts: true,
                retainContextWhenHidden: true,
                localResourceRoots: [
                    vscode.Uri.file(path.join(this.context.extensionPath, 'out'))
                ]
            }
        );

        WelcomeEditorProvider.panel.webview.html = this.getHtmlForWebview(WelcomeEditorProvider.panel.webview);

        WelcomeEditorProvider.panel.webview.onDidReceiveMessage(async (data) => {
            if (data.type === 'selectFile') {
                await this.handleFileSelect();
            }
        });

        WelcomeEditorProvider.panel.onDidDispose(() => {
            WelcomeEditorProvider.panel = undefined;
        });
    }

    private async handleFileSelect() {
        const fileUri = await vscode.window.showOpenDialog({
            canSelectFiles: true,
            canSelectFolders: false,
            canSelectMany: false,
            filters: {
                'Audio Files': ['wav', 'mp3', 'flac', 'ogg', 'aac', 'm4a']
            },
            title: 'Select Audio File'
        });

        if (fileUri && fileUri[0]) {
            // Close welcome panel and open audio file
            if (WelcomeEditorProvider.panel) {
                WelcomeEditorProvider.panel.dispose();
            }
            await vscode.commands.executeCommand('vscode.openWith', fileUri[0], 'avioflow.audioPreview');
        }
    }

    private getHtmlForWebview(webview: vscode.Webview): string {
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
                <title>Avioflow</title>
                <style>
                    * {
                        margin: 0;
                        padding: 0;
                        box-sizing: border-box;
                    }
                    body {
                        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
                        background: #ffffff;
                        min-height: 100vh;
                        display: flex;
                        align-items: center;
                        justify-content: center;
                    }
                    .welcome-container {
                        display: flex;
                        flex-direction: column;
                        align-items: center;
                        justify-content: center;
                        padding: 40px;
                        max-width: 600px;
                        width: 100%;
                    }
                    .dropzone {
                        width: 100%;
                        min-height: 280px;
                        border: 2px dashed #d1d5db;
                        border-radius: 24px;
                        display: flex;
                        flex-direction: column;
                        align-items: center;
                        justify-content: center;
                        padding: 48px;
                        cursor: pointer;
                        transition: all 0.2s ease;
                        background: #fafafa;
                    }
                    .dropzone:hover {
                        border-color: #3b82f6;
                        background: #eff6ff;
                    }
                    .dropzone.dragging {
                        border-color: #3b82f6;
                        background: #dbeafe;
                        box-shadow: 0 0 0 4px rgba(59, 130, 246, 0.1);
                    }
                    .upload-icon {
                        width: 72px;
                        height: 72px;
                        border-radius: 20px;
                        display: flex;
                        align-items: center;
                        justify-content: center;
                        background: linear-gradient(135deg, #3b82f6, #06b6d4);
                        color: white;
                        margin-bottom: 24px;
                        box-shadow: 0 8px 24px rgba(59, 130, 246, 0.3);
                        transition: transform 0.2s ease;
                    }
                    .dropzone:hover .upload-icon {
                        transform: scale(1.05);
                    }
                    .upload-title {
                        font-size: 18px;
                        font-weight: 600;
                        color: #1f2937;
                        margin-bottom: 8px;
                    }
                    .upload-subtitle {
                        font-size: 14px;
                        color: #6b7280;
                        margin-bottom: 16px;
                    }
                    .upload-hint {
                        font-size: 13px;
                        color: #3b82f6;
                    }
                </style>
            </head>
            <body>
                <div class="welcome-container">
                    <div class="dropzone" id="dropzone">
                        <div class="upload-icon">
                            <svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/>
                                <polyline points="17 8 12 3 7 8"/>
                                <line x1="12" y1="3" x2="12" y2="15"/>
                            </svg>
                        </div>
                        <p class="upload-title">拖拽文件到此处或点击上传</p>
                        <p class="upload-subtitle">支持 WAV, MP3, FLAC 等音频格式</p>
                        <p class="upload-hint">可同时上传多个文件</p>
                    </div>
                </div>
                <script>
                    const vscode = acquireVsCodeApi();
                    const dropzone = document.getElementById('dropzone');

                    dropzone.addEventListener('click', () => {
                        vscode.postMessage({ type: 'selectFile' });
                    });

                    dropzone.addEventListener('dragover', (e) => {
                        e.preventDefault();
                        dropzone.classList.add('dragging');
                    });

                    dropzone.addEventListener('dragleave', (e) => {
                        e.preventDefault();
                        dropzone.classList.remove('dragging');
                    });

                    dropzone.addEventListener('drop', (e) => {
                        e.preventDefault();
                        dropzone.classList.remove('dragging');
                        vscode.postMessage({ type: 'selectFile' });
                    });
                </script>
            </body>
            </html>
        `;
    }
}
