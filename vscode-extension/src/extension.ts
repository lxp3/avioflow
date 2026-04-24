import * as vscode from 'vscode';
import { AudioDecoderService } from './AudioDecoderService';

export function activate(context: vscode.ExtensionContext) {
    // Initialize the WASM-based audio decoder service
    AudioDecoderService.initialize(context.extensionPath);

    try {
        const { AudioPreviewProvider } = require('./AudioPreviewProvider');
        const { HomeViewProvider } = require('./HomeViewProvider');

        context.subscriptions.push(
            AudioPreviewProvider.register(context)
        );

        context.subscriptions.push(
            vscode.window.registerWebviewViewProvider(
                'avioflow.homeView',
                new HomeViewProvider(context.extensionUri)
            )
        );

        context.subscriptions.push(
            vscode.commands.registerCommand('avioflow.openAudioFile', async () => {
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
                    await vscode.commands.executeCommand(
                        'vscode.openWith',
                        fileUri[0],
                        'avioflow.audioPreview'
                    );
                }
            })
        );

    } catch (error: any) {
        console.error('[Avioflow] Extension activation failed:', error.message);
        console.error('[Avioflow] Stack:', error.stack);
        vscode.window.showErrorMessage(`Avioflow extension failed to activate: ${error.message}`);
    }
}

export function deactivate() {
    console.log('[Avioflow] Extension deactivated');
    AudioDecoderService.getInstance().dispose();
}
