import * as vscode from 'vscode';

export function activate(context: vscode.ExtensionContext) {
    console.log('Avioflow Audio Previewer is now active!');

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

    // Register providers
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
}

export function deactivate() { }
