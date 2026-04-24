<script lang="ts">
    import { onMount } from "svelte";
    import "./globals.css";
    import FileUpload from "./components/FileUpload.svelte";

    // State
    let metadata: any = null;
    let samples: Float32Array[] = [];
    let filePath = "";
    let decodeTimeMs = 0;
    let totalTimeMs = 0;
    let isLoadingSamples = false;
    let loadStartTime = 0;
    let loadError: { title: string; message: string; code?: string } | null = null;

    // Audio playback
    let audioContext: AudioContext;
    let audioBuffer: AudioBuffer | null = null;
    let sourceNode: AudioBufferSourceNode | null = null;
    let isPlaying = false;
    let currentTime = 0;
    let duration = 0;
    let startTime = 0;
    let pauseOffset = 0;
    let animationFrame: number;

    // VS Code API
    // @ts-ignore
    const vscode = typeof acquireVsCodeApi !== 'undefined' ? acquireVsCodeApi() : null;

    // Canvas & Zoom
    let canvas: HTMLCanvasElement;
    let canvasContainer: HTMLDivElement;
    const CHANNEL_HEIGHT = 128;
    let zoom = 1;
    const MIN_ZOOM = 1;
    const MAX_ZOOM = 50;

    onMount(() => {
        window.addEventListener("message", handleMessage);
        window.addEventListener("keydown", handleKeyDown);

        if (vscode) {
            vscode.postMessage({ type: "ready" });
        }
        loadStartTime = Date.now();

        return () => {
            window.removeEventListener("message", handleMessage);
            window.removeEventListener("keydown", handleKeyDown);
            if (animationFrame) cancelAnimationFrame(animationFrame);
            if (sourceNode) sourceNode.stop();
            if (audioContext) audioContext.close();
        };
    });

    function handleKeyDown(e: KeyboardEvent) {
        if ((e.key === " " || e.code === "Space") && samples.length > 0) {
            e.preventDefault();
            togglePlay();
        }
    }

    function handleMessage(event: MessageEvent) {
        const message = event.data;

        switch (message.type) {
            case "loading":
                filePath = message.filePath || "";
                metadata = null;
                samples = [];
                audioBuffer = null;
                decodeTimeMs = 0;
                totalTimeMs = 0;
                currentTime = 0;
                duration = 0;
                pauseOffset = 0;
                loadError = null;
                isLoadingSamples = true;
                break;

            case "metadata":
                loadError = null;
                filePath = message.filePath || "";
                metadata = message.metadata;
                duration = metadata.duration || 0;
                isLoadingSamples = true;
                break;

            case "samples":
                isLoadingSamples = false;
                decodeTimeMs = message.decodeTimeMs || 0;
                totalTimeMs = Date.now() - loadStartTime;
                loadError = null;

                if (message.samples && message.samples.length > 0) {
                    samples = message.samples.map((ch: any) =>
                        ch instanceof Float32Array ? ch : new Float32Array(ch)
                    );
                }

                initAudio();
                break;

            case "error":
                isLoadingSamples = false;
                metadata = null;
                samples = [];
                audioBuffer = null;
                decodeTimeMs = 0;
                totalTimeMs = 0;
                loadError = {
                    title: message.title || "Failed to load audio",
                    message: message.message || "Unknown error",
                    code: message.code
                };
                console.error("Error:", loadError);
                break;
        }
    }

    function initAudio() {
        if (!audioContext) {
            audioContext = new (window.AudioContext || (window as any).webkitAudioContext)();
        }

        if (samples.length > 0 && metadata) {
            const numChannels = samples.length;
            const length = samples[0].length;
            audioBuffer = audioContext.createBuffer(numChannels, length, metadata.sampleRate);

            for (let i = 0; i < numChannels; i++) {
                audioBuffer.copyToChannel(samples[i], i);
            }
        }

        setTimeout(() => {
            drawWaveform();
        }, 50);
    }

    function drawWaveform() {
        if (!canvas || samples.length === 0) return;

        const containerRect = canvasContainer.getBoundingClientRect();
        const containerWidth = containerRect.width;

        if (containerWidth === 0) {
            setTimeout(drawWaveform, 50);
            return;
        }

        const numChannels = samples.length;
        const totalHeight = numChannels * CHANNEL_HEIGHT;
        const totalWidth = containerWidth * zoom;

        const dpr = window.devicePixelRatio || 1;
        canvas.width = totalWidth * dpr;
        canvas.height = totalHeight * dpr;
        canvas.style.width = totalWidth + "px";
        canvas.style.height = totalHeight + "px";

        const ctx = canvas.getContext("2d")!;
        ctx.scale(dpr, dpr);
        ctx.clearRect(0, 0, totalWidth, totalHeight);

        // Draw each channel
        for (let ch = 0; ch < numChannels; ch++) {
            const data = samples[ch];
            const yOffset = ch * CHANNEL_HEIGHT;
            const centerY = yOffset + CHANNEL_HEIGHT / 2;
            const amplitude = CHANNEL_HEIGHT / 2 * 0.85;

            // Draw zero line (y=0 axis)
            ctx.beginPath();
            ctx.strokeStyle = "#e0e0e0";
            ctx.lineWidth = 1;
            ctx.setLineDash([4, 4]);
            ctx.moveTo(0, centerY);
            ctx.lineTo(totalWidth, centerY);
            ctx.stroke();
            ctx.setLineDash([]);

            // Calculate min/max for each pixel
            const step = data.length / totalWidth;
            const playedX = (currentTime / duration) * totalWidth;

            // Unplayed part (gray)
            ctx.beginPath();
            ctx.strokeStyle = "#b0b0b0";
            ctx.lineWidth = 1;

            for (let x = Math.floor(playedX); x < totalWidth; x++) {
                const startIdx = Math.floor(x * step);
                const endIdx = Math.floor((x + 1) * step);
                let min = 0, max = 0;
                for (let i = startIdx; i < endIdx && i < data.length; i++) {
                    if (data[i] < min) min = data[i];
                    if (data[i] > max) max = data[i];
                }
                ctx.moveTo(x + 0.5, centerY - max * amplitude);
                ctx.lineTo(x + 0.5, centerY - min * amplitude);
            }
            ctx.stroke();

            // Played part (blue)
            if (playedX > 0) {
                ctx.beginPath();
                ctx.strokeStyle = "#3b82f6";
                ctx.lineWidth = 1;
                for (let x = 0; x < playedX && x < totalWidth; x++) {
                    const startIdx = Math.floor(x * step);
                    const endIdx = Math.floor((x + 1) * step);
                    let min = 0, max = 0;
                    for (let i = startIdx; i < endIdx && i < data.length; i++) {
                        if (data[i] < min) min = data[i];
                        if (data[i] > max) max = data[i];
                    }
                    ctx.moveTo(x + 0.5, centerY - max * amplitude);
                    ctx.lineTo(x + 0.5, centerY - min * amplitude);
                }
                ctx.stroke();
            }

            // Channel separator
            if (ch < numChannels - 1) {
                ctx.beginPath();
                ctx.strokeStyle = "#d0d0d0";
                ctx.lineWidth = 1;
                ctx.moveTo(0, (ch + 1) * CHANNEL_HEIGHT);
                ctx.lineTo(totalWidth, (ch + 1) * CHANNEL_HEIGHT);
                ctx.stroke();
            }

            // Channel label
            ctx.fillStyle = "#666666";
            ctx.font = "11px sans-serif";
            ctx.fillText(`CH ${ch}`, 8, yOffset + 16);
        }

        // Draw time axis
        drawTimeAxis(ctx, totalWidth, totalHeight);

        // Draw playhead
        const playheadX = (currentTime / duration) * totalWidth;
        ctx.beginPath();
        ctx.strokeStyle = "#ef4444";
        ctx.lineWidth = 2;
        ctx.moveTo(playheadX, 0);
        ctx.lineTo(playheadX, totalHeight);
        ctx.stroke();
    }

    function drawTimeAxis(ctx: CanvasRenderingContext2D, width: number, height: number) {
        const numTicks = Math.max(10, Math.floor(width / 100));

        ctx.fillStyle = "#666666";
        ctx.font = "10px sans-serif";

        for (let i = 0; i <= numTicks; i++) {
            const x = (i / numTicks) * width;
            const time = (i / numTicks) * duration;

            // Tick mark at top
            ctx.beginPath();
            ctx.strokeStyle = "#cccccc";
            ctx.lineWidth = 1;
            ctx.moveTo(x, 0);
            ctx.lineTo(x, 8);
            ctx.stroke();

            // Tick mark at bottom
            ctx.beginPath();
            ctx.moveTo(x, height - 20);
            ctx.lineTo(x, height);
            ctx.stroke();

            // Time label at bottom
            if (i === 0) {
                ctx.textAlign = "left";
                ctx.fillText(formatTimeShort(time), x + 2, height - 6);
            } else if (i === numTicks) {
                ctx.textAlign = "right";
                ctx.fillText(formatTimeShort(time), x - 2, height - 6);
            } else {
                ctx.textAlign = "center";
                ctx.fillText(formatTimeShort(time), x, height - 6);
            }
        }
    }

    function formatTimeShort(s: number): string {
        if (!s || isNaN(s)) return "0:00";
        const min = Math.floor(s / 60);
        const sec = Math.floor(s % 60);
        const ms = Math.floor((s % 1) * 10);
        if (zoom > 5) {
            return `${min}:${sec.toString().padStart(2, "0")}.${ms}`;
        }
        return `${min}:${sec.toString().padStart(2, "0")}`;
    }

    function handleCanvasClick(e: MouseEvent) {
        if (!canvas || duration === 0 || samples.length === 0) return;

        const containerRect = canvasContainer.getBoundingClientRect();
        const clickXInContainer = e.clientX - containerRect.left;
        const absoluteX = clickXInContainer + canvasContainer.scrollLeft;
        const containerWidth = canvasContainer.offsetWidth;
        const totalWidth = containerWidth * zoom;
        const ratio = Math.max(0, Math.min(1, absoluteX / totalWidth));

        if (isPlaying) pause();
        pauseOffset = ratio * duration;
        currentTime = pauseOffset;
        drawWaveform();
        play();
    }

    function handleWheel(e: WheelEvent) {
        e.preventDefault();

        const delta = e.deltaY > 0 ? 0.9 : 1.1;
        const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, zoom * delta));

        if (newZoom !== zoom) {
            const rect = canvasContainer.getBoundingClientRect();
            const mouseX = e.clientX - rect.left;
            const scrollBefore = canvasContainer.scrollLeft;
            const positionRatio = (scrollBefore + mouseX) / (rect.width * zoom);

            zoom = newZoom;

            setTimeout(() => {
                drawWaveform();
                const newScroll = positionRatio * rect.width * zoom - mouseX;
                canvasContainer.scrollLeft = Math.max(0, newScroll);
            }, 0);
        }
    }

    function zoomIn() {
        zoom = Math.min(MAX_ZOOM, zoom * 1.5);
        drawWaveform();
    }

    function zoomOut() {
        zoom = Math.max(MIN_ZOOM, zoom / 1.5);
        drawWaveform();
    }

    function resetZoom() {
        zoom = 1;
        canvasContainer.scrollLeft = 0;
        drawWaveform();
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

        if (sourceNode) {
            sourceNode.stop();
        }

        sourceNode = audioContext.createBufferSource();
        sourceNode.buffer = audioBuffer;
        sourceNode.connect(audioContext.destination);
        sourceNode.start(0, pauseOffset);
        startTime = audioContext.currentTime - pauseOffset;
        isPlaying = true;

        sourceNode.onended = () => {
            if (isPlaying && currentTime >= duration) {
                isPlaying = false;
                pauseOffset = 0;
                currentTime = 0;
                drawWaveform();
            }
        };

        updatePlayback();
    }

    function pause() {
        if (sourceNode) {
            sourceNode.stop();
            pauseOffset = audioContext.currentTime - startTime;
        }
        isPlaying = false;
        if (animationFrame) cancelAnimationFrame(animationFrame);
    }

    function updatePlayback() {
        if (!isPlaying) return;
        currentTime = audioContext.currentTime - startTime;
        if (currentTime >= duration) {
            currentTime = duration;
            isPlaying = false;
        } else {
            animationFrame = requestAnimationFrame(updatePlayback);
        }
        drawWaveform();

        if (zoom > 1) {
            const containerWidth = canvasContainer.offsetWidth;
            const totalWidth = containerWidth * zoom;
            const playheadX = (currentTime / duration) * totalWidth;
            const scrollLeft = canvasContainer.scrollLeft;

            if (playheadX < scrollLeft || playheadX > scrollLeft + containerWidth - 50) {
                canvasContainer.scrollLeft = Math.max(0, playheadX - 100);
            }
        }
    }

    function formatTime(s: number): string {
        if (!s || isNaN(s)) return "0:00";
        const min = Math.floor(s / 60);
        const sec = Math.floor(s % 60);
        return `${min}:${sec.toString().padStart(2, "0")}`;
    }

    function formatSize(bytes: number): string {
        if (!bytes || isNaN(bytes)) return "-";
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
        return (bytes / (1024 * 1024)).toFixed(2) + " MB";
    }

    function getFileName(path: string): string {
        return path.split(/[\\/]/).pop() || path;
    }

    function handleFileUpload() {
        if (vscode) {
            loadStartTime = Date.now();
            loadError = null;
            vscode.postMessage({ type: "selectFile" });
        }
    }

    $: canvasHeight = samples.length > 0 ? samples.length * CHANNEL_HEIGHT : 120;
