import * as vscode from 'vscode';
import * as path from 'path';

export class HomeViewProvider implements vscode.WebviewViewProvider {

    public static readonly viewType = 'avioflow.homeView';

    private _view?: vscode.WebviewView;

    constructor(
        private readonly _extensionUri: vscode.Uri,
    ) { }

    public resolveWebviewView(
        webviewView: vscode.WebviewView,
        context: vscode.WebviewViewResolveContext,
        _token: vscode.CancellationToken,
    ) {
        this._view = webviewView;

        webviewView.webview.options = {
            enableScripts: true,
            localResourceRoots: [
                this._extensionUri
            ]
        };

        webviewView.webview.html = this._getHtmlForWebview(webviewView.webview);

        webviewView.webview.onDidReceiveMessage(data => {
            switch (data.type) {
                case 'openFile':
                    {
                        vscode.commands.executeCommand('workbench.action.files.openFile');
                        break;
                    }
            }
        });
    }

    private _getHtmlForWebview(webview: vscode.Webview) {
        const styleUri = webview.asWebviewUri(vscode.Uri.joinPath(this._extensionUri, 'out', 'webview', 'style.css'));

        return `<!DOCTYPE html>
			<html lang="en">
			<head>
				<meta charset="UTF-8">
				<meta name="viewport" content="width=device-width, initial-scale=1.0">
				<link href="${styleUri}" rel="stylesheet">
				<title>Avioflow Home</title>
				<style>
					body {
						padding: 20px;
						color: var(--vscode-foreground);
						font-family: var(--vscode-font-family);
						display: flex;
						flex-direction: column;
						align-items: center;
						justify-content: center;
						height: 100vh;
						text-align: center;
					}
					.logo {
						font-size: 3rem;
						font-weight: bold;
						margin-bottom: 10px;
						background: linear-gradient(45deg, #000, #666);
						-webkit-background-clip: text;
						-webkit-text-fill-color: transparent;
					}
					.tagline {
						font-size: 1.1rem;
						opacity: 0.8;
						margin-bottom: 30px;
					}
					button {
						background: var(--vscode-button-background);
						color: var(--vscode-button-foreground);
						border: none;
						padding: 8px 16px;
						cursor: pointer;
						border-radius: 4px;
						font-size: 1rem;
						transition: opacity 0.2s;
					}
					button:hover {
						opacity: 0.9;
					}
					.info-link {
						margin-top: 20px;
						font-size: 0.9rem;
						color: var(--vscode-textLink-foreground);
						text-decoration: none;
					}
				</style>
			</head>
			<body>
				<div class="logo">Avioflow</div>
				<div class="tagline">High-performance Audio Hub</div>
				<button onclick="openFile()">Open Audio File</button>
				<a href="https://github.com" class="info-link">Documentation</a>

				<script>
					const vscode = acquireVsCodeApi();
					function openFile() {
						vscode.postMessage({ type: 'openFile' });
					}
				</script>
			</body>
			</html>`;
    }
}
