<script lang="ts">
    import { onMount } from "svelte";
    import "./globals.css";

    let metadata: any = null;
    let samples: Float32Array[] = [];
    let minData: Float32Array[] = [];
    let maxData: Float32Array[] = [];
    let isPlaying = false;
    let currentTime = 0;
    let duration = 0;
    let volume = 0.5;
    let canvasWidth = 0;
    let canvasHeight = 0;
    let filePath = "";
    let uiStartTime = 0;
    let finalTotalTime = 0;

    let audioContext: AudioContext;
    let audioBuffer: AudioBuffer | null = null;
    let sourceNode: AudioBufferSourceNode | null = null;
    let startTime = 0;
    let pauseOffset = 0;
    let animationFrame: number;

    let canvas: HTMLCanvasElement;
    let waveformSummary: { min: Float32Array; max: Float32Array }[] = [];

    onMount(() => {
        const handleInteraction = () => {
            if (audioContext && audioContext.state === "suspended") {
                audioContext.resume();
            }
        };
        window.addEventListener("click", handleInteraction);
        window.addEventListener("keydown", handleInteraction);

        window.addEventListener("message", (event) => {
            const message = event.data;
            switch (message.type) {
                case "loading":
                    uiStartTime = Date.now();
                    break;
                case "init":
                    filePath = message.filePath || "";
                    metadata = message.metadata;

                    if (uiStartTime > 0) {
                        finalTotalTime = Date.now() - uiStartTime;
                    }

                    if (message.samples) {
                        samples = message.samples.map((ch: any) =>
                            ch instanceof Float32Array ? ch : new Float32Array(ch),
                        );
                    }
                    if (message.min && message.max) {
                        minData = message.min.map((ch: any) =>
                            ch instanceof Float32Array ? ch : new Float32Array(ch),
                        );
                        maxData = message.max.map((ch: any) =>
                            ch instanceof Float32Array ? ch : new Float32Array(ch),
                        );
                    }
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

        if (samples && samples.length > 0) {
            const numChannels = samples.length;
            const length = samples[0].length;
            audioBuffer = audioContext.createBuffer(
                numChannels,
                length,
                metadata.sampleRate,
            );

            for (let i = 0; i < numChannels; i++) {
                const channelData = samples[i] instanceof Float32Array
                    ? samples[i]
                    : new Float32Array(samples[i]);
                audioBuffer.copyToChannel(channelData, i);
            }
        }

        calculateSummary();
        drawWaveform();
    }

    function calculateSummary() {
        if (minData.length > 0 && maxData.length > 0) {
            waveformSummary = minData.map((min, i) => ({
                min: min,
                max: maxData[i]
            }));
            return;
        }

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

    function drawTimeline(ctx: CanvasRenderingContext2D, width: number, height: number) {
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

            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);

            if (i > 0 && i < numTicks) {
                ctx.fillText(formatTime(time), x, 12);
            }
        }
        ctx.stroke();
        ctx.restore();
    }

    function drawWaveform() {
        if (!canvas || (waveformSummary.length === 0 && minData.length === 0)) return;

        const dpr = window.devicePixelRatio || 1;
        const displayWidth = canvas.offsetWidth;
        const displayHeight = canvas.offsetHeight;

        if (canvas.width !== displayWidth * dpr || canvas.height !== displayHeight * dpr) {
            canvas.width = displayWidth * dpr;
            canvas.height = displayHeight * dpr;
        }

        const ctx = canvas.getContext("2d")!;
        ctx.save();
        ctx.scale(dpr, dpr);

        const width = displayWidth;
        const height = displayHeight;
        ctx.clearRect(0, 0, width, height);

        drawTimeline(ctx, width, height);

        const numChannels = waveformSummary.length;
        const channelHeight = height / numChannels;
        const halfChannelHeight = channelHeight / 2;
        const drawHeight = halfChannelHeight * 0.92;

        waveformSummary.forEach((summary, chIndex) => {
            const yBase = chIndex * channelHeight + halfChannelHeight;
            const playedWidth = Math.floor((currentTime / duration) * width);
            const numPoints = summary.min.length;
            const stepX = width / numPoints;

            ctx.beginPath();
            ctx.strokeStyle = "#d1d1d6";
            ctx.lineWidth = Math.max(1, stepX);

            const startPoint = Math.floor(playedWidth / stepX);

            for (let i = startPoint; i < numPoints; i++) {
                const x = i * stepX + 0.5;
                ctx.moveTo(x, yBase + summary.min[i] * drawHeight);
                ctx.lineTo(x, yBase + summary.max[i] * drawHeight);
            }
            ctx.stroke();

            if (startPoint > 0) {
                ctx.beginPath();
                ctx.strokeStyle = "#007aff";
                ctx.lineWidth = Math.max(1, stepX);
                for (let i = 0; i < startPoint; i++) {
                    const x = i * stepX + 0.5;
                    ctx.moveTo(x, yBase + summary.min[i] * drawHeight);
                    ctx.lineTo(x, yBase + summary.max[i] * drawHeight);
                }
                ctx.stroke();
            }

            if (chIndex < numChannels - 1) {
                ctx.beginPath();
                ctx.strokeStyle = "rgba(0,0,0,0.05)";
                ctx.moveTo(0, (chIndex + 1) * channelHeight);
                ctx.lineTo(width, (chIndex + 1) * channelHeight);
                ctx.stroke();
            }
        });

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
            audioContext.resume().then(() => {
                startSourceNode();
            });
        } else {
            startSourceNode();
        }
    }

    function startSourceNode() {
        if (sourceNode) {
            sourceNode.stop();
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
    }

    $: if (canvas && (canvasWidth || canvasHeight)) {
        calculateSummary();
        canvas.width = canvas.offsetWidth;
        canvas.height = canvas.offsetHeight;
        drawWaveform();
    }
</script>

<main class="flex flex-col h-screen bg-vscode-bg text-vscode-fg overflow-hidden p-6 gap-5">
    {#if metadata}
        <!-- Header Section -->
        <div class="flex flex-col gap-2">
            <!-- File Path Row -->
            <div class="flex items-center gap-2">
                <span class="text-xs font-semibold text-gray-500 uppercase">File Path:</span>
                <code class="text-sm font-mono bg-gray-100 dark:bg-gray-800 px-2 py-1 rounded flex-1 truncate">
                    {filePath}
                </code>
                <button
                    class="p-1 hover:bg-gray-200 dark:hover:bg-gray-700 rounded transition-colors"
                    on:click={() => copyToClipboard(filePath)}
                    title="Copy Path"
                >
                    <svg viewBox="0 0 24 24" width="14" height="14" class="text-vscode-fg">
                        <path fill="currentColor" d="M16 1H4c-1.1 0-2 .9-2 2v14h2V3h12V1zm3 4H8c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h11c1.1 0 2-.9 2-2V7c0-1.1-.9-2-2-2zm0 16H8V7h11v14z" />
                    </svg>
                </button>
            </div>

            <!-- Stats Row -->
            <div class="flex gap-4 text-xs">
                <span class="text-gray-600 dark:text-gray-400">
                    total time: <span class="font-semibold text-vscode-fg">{finalTotalTime}ms</span>
                </span>
                <span class="text-gray-600 dark:text-gray-400">
                    decode time: <span class="font-semibold text-vscode-fg">{metadata.wasmDecodeTimeMs || 0}ms</span>
                </span>
            </div>

            <!-- Filename Row -->
            <div class="flex items-center gap-2">
                <svg class="w-6 h-6 text-vscode-fg" viewBox="0 0 24 24">
                    <path fill="currentColor" d="M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z" />
                </svg>
                <span class="text-lg font-semibold">{filePath.split(/[\\/]/).pop()}</span>
            </div>
        </div>

        <!-- Metadata Card -->
        <div class="bg-white dark:bg-gray-900 rounded-lg border border-gray-200 dark:border-gray-700 p-4">
            <div class="grid grid-cols-4 gap-4">
                <div class="flex flex-col gap-1">
                    <span class="meta-label">size</span>
                    <span class="meta-value">{formatSize(metadata.fileSize)}</span>
                </div>
                <div class="flex flex-col gap-1">
                    <span class="meta-label">format</span>
                    <span class="meta-value">{metadata.container}</span>
                </div>
                <div class="flex flex-col gap-1">
                    <span class="meta-label">codec</span>
                    <span class="meta-value">{metadata.codec.toLowerCase()}</span>
                </div>
                <div class="flex flex-col gap-1">
                    <span class="meta-label">duration</span>
                    <span class="meta-value">{formatTime(duration)}</span>
                </div>

                <div class="flex flex-col gap-1">
                    <span class="meta-label">sample rate</span>
                    <span class="meta-value">{metadata.sampleRate.toLocaleString()} Hz</span>
                </div>
                <div class="flex flex-col gap-1">
                    <span class="meta-label">channels</span>
                    <span class="meta-value">{metadata.numChannels}</span>
                </div>
                <div class="flex flex-col gap-1">
                    <span class="meta-label">bitrate</span>
                    <span class="meta-value">
                        {metadata.bitRate ? (metadata.bitRate / 1000).toFixed(0) + " kbps" : "Variable"}
                    </span>
                </div>
            </div>
        </div>

        <!-- Waveform Section -->
        <div class="flex-1 flex flex-col bg-white dark:bg-gray-900 rounded-lg border border-gray-200 dark:border-gray-700 overflow-hidden">
            <div
                class="flex-1 relative"
                bind:clientWidth={canvasWidth}
                bind:clientHeight={canvasHeight}
            >
                <canvas bind:this={canvas} on:click={handleSeek} class="w-full h-full cursor-pointer"></canvas>
                <div class="absolute inset-0 pointer-events-none flex flex-col">
                    {#each Array(metadata.numChannels) as _, i}
                        <div class="flex-1 flex items-center justify-start pl-2 text-xs text-gray-500 font-medium">
                            CH {i}
                        </div>
                    {/each}
                </div>
            </div>
        </div>

        <!-- Controls Card -->
        <div class="bg-white dark:bg-gray-900 rounded-lg border border-gray-200 dark:border-gray-700 p-4">
            <div class="flex items-center gap-4">
                <button
                    class="flex items-center justify-center w-10 h-10 rounded-lg bg-blue-600 hover:bg-blue-700 text-white transition-colors"
                    on:click={togglePlay}
                    aria-label={isPlaying ? "Pause" : "Play"}
                >
                    {#if isPlaying}
                        <svg viewBox="0 0 24 24" width="24" height="24">
                            <path fill="currentColor" d="M6 19h4V5H6v14zm8-14v14h4V5h-4z" />
                        </svg>
                    {:else}
                        <svg viewBox="0 0 24 24" width="24" height="24">
                            <path fill="currentColor" d="M8 5v14l11-7z" />
                        </svg>
                    {/if}
                </button>

                <div class="flex items-center gap-2 text-sm font-mono">
                    <span class="text-vscode-fg">{formatTime(currentTime)}</span>
                    <span class="text-gray-400">/</span>
                    <span class="text-gray-600 dark:text-gray-400">{formatTime(duration)}</span>
                </div>

                <div class="flex items-center gap-2 ml-auto">
                    <svg class="w-4 h-4 text-vscode-fg" viewBox="0 0 24 24">
                        <path fill="currentColor" d="M3 9v6h4l5 5V4L7 9H3zm13.5 3c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02zM14 3.23v2.06c2.89.86 5 3.54 5 6.71s-2.11 5.85-5 6.71v2.06c4.01-.91 7-4.49 7-8.77s-2.99-7.86-7-8.77z" />
                    </svg>
                    <input
                        type="range"
                        min="0"
                        max="1"
                        step="0.01"
                        bind:value={volume}
                        class="w-24 h-1 bg-gray-300 dark:bg-gray-600 rounded-lg appearance-none cursor-pointer"
                    />
                </div>
            </div>
        </div>
    {:else}
        <!-- Loading State -->
        <div class="flex flex-col items-center justify-center h-full gap-4">
            <div class="w-12 h-12 border-4 border-gray-300 dark:border-gray-600 border-t-blue-600 rounded-full animate-spin"></div>
            <span class="text-gray-600 dark:text-gray-400">Decoding audio...</span>
        </div>
    {/if}
</main>

<style>
    :global(body) {
        margin: 0;
        padding: 0;
        overflow: hidden;
    }

    :global(html) {
        height: 100%;
    }

    input[type="range"] {
        -webkit-appearance: none;
        appearance: none;
        background: transparent;
    }

    input[type="range"]::-webkit-slider-thumb {
        -webkit-appearance: none;
        appearance: none;
        width: 12px;
        height: 12px;
        border-radius: 50%;
        background: #007aff;
        cursor: pointer;
    }

    input[type="range"]::-moz-range-thumb {
        width: 12px;
        height: 12px;
        border-radius: 50%;
        background: #007aff;
        cursor: pointer;
        border: none;
    }
</style>
