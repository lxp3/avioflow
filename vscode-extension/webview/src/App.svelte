<script lang="ts">
    import { onMount } from "svelte";

    let metadata: any = null;
    let samples: Float32Array[] = [];
    let isPlaying = false;
    let currentTime = 0;
    let duration = 0;
    let volume = 0.5;
    let canvasWidth = 0;
    let canvasHeight = 0;
    let filePath = "";

    let audioContext: AudioContext;
    let audioBuffer: AudioBuffer | null = null;
    let sourceNode: AudioBufferSourceNode | null = null;
    let startTime = 0;
    let pauseOffset = 0;
    let animationFrame: number;

    let canvas: HTMLCanvasElement;

    onMount(() => {
        window.addEventListener("message", (event) => {
            const message = event.data;
            switch (message.type) {
                case "init":
                    filePath = message.filePath || "";
                    metadata = message.metadata;
                    samples = message.samples.map(
                        (ch: any) => new Float32Array(ch),
                    );
                    duration = metadata.duration;
                    initAudio();
                    break;
            }
        });

        // Notify extension we are ready
        // @ts-ignore
        const vscode = acquireVsCodeApi();
        vscode.postMessage({ type: "ready" });

        return () => {
            if (animationFrame) cancelAnimationFrame(animationFrame);
            if (sourceNode) sourceNode.stop();
            if (audioContext) audioContext.close();
        };
    });

    async function initAudio() {
        if (!audioContext) {
            audioContext = new (window.AudioContext ||
                (window as any).webkitAudioContext)();
        }

        // Create AudioBuffer from samples
        const numChannels = samples.length;
        const length = samples[0].length;
        audioBuffer = audioContext.createBuffer(
            numChannels,
            length,
            metadata.sampleRate,
        );

        for (let i = 0; i < numChannels; i++) {
            audioBuffer.copyToChannel(samples[i], i);
        }

        drawWaveform();
    }

    function drawWaveform() {
        if (!canvas || samples.length === 0) return;
        const ctx = canvas.getContext("2d")!;
        const width = canvas.width;
        const height = canvas.height;

        ctx.clearRect(0, 0, width, height);

        const numChannels = samples.length;
        const channelHeight = height / numChannels;
        const halfChannelHeight = channelHeight / 2;

        ctx.lineWidth = 1;

        samples.forEach((data, chIndex) => {
            const yBase = chIndex * channelHeight + halfChannelHeight;
            const step = Math.ceil(data.length / width);

            ctx.beginPath();
            ctx.strokeStyle = "#e0e0e0"; // Light gray for the whole waveform

            for (let i = 0; i < width; i++) {
                let min = 1.0;
                let max = -1.0;
                for (let j = 0; j < step; j++) {
                    const idx = i * step + j;
                    if (idx >= data.length) break;
                    const val = data[idx];
                    if (val < min) min = val;
                    if (val > max) max = val;
                }
                ctx.moveTo(i, yBase + min * halfChannelHeight * 0.9);
                ctx.lineTo(i, yBase + max * halfChannelHeight * 0.9);
            }
            ctx.stroke();

            // Overdraw the played part with blue
            if (currentTime > 0) {
                const playedWidth = (currentTime / duration) * width;
                ctx.beginPath();
                ctx.strokeStyle = "#007aff"; // Apple Blue
                for (let i = 0; i < playedWidth; i++) {
                    let min = 1.0;
                    let max = -1.0;
                    for (let j = 0; j < step; j++) {
                        const idx = i * step + j;
                        if (idx >= data.length) break;
                        const val = data[idx];
                        if (val < min) min = val;
                        if (val > max) max = val;
                    }
                    ctx.moveTo(i, yBase + min * halfChannelHeight * 0.9);
                    ctx.lineTo(i, yBase + max * halfChannelHeight * 0.9);
                }
                ctx.stroke();
            }

            // Draw channel separator
            if (chIndex < numChannels - 1) {
                ctx.beginPath();
                ctx.strokeStyle = "#f0f0f0";
                ctx.moveTo(0, (chIndex + 1) * channelHeight);
                ctx.lineTo(width, (chIndex + 1) * channelHeight);
                ctx.stroke();
            }
        });

        // Draw playhead
        const playheadX = (currentTime / duration) * width;
        ctx.beginPath();
        ctx.strokeStyle = "#ff3b30"; // Apple Red for playhead
        ctx.moveTo(playheadX, 0);
        ctx.lineTo(playheadX, height);
        ctx.stroke();
    }

    function togglePlay() {
        if (isPlaying) {
            pause();
        } else {
            play();
        }
    }

    function play() {
        if (!audioBuffer) return;
        if (audioContext.state === "suspended") {
            audioContext.resume();
        }

        sourceNode = audioContext.createBufferSource();
        sourceNode.buffer = audioBuffer;

        const gainNode = audioContext.createGain();
        gainNode.gain.value = volume;
        sourceNode.connect(gainNode).connect(audioContext.destination);

        sourceNode.start(0, pauseOffset);
        startTime = audioContext.currentTime - pauseOffset;
        isPlaying = true;

        sourceNode.onended = () => {
            if (isPlaying && audioContext.currentTime - startTime >= duration) {
                isPlaying = false;
                pauseOffset = 0;
                currentTime = 0;
                drawWaveform();
            }
        };

        updateTime();
    }

    function pause() {
        if (sourceNode) {
            sourceNode.stop();
            pauseOffset = audioContext.currentTime - startTime;
        }
        isPlaying = false;
        if (animationFrame) cancelAnimationFrame(animationFrame);
    }

    function updateTime() {
        if (!isPlaying) return;
        currentTime = audioContext.currentTime - startTime;
        if (currentTime >= duration) {
            currentTime = duration;
            isPlaying = false;
        } else {
            animationFrame = requestAnimationFrame(updateTime);
        }
        drawWaveform();
    }

    function handleSeek(e: MouseEvent) {
        const rect = canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const ratio = Math.max(0, Math.min(1, x / rect.width));

        const wasPlaying = isPlaying;
        if (isPlaying) pause();

        pauseOffset = ratio * duration;
        currentTime = pauseOffset;
        drawWaveform();

        if (wasPlaying) play();
    }

    function formatTime(s: number) {
        const min = Math.floor(s / 60);
        const sec = Math.floor(s % 60);
        return `${min}:${sec.toString().padStart(2, "0")}`;
    }

    $: if (canvas && (canvasWidth || canvasHeight)) {
        canvas.width = canvas.offsetWidth;
        canvas.height = canvas.offsetHeight;
        drawWaveform();
    }

    $: if (volume !== undefined && sourceNode) {
        // Volume update logic would go here if gainNode was persisted
    }
