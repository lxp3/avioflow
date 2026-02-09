<script lang="ts">
    import { createEventDispatcher } from 'svelte';

    const dispatch = createEventDispatcher();

    let isDragging = false;

    function handleClick() {
        // Trigger file selection via VS Code dialog
        dispatch('select');
    }

    function handleDragOver(e: DragEvent) {
        e.preventDefault();
        isDragging = true;
    }

    function handleDragLeave(e: DragEvent) {
        e.preventDefault();
        isDragging = false;
    }

    function handleDrop(e: DragEvent) {
        e.preventDefault();
        isDragging = false;
        // In VS Code webview, we can't access local file paths from dropped files
        // So we trigger the file dialog instead
        dispatch('select');
    }
</script>

<div class="file-upload-wrapper">
    <div
        class="dropzone"
        class:dragging={isDragging}
        on:drop={handleDrop}
        on:dragover={handleDragOver}
        on:dragleave={handleDragLeave}
        on:click={handleClick}
        role="button"
        tabindex="0"
        on:keydown={(e) => e.key === 'Enter' && handleClick()}
    >
        <div class="upload-icon">
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/>
                <polyline points="17 8 12 3 7 8"/>
                <line x1="12" y1="3" x2="12" y2="15"/>
            </svg>
        </div>

        <p class="upload-title">拖拽文件到此处或点击上传</p>
        <p class="upload-subtitle">支持 WAV, MP3, FLAC 等音频格式</p>
    </div>
</div>

<style>
    .file-upload-wrapper {
        display: flex;
        flex-direction: column;
        padding: 8px;
        box-sizing: border-box;
        max-width: 280px;
        max-height: 180px;
    }

    .dropzone {
        flex: 1;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        border: 2px dashed #d1d5db;
        border-radius: 12px;
        padding: 16px;
        text-align: center;
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
        width: 48px;
        height: 48px;
        border-radius: 12px;
        display: flex;
        align-items: center;
        justify-content: center;
        background: linear-gradient(135deg, #3b82f6, #06b6d4);
        color: white;
        margin-bottom: 12px;
        box-shadow: 0 4px 12px rgba(59, 130, 246, 0.3);
        transition: transform 0.2s ease;
    }

    .dropzone:hover .upload-icon {
        transform: scale(1.05);
    }

    .upload-title {
        font-size: 13px;
        font-weight: 600;
        color: #374151;
        margin: 0 0 4px 0;
    }

    .upload-subtitle {
        font-size: 11px;
        color: #6b7280;
        margin: 0;
    }
</style>
