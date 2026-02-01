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
    let waveformSummary: { min: Float32Array; max: Float32Array }[] = [];

    onMount(() => {
        window.addEventListener("message", (event) => {
            const message = event.data;
            switch (message.type) {
                case "init":
                    filePath = message.filePath || "";
                    metadata = message.metadata;
                    samples = message.samples.map((ch: any) =>
                        ch instanceof Float32Array ? ch : new Float32Array(ch),
                    );
                    duration = metadata.duration;
                    initAudio();
                    break;
            }
        });

        const handleKeyDown = (e: KeyboardEvent) => {
            if (e.key === " " || e.code === "Space") {
                e.preventDefault();
                togglePlay();
            }
        };

        window.addEventListener("keydown", handleKeyDown);

        // Notify extension we are ready
        // @ts-ignore
        const vscode = acquireVsCodeApi();
        vscode.postMessage({ type: "ready" });

        return () => {
            window.removeEventListener("keydown", handleKeyDown);
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

        calculateSummary();
        drawWaveform();
    }

    function calculateSummary() {
        if (samples.length === 0 || canvasWidth === 0) return;

        const width = Math.ceil(canvasWidth);
        waveformSummary = samples.map((data) => {
            const minArr = new Float32Array(width);
            const maxArr = new Float32Array(width);
            const step = data.length / width;

            for (let i = 0; i < width; i++) {
                let min = 1.0;
                let max = -1.0;
                const startIdx = Math.floor(i * step);
                const endIdx = Math.floor((i + 1) * step);

                for (let j = startIdx; j < endIdx && j < data.length; j++) {
                    const val = data[j];
                    if (val < min) min = val;
                    if (val > max) max = val;
                }
                minArr[i] = min;
                maxArr[i] = max;
            }
            return { min: minArr, max: maxArr };
        });
    }

    function drawTimeline(
        ctx: CanvasRenderingContext2D,
        width: number,
        height: number,
    ) {
        const numTicks = 10;
        const tickSpacing = width / numTicks;

        ctx.save();
        ctx.beginPath();
        ctx.strokeStyle = "rgba(0,0,0,0.1)";
        ctx.lineWidth = 1;
        ctx.font = "10px Inter, -apple-system, sans-serif";
        ctx.fillStyle = "#86868b";
        ctx.textAlign = "center";

        for (let i = 0; i <= numTicks; i++) {
            const x = i * tickSpacing + 0.5;
            const time = (i / numTicks) * duration;

            // Vertical grid line
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);

            // Time label
            if (i > 0 && i < numTicks) {
                ctx.fillText(formatTime(time), x, 12);
            }
        }
        ctx.stroke();
        ctx.restore();
    }

    function drawWaveform() {
        if (!canvas || waveformSummary.length === 0) return;

        const dpr = window.devicePixelRatio || 1;
        const displayWidth = canvas.offsetWidth;
        const displayHeight = canvas.offsetHeight;

        if (
            canvas.width !== displayWidth * dpr ||
            canvas.height !== displayHeight * dpr
        ) {
            canvas.width = displayWidth * dpr;
            canvas.height = displayHeight * dpr;
        }

        const ctx = canvas.getContext("2d")!;
        ctx.save();
        ctx.scale(dpr, dpr);

        const width = displayWidth;
        const height = displayHeight;
        ctx.clearRect(0, 0, width, height);

        // 0. Draw Timeline/Ruler
        drawTimeline(ctx, width, height);

        const numChannels = waveformSummary.length;
        const channelHeight = height / numChannels;
        const halfChannelHeight = channelHeight / 2;
        const drawHeight = halfChannelHeight * 0.92;

        waveformSummary.forEach((summary, chIndex) => {
            const yBase = chIndex * channelHeight + halfChannelHeight;
            const playedWidth = Math.floor((currentTime / duration) * width);

            // 1. Draw unplayed part (background)
            ctx.beginPath();
            ctx.strokeStyle = "#d1d1d6";
            ctx.lineWidth = 1;
            for (let i = playedWidth; i < width; i++) {
                const x = i + 0.5;
                ctx.moveTo(x, yBase + summary.min[i] * drawHeight);
                ctx.lineTo(x, yBase + summary.max[i] * drawHeight);
            }
            ctx.stroke();

            // 2. Draw played part (foreground)
            if (playedWidth > 0) {
                ctx.beginPath();
                ctx.strokeStyle = "#007aff";
                ctx.lineWidth = 1;
                for (let i = 0; i < playedWidth; i++) {
                    const x = i + 0.5;
                    ctx.moveTo(x, yBase + summary.min[i] * drawHeight);
                    ctx.lineTo(x, yBase + summary.max[i] * drawHeight);
                }
                ctx.stroke();
            }

            // 3. Channel separator
            if (chIndex < numChannels - 1) {
                ctx.beginPath();
                ctx.strokeStyle = "rgba(0,0,0,0.05)";
                ctx.moveTo(0, (chIndex + 1) * channelHeight);
                ctx.lineTo(width, (chIndex + 1) * channelHeight);
                ctx.stroke();
            }
        });

        // 4. Playhead
        const playheadX = (currentTime / duration) * width + 0.5;
        ctx.beginPath();
        ctx.strokeStyle = "#ff3b30";
        ctx.lineWidth = 1.5;
        ctx.moveTo(playheadX, 0);
        ctx.lineTo(playheadX, height);
        ctx.stroke();

        ctx.restore();
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

        if (isPlaying) pause();

        pauseOffset = ratio * duration;
        currentTime = pauseOffset;
        drawWaveform();

        play();
    }

    function formatTime(s: number) {
        if (!s || s === 0) return "0.000s";
        const hrs = Math.floor(s / 3600);
        const min = Math.floor((s % 3600) / 60);
        const sec = s % 60;

        let result = "";
        if (hrs > 0) result += `${hrs}h `;
        if (min > 0 || hrs > 0) result += `${min}m `;
        result += `${sec.toFixed(3)}s`;
        return result;
    }

    function formatSize(bytes: number) {
        if (bytes === 0) return "0 B";
        const k = 1024;
        const sizes = ["B", "KB", "MB", "GB"];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + " " + sizes[i];
    }

    function copyToClipboard(text: string) {
        navigator.clipboard.writeText(text);
        // Maybe add a temporary toast or tooltip here
    }

    $: if (canvas && (canvasWidth || canvasHeight)) {
        calculateSummary();
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
                <div class="path-row">
                    <span class="full-path-title">File Path:</span>
                    <code class="full-path-text">{filePath}</code>
                    <button
                        class="copy-btn"
                        on:click={() => copyToClipboard(filePath)}
                        title="Copy Path"
                    >
                        <svg viewBox="0 0 24 24" width="14" height="14"
                            ><path
                                fill="currentColor"
                                d="M16 1H4c-1.1 0-2 .9-2 2v14h2V3h12V1zm3 4H8c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h11c1.1 0 2-.9 2-2V7c0-1.1-.9-2-2-2zm0 16H8V7h11v14z"
                            /></svg
                        >
                    </button>
                </div>
                <div class="title-row">
                    <svg
                        class="file-icon"
                        viewBox="0 0 24 24"
                        width="24"
                        height="24"
                        ><path
                            fill="currentColor"
                            d="M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"
                        /></svg
                    >
                    <span class="filename">{filePath.split(/[\\/]/).pop()}</span
                    >
                </div>
            </div>
        </div>

        <div class="metadata-card">
            <div class="metadata-grid">
                <!-- Row 1: Size, Format, Codec -->
                <div class="meta-item">
                    <span class="label">size</span>
                    <span class="value">{formatSize(metadata.fileSize)}</span>
                </div>
                <div class="meta-item">
                    <span class="label">format</span>
                    <span class="value">{metadata.container}</span>
                </div>
                <div class="meta-item">
                    <span class="label">codec</span>
                    <span class="value">{metadata.codec.toLowerCase()}</span>
                </div>
                <div class="meta-item empty-fill"></div>

                <!-- Row 2: Duration, Sample Rate, Channels, Bitrate -->
                <div class="meta-item">
                    <span class="label">duration</span>
                    <span class="value">{formatTime(duration)}</span>
                </div>
                <div class="meta-item">
                    <span class="label">sample rate</span>
                    <span class="value"
                        >{metadata.sampleRate.toLocaleString()} Hz</span
                    >
                </div>
                <div class="meta-item">
                    <span class="label">channels</span>
                    <span class="value">{metadata.numChannels}</span>
                </div>
                <div class="meta-item">
                    <span class="label">bitrate</span>
                    <span class="value"
                        >{metadata.bitRate
                            ? (metadata.bitRate / 1000).toFixed(0) + " kbps"
                            : "Variable"}</span
                    >
                </div>
            </div>
        </div>

        <div class="waveform-section">
            <div
                class="waveform-container"
                bind:clientWidth={canvasWidth}
                style="height: {metadata.numChannels * 160}px"
            >
                <canvas bind:this={canvas} on:click={handleSeek}></canvas>
                <div class="channel-overlay">
                    {#each Array(metadata.numChannels) as _, i}
                        <div
                            class="channel-label"
                            style="height: {100 / metadata.numChannels}%"
                        >
                            <span>{i}</span>
                        </div>
                    {/each}
                </div>
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
        margin-bottom: 8px;
    }

    .header-content {
        display: flex;
        flex-direction: column;
        gap: 8px;
    }

    .path-row {
        display: flex;
        align-items: center;
        gap: 8px;
        background: transparent;
        padding: 0;
        border: none;
        min-width: 0;
    }

    .full-path-title {
        font-size: 11px;
        font-weight: 700;
        color: #86868b;
    }

    .full-path-text {
        font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas,
            monospace;
        font-size: 12px;
        color: #1d1d1f;
        flex: 1;
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
    }

    .copy-btn {
        background: transparent;
        border: none;
        color: #007aff;
        cursor: pointer;
        padding: 4px;
        display: flex;
        align-items: center;
        justify-content: center;
        border-radius: 4px;
        transition: background 0.2s;
    }

    .copy-btn:hover {
        background: rgba(0, 122, 255, 0.1);
    }

    .title-row {
        display: flex;
        align-items: center;
        gap: 10px;
    }

    .filename {
        font-size: 20px;
        font-weight: 600;
        color: #1d1d1f;
    }

    .file-icon {
        color: #007aff;
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
        grid-template-columns: repeat(4, auto);
        gap: 8px 32px;
        justify-content: start;
    }

    .meta-item {
        display: flex;
        flex-direction: row;
        align-items: center;
        padding: 2px 0;
    }

    .meta-item:last-child {
        border-bottom: none;
    }

    .label {
        font-size: 10px;
        font-weight: 700;
        letter-spacing: 0.02em;
        color: #86868b;
        margin-right: 8px;
        white-space: nowrap;
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
        overflow-y: auto;
    }

    .waveform-container {
        width: 100%;
        position: relative;
        background: #ffffff;
        border: 1px solid rgba(0, 0, 0, 0.05);
        border-radius: 12px;
        cursor: pointer;
        overflow: hidden;
        box-shadow: inset 0 1px 3px rgba(0, 0, 0, 0.02);
    }

    .channel-overlay {
        position: absolute;
        top: 0;
        left: 0;
        bottom: 0;
        width: 24px;
        pointer-events: none;
        display: flex;
        flex-direction: column;
    }

    .channel-label {
        display: flex;
        align-items: center;
        justify-content: center;
        font-size: 10px;
        font-weight: 700;
        color: #86868b;
        background: rgba(255, 255, 255, 0.7);
        border-right: 1px solid rgba(0, 0, 0, 0.05);
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
