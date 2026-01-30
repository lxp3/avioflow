import * as vscode from 'vscode';

export function activate(context: vscode.ExtensionContext) {
    console.log('[Avioflow] Extension activation started...');

    // Log version information for debugging
    console.log('=== Runtime Version Information ===');
    console.log('Node.js version:', process.versions.node);
    console.log('V8 version:', process.versions.v8);
    console.log('Electron version:', process.versions.electron || 'Not running in Electron');
    console.log('Chrome version:', process.versions.chrome || 'N/A');
    console.log('ABI version:', process.versions.modules);
    console.log('Platform:', process.platform);
    console.log('Arch:', process.arch);
    console.log('===================================');

    try {
        console.log('[Avioflow] Loading AudioPreviewProvider...');
        const { AudioPreviewProvider } = require('./AudioPreviewProvider');
        console.log('[Avioflow] AudioPreviewProvider loaded');

        console.log('[Avioflow] Loading HomeViewProvider...');
        const { HomeViewProvider } = require('./HomeViewProvider');
        console.log('[Avioflow] HomeViewProvider loaded');

        console.log('[Avioflow] Registering AudioPreviewProvider...');
        context.subscriptions.push(
            AudioPreviewProvider.register(context)
        );
        console.log('[Avioflow] AudioPreviewProvider registered');

        console.log('[Avioflow] Registering HomeViewProvider...');
        context.subscriptions.push(
            vscode.window.registerWebviewViewProvider(
                'avioflow.homeView',
                new HomeViewProvider(context.extensionUri)
            )
        );
        console.log('[Avioflow] HomeViewProvider registered');

        console.log('[Avioflow] Extension activation completed successfully!');
    } catch (error: any) {
        console.error('[Avioflow] Extension activation failed:', error.message);
        console.error('[Avioflow] Stack:', error.stack);
        vscode.window.showErrorMessage(`Avioflow extension failed to activate: ${error.message}`);
    }
}

export function deactivate() {
    console.log('[Avioflow] Extension deactivated');
}
