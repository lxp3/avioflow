export default {
  content: ['./src/**/*.{html,js,svelte,ts}'],
  theme: {
    extend: {
      colors: {
        vscode: {
          bg: 'var(--vscode-editor-background)',
          fg: 'var(--vscode-editor-foreground)',
          border: 'var(--vscode-panel-border)',
          hover: 'var(--vscode-list-hoverBackground)',
          active: 'var(--vscode-list-activeSelectionBackground)',
        }
      }
    }
  },
  plugins: []
}
