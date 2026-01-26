import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import sveltePreprocess from 'svelte-preprocess';
import path from 'path';

export default defineConfig({
    plugins: [
        svelte({
            preprocess: sveltePreprocess()
        })
    ],
    build: {
        outDir: '../out/webview',
        emptyOutDir: true,
        lib: {
            entry: path.resolve(__dirname, 'src/main.ts'),
            formats: ['iife'],
            name: 'webview',
            fileName: () => 'main.js',
        },
        rollupOptions: {
            output: {
                extend: true,
                assetFileNames: (assetInfo: any) => {
                    if (assetInfo.name === 'style.css') return 'style.css';
                    return assetInfo.name || '[name].[ext]';
                },
            },
        },
    },
    resolve: {
        alias: {
            $src: path.resolve(__dirname, 'src'),
        },
    },
});