</script>

<main class="flex flex-col h-full w-full bg-white p-4 gap-3">
    {#if metadata || isLoadingSamples}
        <!-- Line 1: File Path -->
        <div class="text-sm text-gray-600 truncate">
            {filePath || "Loading audio..."}
        </div>

        <!-- Line 2: Timing Info -->
        <div class="flex gap-4 text-xs text-gray-500">
            <span>Total: <span class="text-gray-700 font-medium">{totalTimeMs} ms</span></span>
            <span>Decode: <span class="text-gray-700 font-medium">{decodeTimeMs} ms</span></span>
            <span>Playback: <span class="text-gray-700 font-medium">{formatTime(currentTime)} / {formatTime(duration)}</span></span>
        </div>

        {#if metadata}
            <!-- Metadata Table -->
            <table class="text-sm border-collapse w-full max-w-lg">
                <tbody>
                    <tr class="border-b border-gray-100">
                        <td class="py-1 pr-4 text-gray-500">File</td>
                        <td class="py-1 text-gray-800 font-medium">{getFileName(filePath)}</td>
                        <td class="py-1 pr-4 text-gray-500 pl-6">Size</td>
                        <td class="py-1 text-gray-800">{formatSize(metadata.fileSize)}</td>
                    </tr>
                    <tr class="border-b border-gray-100">
                        <td class="py-1 pr-4 text-gray-500">Format</td>
                        <td class="py-1 text-gray-800">{metadata.container || "-"}</td>
                        <td class="py-1 pr-4 text-gray-500 pl-6">Codec</td>
                        <td class="py-1 text-gray-800">{metadata.codec || "-"}</td>
                    </tr>
                    <tr class="border-b border-gray-100">
                        <td class="py-1 pr-4 text-gray-500">Duration</td>
                        <td class="py-1 text-gray-800">{formatTime(duration)}</td>
                        <td class="py-1 pr-4 text-gray-500 pl-6">Sample Rate</td>
                        <td class="py-1 text-gray-800">{metadata.sampleRate?.toLocaleString() || "-"} Hz</td>
                    </tr>
                    <tr>
                        <td class="py-1 pr-4 text-gray-500">Channels</td>
                        <td class="py-1 text-gray-800">{metadata.numChannels || "-"}</td>
                        <td class="py-1 pr-4 text-gray-500 pl-6">Bitrate</td>
                        <td class="py-1 text-gray-800">{metadata.bitRate ? Math.round(metadata.bitRate / 1000) + " kbps" : "-"}</td>
                    </tr>
                </tbody>
            </table>
        {/if}

        <!-- Waveform Section -->
        <div class="flex-1 flex flex-col gap-2 min-h-0">
            <!-- Zoom Controls -->
            <div class="flex items-center gap-2">
                <span class="text-xs text-gray-500">Zoom:</span>
                <button
                    on:click={zoomOut}
                    disabled={zoom <= MIN_ZOOM}
                    class="px-2 py-1 text-xs bg-gray-100 hover:bg-gray-200 disabled:opacity-50 disabled:cursor-not-allowed rounded border border-gray-300"
                >−</button>
                <span class="text-xs text-gray-600 w-12 text-center">{zoom.toFixed(1)}x</span>
                <button
                    on:click={zoomIn}
                    disabled={zoom >= MAX_ZOOM}
                    class="px-2 py-1 text-xs bg-gray-100 hover:bg-gray-200 disabled:opacity-50 disabled:cursor-not-allowed rounded border border-gray-300"
                >+</button>
                <button
                    on:click={resetZoom}
                    disabled={zoom === 1}
                    class="px-2 py-1 text-xs bg-gray-100 hover:bg-gray-200 disabled:opacity-50 disabled:cursor-not-allowed rounded border border-gray-300"
                >Reset</button>
                <span class="text-xs text-gray-400 ml-2">(Scroll to zoom, Space to play/pause)</span>
            </div>

            <!-- Waveform Container -->
            <div
                bind:this={canvasContainer}
                on:wheel={handleWheel}
                class="flex-1 overflow-x-auto overflow-y-hidden bg-white rounded border border-gray-200"
                style="height: {canvasHeight}px; min-height: {canvasHeight}px;"
            >
                {#if isLoadingSamples}
                    <div class="flex items-center justify-center h-full bg-gray-50">
                        <div class="flex items-center gap-3">
                            <div class="w-5 h-5 border-2 border-gray-300 border-t-blue-500 rounded-full animate-spin"></div>
                            <span class="text-gray-500 text-sm">Decoding audio...</span>
                        </div>
                    </div>
                {:else if loadError}
                    <div class="flex items-center justify-center h-full bg-red-50 px-6">
                        <div class="max-w-xl text-center">
                            <div class="text-red-700 font-semibold mb-2">{loadError.title}</div>
                            <div class="text-red-600 text-sm break-words">{loadError.message}</div>
                            {#if loadError.code}
                                <div class="text-red-400 text-xs mt-2">Code: {loadError.code}</div>
                            {/if}
                        </div>
                    </div>
                {:else if samples.length > 0}
                    <canvas
                        bind:this={canvas}
                        on:click={handleCanvasClick}
                        class="cursor-pointer"
                    ></canvas>
                {:else}
                    <div class="flex items-center justify-center h-full text-gray-400 text-sm">
                        No waveform data
                    </div>
                {/if}
            </div>
        </div>
    {:else if loadError}
        <div class="home-container">
            <div class="intro-section">
                <h1 class="intro-title error-title">{loadError.title}</h1>
                <p class="intro-desc error-desc">{loadError.message}</p>
                {#if loadError.code}
                    <p class="error-code">Code: {loadError.code}</p>
                {/if}
            </div>
            <FileUpload on:select={handleFileUpload} />
        </div>
    {:else}
        <!-- Home Page with Introduction -->
        <div class="home-container">
            <div class="intro-section">
                <h1 class="intro-title">Avioflow</h1>
                <p class="intro-desc">高性能音频引擎，支持 WAV、MP3、FLAC 等格式的解码与波形可视化</p>
            </div>
            <FileUpload on:select={handleFileUpload} />
        </div>
    {/if}
</main>

<style>
    .home-container {
        flex: 1;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        gap: 24px;
    }

    .intro-section {
        text-align: center;
    }

    .intro-title {
        font-size: 24px;
        font-weight: 700;
        color: #1f2937;
        margin: 0 0 8px 0;
    }

    .intro-desc {
        font-size: 14px;
        color: #6b7280;
        margin: 0;
        max-width: 300px;
    }

    .error-title {
        color: #b91c1c;
    }

    .error-desc {
        color: #991b1b;
        max-width: 520px;
    }

    .error-code {
        margin-top: 8px;
        font-size: 12px;
        color: #dc2626;
    }
</style>