</script>

<main class="container">
    {#if metadata}
        <div class="header">
            <div class="header-content">
                <svg
                    class="file-icon"
                    viewBox="0 0 24 24"
                    width="20"
                    height="20"
                    ><path
                        fill="currentColor"
                        d="M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"
                    /></svg
                >
                <span class="path">{filePath.split(/[\\/]/).pop()}</span>
                <span class="full-path">{filePath}</span>
            </div>
        </div>

        <div class="metadata-card">
            <div class="metadata-grid">
                <div class="meta-item">
                    <span class="label">Duration</span>
                    <span class="value">{formatTime(duration)}</span>
                </div>
                <div class="meta-item">
                    <span class="label">Sample Rate</span>
                    <span class="value"
                        >{metadata.sampleRate.toLocaleString()} Hz</span
                    >
                </div>
                <div class="meta-item">
                    <span class="label">Channels</span>
                    <span class="value"
                        >{metadata.numChannels === 1
                            ? "Mono"
                            : metadata.numChannels === 2
                              ? "Stereo"
                              : metadata.numChannels + " Channels"}</span
                    >
                </div>
                <div class="meta-item">
                    <span class="label">Codec</span>
                    <span class="value">{metadata.codec.toUpperCase()}</span>
                </div>
                <div class="meta-item">
                    <span class="label">Bitrate</span>
                    <span class="value"
                        >{metadata.bitrate
                            ? (metadata.bitrate / 1000).toFixed(0) + " kbps"
                            : "Variable"}</span
                    >
                </div>
                <div class="meta-item">
                    <span class="label">Format</span>
                    <span class="value">{metadata.formatName}</span>
                </div>
            </div>
        </div>

        <div class="waveform-section">
            <div
                class="waveform-container"
                bind:clientWidth={canvasWidth}
                bind:clientHeight={canvasHeight}
            >
                <canvas bind:this={canvas} on:click={handleSeek}></canvas>
            </div>
        </div>

        <div class="controls-card">
            <div class="controls">
                <button
                    class="play-btn"
                    on:click={togglePlay}
                    aria-label={isPlaying ? "Pause" : "Play"}
                >
                    {#if isPlaying}
                        <svg viewBox="0 0 24 24" width="24" height="24"
                            ><path
                                fill="currentColor"
                                d="M6 19h4V5H6v14zm8-14v14h4V5h-4z"
                            /></svg
                        >
                    {:else}
                        <svg viewBox="0 0 24 24" width="24" height="24"
                            ><path fill="currentColor" d="M8 5v14l11-7z" /></svg
                        >
                    {/if}
                </button>

                <div class="time-container">
                    <span class="current-time">{formatTime(currentTime)}</span>
                    <span class="separator">/</span>
                    <span class="duration-time">{formatTime(duration)}</span>
                </div>

                <div class="volume-container">
                    <svg
                        class="volume-icon"
                        viewBox="0 0 24 24"
                        width="18"
                        height="18"
                        ><path
                            fill="currentColor"
                            d="M3 9v6h4l5 5V4L7 9H3zm13.5 3c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02zM14 3.23v2.06c2.89.86 5 3.54 5 6.71s-2.11 5.85-5 6.71v2.06c4.01-.91 7-4.49 7-8.77s-2.99-7.86-7-8.77z"
                        /></svg
                    >
                    <input
                        type="range"
                        min="0"
                        max="1"
                        step="0.01"
                        bind:value={volume}
                    />
                </div>
            </div>
        </div>
    {:else}
        <div class="loading">
            <div class="spinner"></div>
            <span>Decoding audio...</span>
        </div>
    {/if}
</main>

<style>
    :global(body) {
        margin: 0;
        padding: 0;
        font-family: -apple-system, BlinkMacSystemFont, "SF Pro Text",
            "Helvetica Neue", Arial, sans-serif;
        background-color: #f5f5f7;
        color: #1d1d1f;
        overflow: hidden;
        -webkit-font-smoothing: antialiased;
    }

    .container {
        display: flex;
        flex-direction: column;
        height: 100vh;
        padding: 24px;
        box-sizing: border-box;
        gap: 20px;
    }

    .header {
        margin-bottom: 4px;
    }

    .header-content {
        display: flex;
        align-items: center;
        gap: 10px;
    }

    .file-icon {
        color: #007aff;
    }

    .path {
        font-size: 18px;
        font-weight: 600;
        color: #1d1d1f;
    }

    .full-path {
        font-size: 12px;
        color: #86868b;
        margin-left: 8px;
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
    }

    .metadata-card {
        background: rgba(255, 255, 255, 0.8);
        backdrop-filter: blur(20px);
        -webkit-backdrop-filter: blur(20px);
        border-radius: 12px;
        padding: 16px 20px;
        border: 1px solid rgba(0, 0, 0, 0.05);
        box-shadow: 0 4px 12px rgba(0, 0, 0, 0.03);
    }

    .metadata-grid {
        display: grid;
        grid-template-columns: repeat(3, 1fr);
        gap: 16px 32px;
    }

    .meta-item {
        display: flex;
        flex-direction: column;
        gap: 4px;
    }

    .label {
        font-size: 11px;
        font-weight: 600;
        text-transform: uppercase;
        letter-spacing: 0.05em;
        color: #86868b;
    }

    .value {
        font-size: 14px;
        font-weight: 500;
        color: #1d1d1f;
    }

    .waveform-section {
        flex: 1;
        display: flex;
        min-height: 0;
    }

    .waveform-container {
        flex: 1;
        position: relative;
        background: #ffffff;
        border: 1px solid rgba(0, 0, 0, 0.08);
        border-radius: 12px;
        cursor: pointer;
        overflow: hidden;
        box-shadow: inset 0 1px 3px rgba(0, 0, 0, 0.02);
    }

    canvas {
        width: 100%;
        height: 100%;
        display: block;
    }

    .controls-card {
        background: rgba(255, 255, 255, 0.8);
        backdrop-filter: blur(20px);
        -webkit-backdrop-filter: blur(20px);
        border-radius: 12px;
        padding: 12px 20px;
        border: 1px solid rgba(0, 0, 0, 0.05);
        box-shadow: 0 4px 16px rgba(0, 0, 0, 0.05);
    }

    .controls {
        display: flex;
        align-items: center;
        gap: 24px;
    }

    .play-btn {
        background: #007aff;
        color: white;
        border: none;
        width: 48px;
        height: 48px;
        border-radius: 50%;
        display: flex;
        align-items: center;
        justify-content: center;
        cursor: pointer;
        transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
        box-shadow: 0 4px 12px rgba(0, 122, 255, 0.3);
    }

    .play-btn:hover {
        background: #0063d1;
        transform: translateY(-1px);
        box-shadow: 0 6px 16px rgba(0, 122, 255, 0.4);
    }

    .play-btn:active {
        transform: scale(0.96);
    }

    .time-container {
        display: flex;
        align-items: center;
        gap: 6px;
        font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas,
            monospace;
        font-size: 14px;
        letter-spacing: -0.02em;
        font-weight: 500;
        color: #1d1d1f;
        min-width: 130px;
    }

    .separator {
        color: #c1c1c6;
    }

    .duration-time {
        color: #86868b;
    }

    .volume-container {
        display: flex;
        align-items: center;
        gap: 12px;
        margin-left: auto;
        color: #86868b;
    }

    .volume-icon {
        transition: color 0.2s;
    }

    .volume-container:hover .volume-icon {
        color: #007aff;
    }

    input[type="range"] {
        -webkit-appearance: none;
        width: 120px;
        height: 4px;
        background: #e5e5ea;
        border-radius: 2px;
        outline: none;
        cursor: pointer;
    }

    input[type="range"]::-webkit-slider-thumb {
        -webkit-appearance: none;
        width: 16px;
        height: 16px;
        background: #ffffff;
        border: 0.5px solid rgba(0, 0, 0, 0.1);
        border-radius: 50%;
        box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
        transition: transform 0.1s;
    }

    input[type="range"]:active::-webkit-slider-thumb {
        transform: scale(1.1);
    }

    .loading {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        height: 100%;
        gap: 16px;
        color: #86868b;
        font-size: 15px;
        font-weight: 500;
    }

    .spinner {
        width: 24px;
        height: 24px;
        border: 3px solid #e5e5ea;
        border-top: 3px solid #007aff;
        border-radius: 50%;
        animation: spin 1s linear infinite;
    }

    @keyframes spin {
        0% {
            transform: rotate(0deg);
        }
        100% {
            transform: rotate(360deg);
        }
    }
</style>
