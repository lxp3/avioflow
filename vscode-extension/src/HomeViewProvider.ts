import * as vscode from 'vscode';

export class HomeViewProvider implements vscode.WebviewViewProvider {

    public static readonly viewType = 'avioflow.homeView';

    private _view?: vscode.WebviewView;

    constructor(
        private readonly _extensionUri: vscode.Uri,
    ) {}

    public resolveWebviewView(
        webviewView: vscode.WebviewView,
        context: vscode.WebviewViewResolveContext,
        _token: vscode.CancellationToken,
    ) {
        this._view = webviewView;

        webviewView.webview.options = {
            enableScripts: true,
            localResourceRoots: [this._extensionUri]
        };

        webviewView.webview.html = this._getHtmlForWebview(webviewView.webview);

        webviewView.webview.onDidReceiveMessage(data => {
            if (data.type === 'openUpload') {
                vscode.commands.executeCommand('avioflow.openWelcome');
            }
        });
    }

    private _getHtmlForWebview(webview: vscode.Webview) {
        return `<!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <title>Avioflow Home</title>
                <style>
                    * {
                        margin: 0;
                        padding: 0;
                        box-sizing: border-box;
                    }
                    body {
                        font-family: var(--vscode-font-family);
                        color: var(--vscode-foreground);
                        background: var(--vscode-sideBar-background);
                        padding: 16px;
                    }
                    .container {
                        display: flex;
                        flex-direction: column;
                        gap: 16px;
                    }
                    .logo {
                        display: flex;
                        align-items: center;
                        gap: 10px;
                        margin-bottom: 8px;
                    }
                    .logo-icon {
                        width: 32px;
                        height: 32px;
                        border-radius: 8px;
                        background: linear-gradient(135deg, #3b82f6, #06b6d4);
                        display: flex;
                        align-items: center;
                        justify-content: center;
                        color: white;
                    }
                    .logo-text {
                        font-size: 18px;
                        font-weight: 700;
                        color: var(--vscode-foreground);
                    }
                    .description {
                        font-size: 12px;
                        color: var(--vscode-descriptionForeground);
                        line-height: 1.5;
                    }
                    .features {
                        display: flex;
                        flex-direction: column;
                        gap: 8px;
                    }
                    .feature {
                        display: flex;
                        align-items: center;
                        gap: 8px;
                        font-size: 11px;
                        color: var(--vscode-descriptionForeground);
                    }
                    .feature-icon {
                        width: 16px;
                        height: 16px;
                        color: #3b82f6;
                    }
                    .open-btn {
                        margin-top: 8px;
                        padding: 8px 16px;
                        background: var(--vscode-button-background);
                        color: var(--vscode-button-foreground);
                        border: none;
                        border-radius: 4px;
                        font-size: 12px;
                        cursor: pointer;
                        transition: background 0.2s;
                    }
                    .open-btn:hover {
                        background: var(--vscode-button-hoverBackground);
                    }
                </style>
            </head>
            <body>
                <div class="container">
                    <div class="logo">
                        <div class="logo-icon">
                            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                                <path d="M9 18V5l12-2v13"/>
                                <circle cx="6" cy="18" r="3"/>
                                <circle cx="18" cy="16" r="3"/>
                            </svg>
                        </div>
                        <span class="logo-text">Avioflow</span>
                    </div>

                    <p class="description">
                        基于 WebAssembly + Ffmpeg7，支持多种音频格式的快速解码与波形可视化。
                    </p>

                    <div class="features">
                        <div class="feature">
                            <svg class="feature-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                                <polyline points="20 6 9 17 4 12"/>
                            </svg>
                            <span>支持 WAV, MP3, FLAC, OGG, AAC等任意音频格式</span>
                        </div>
                        <div class="feature">
                            <svg class="feature-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                                <polyline points="20 6 9 17 4 12"/>
                            </svg>
                            <span>波形可视化</span>
                        </div>
                        <div class="feature">
                            <svg class="feature-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                                <polyline points="20 6 9 17 4 12"/>
                            </svg>
                            <span>音频播放与缩放控制</span>
                        </div>
                    </div>

                    <button class="open-btn" id="openBtn">
                        打开音频文件
                    </button>
                </div>
                <script>
                    const vscode = acquireVsCodeApi();
                    document.getElementById('openBtn').addEventListener('click', () => {
                        vscode.postMessage({ type: 'openUpload' });
                    });
                </script>
            </body>
            </html>`;
    }
}
