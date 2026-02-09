import * as vscode from 'vscode';
import { AudioDecoderService } from './AudioDecoderService';

export function activate(context: vscode.ExtensionContext) {
    // Initialize the WASM-based audio decoder service
    AudioDecoderService.initialize(context.extensionPath);

    try {
        const { AudioPreviewProvider } = require('./AudioPreviewProvider');
        const { HomeViewProvider } = require('./HomeViewProvider');
        const { WelcomeEditorProvider } = require('./WelcomeEditorProvider');

        context.subscriptions.push(
            AudioPreviewProvider.register(context)
        );

        context.subscriptions.push(
            vscode.window.registerWebviewViewProvider(
                'avioflow.homeView',
                new HomeViewProvider(context.extensionUri)
            )
        );

        // Register Welcome panel command
        context.subscriptions.push(
            WelcomeEditorProvider.register(context)
        );

        // Auto-open welcome panel on first activation
        vscode.commands.executeCommand('avioflow.openWelcome');

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
