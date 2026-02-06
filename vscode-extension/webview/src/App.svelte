<script lang="ts">
    import { onMount } from "svelte";
    import "./globals.css";

    let metadata: any = null;
    let samples: Float32Array[] = [];
    let filePath = "";
    let decodeTimeMs = 0;

    let audioContext: AudioContext;
    let audioBuffer: AudioBuffer | null = null;
    let sourceNode: AudioBufferSourceNode | null = null;
    let isPlaying = false;
    let currentTime = 0;
    let duration = 0;
    let startTime = 0;
    let pauseOffset = 0;
    let animationFrame: number;

    let canvas: HTMLCanvasElement;
    let canvasContainer: HTMLDivElement;

    onMount(() => {
        window.addEventListener("message", handleMessage);

        // @ts-ignore
        const vscode = acquireVsCodeApi();
        vscode.postMessage({ type: "ready" });

        return () => {
            window.removeEventListener("message", handleMessage);
            if (animationFrame) cancelAnimationFrame(animationFrame);
            if (sourceNode) sourceNode.stop();
            if (audioContext) audioContext.close();
        };
    });

    function handleMessage(event: MessageEvent) {
        const message = event.data;
        if (message.type === "init") {
            filePath = message.filePath || "";
            metadata = message.metadata;
            decodeTimeMs = metadata.wasmDecodeTimeMs || 0;
            duration = metadata.duration || 0;

            if (message.samples && message.samples.length > 0) {
                samples = message.samples.map((ch: any) =>
                    ch instanceof Float32Array ? ch : new Float32Array(ch)
                );
            }

            initAudio();
        }
    }

    function initAudio() {
        if (!audioContext) {
            audioContext = new (window.AudioContext || (window as any).webkitAudioContext)();
        }

        if (samples.length > 0) {
            const numChannels = samples.length;
            const length = samples[0].length;
            audioBuffer = audioContext.createBuffer(numChannels, length, metadata.sampleRate);

            for (let i = 0; i < numChannels; i++) {
                audioBuffer.copyToChannel(samples[i], i);
            }
        }

        // Wait for layout to complete
        setTimeout(() => {
            drawWaveform();
        }, 50);
    }

    function drawWaveform() {
        if (!canvas || samples.length === 0) return;

        const rect = canvasContainer.getBoundingClientRect();
        const width = rect.width;
        const height = rect.height;

        if (width === 0 || height === 0) {
            setTimeout(drawWaveform, 50);
            return;
        }

        const dpr = window.devicePixelRatio || 1;
        canvas.width = width * dpr;
        canvas.height = height * dpr;
        canvas.style.width = width + "px";
        canvas.style.height = height + "px";

        const ctx = canvas.getContext("2d")!;
        ctx.scale(dpr, dpr);
        ctx.clearRect(0, 0, width, height);

        const numChannels = samples.length;
        const channelHeight = height / numChannels;

        for (let ch = 0; ch < numChannels; ch++) {
            const data = samples[ch];
            const yOffset = ch * channelHeight;
            const centerY = yOffset + channelHeight / 2;
            const amplitude = channelHeight / 2 * 0.9;

            // Calculate min/max for each pixel
            const step = data.length / width;

            // Draw unplayed part (gray)
            ctx.beginPath();
            ctx.strokeStyle = "#cccccc";
            ctx.lineWidth = 1;

            const playedX = (currentTime / duration) * width;

            for (let x = Math.floor(playedX); x < width; x++) {
                const startIdx = Math.floor(x * step);
                const endIdx = Math.floor((x + 1) * step);

                let min = 0, max = 0;
                for (let i = startIdx; i < endIdx && i < data.length; i++) {
                    if (data[i] < min) min = data[i];
                    if (data[i] > max) max = data[i];
                }

                ctx.moveTo(x + 0.5, centerY + min * amplitude);
                ctx.lineTo(x + 0.5, centerY + max * amplitude);
            }
            ctx.stroke();

            // Draw played part (blue)
            if (playedX > 0) {
                ctx.beginPath();
                ctx.strokeStyle = "#3b82f6";
                ctx.lineWidth = 1;

                for (let x = 0; x < playedX && x < width; x++) {
                    const startIdx = Math.floor(x * step);
                    const endIdx = Math.floor((x + 1) * step);

                    let min = 0, max = 0;
                    for (let i = startIdx; i < endIdx && i < data.length; i++) {
                        if (data[i] < min) min = data[i];
                        if (data[i] > max) max = data[i];
                    }

                    ctx.moveTo(x + 0.5, centerY + min * amplitude);
                    ctx.lineTo(x + 0.5, centerY + max * amplitude);
                }
                ctx.stroke();
            }

            // Draw channel separator
            if (ch < numChannels - 1) {
                ctx.beginPath();
                ctx.strokeStyle = "#e5e5e5";
                ctx.lineWidth = 1;
                ctx.moveTo(0, (ch + 1) * channelHeight);
                ctx.lineTo(width, (ch + 1) * channelHeight);
                ctx.stroke();
            }

            // Draw channel label
            ctx.fillStyle = "#999999";
            ctx.font = "11px sans-serif";
            ctx.fillText(`CH ${ch}`, 8, yOffset + 16);
        }

        // Draw playhead
        const playheadX = (currentTime / duration) * width;
        ctx.beginPath();
        ctx.strokeStyle = "#ef4444";
        ctx.lineWidth = 2;
        ctx.moveTo(playheadX, 0);
        ctx.lineTo(playheadX, height);
        ctx.stroke();
    }

    function handleCanvasClick(e: MouseEvent) {
        if (!canvas || duration === 0) return;
        const rect = canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const ratio = Math.max(0, Math.min(1, x / rect.width));

        if (isPlaying) pause();
        pauseOffset = ratio * duration;
        currentTime = pauseOffset;
        drawWaveform();
        play();
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
</script>

<main class="flex flex-col h-full w-full bg-white p-4 gap-4">
    {#if metadata}
        <!-- Area 1: File Path -->
        <div class="flex items-center gap-2 px-3 py-2 bg-gray-50 rounded border border-gray-200">
            <span class="text-xs text-gray-500 uppercase font-medium">Path:</span>
            <code class="text-sm text-gray-700 truncate flex-1">{filePath}</code>
        </div>

        <!-- Area 2: Metadata (two columns) -->
        <div class="grid grid-cols-2 gap-x-8 gap-y-2 px-3 py-3 bg-gray-50 rounded border border-gray-200 text-sm">
            <div class="flex justify-between">
                <span class="text-gray-500">File:</span>
                <span class="text-gray-800 font-medium">{getFileName(filePath)}</span>
            </div>
            <div class="flex justify-between">
                <span class="text-gray-500">Size:</span>
                <span class="text-gray-800">{formatSize(metadata.fileSize)}</span>
            </div>
            <div class="flex justify-between">
                <span class="text-gray-500">Format:</span>
                <span class="text-gray-800">{metadata.container || "-"}</span>
            </div>
            <div class="flex justify-between">
                <span class="text-gray-500">Codec:</span>
                <span class="text-gray-800">{metadata.codec || "-"}</span>
            </div>
            <div class="flex justify-between">
                <span class="text-gray-500">Duration:</span>
                <span class="text-gray-800">{formatTime(duration)}</span>
            </div>
            <div class="flex justify-between">
                <span class="text-gray-500">Sample Rate:</span>
                <span class="text-gray-800">{metadata.sampleRate?.toLocaleString() || "-"} Hz</span>
            </div>
            <div class="flex justify-between">
                <span class="text-gray-500">Channels:</span>
                <span class="text-gray-800">{metadata.numChannels || "-"}</span>
            </div>
            <div class="flex justify-between">
                <span class="text-gray-500">Bitrate:</span>
                <span class="text-gray-800">{metadata.bitRate ? Math.round(metadata.bitRate / 1000) + " kbps" : "-"}</span>
            </div>
            <div class="flex justify-between">
                <span class="text-gray-500">Decode Time:</span>
                <span class="text-gray-800">{decodeTimeMs} ms</span>
            </div>
        </div>

        <!-- Area 3: Waveform Canvas -->
        <div
            bind:this={canvasContainer}
            class="flex-1 relative bg-white rounded border border-gray-200 overflow-hidden min-h-[120px]"
        >
            <canvas
                bind:this={canvas}
                on:click={handleCanvasClick}
                class="absolute inset-0 w-full h-full cursor-pointer"
            ></canvas>
        </div>

        <!-- Controls -->
        <div class="flex items-center gap-4 px-3 py-2 bg-gray-50 rounded border border-gray-200">
            <button
                on:click={togglePlay}
                class="w-10 h-10 flex items-center justify-center rounded-full bg-blue-500 hover:bg-blue-600 text-white"
            >
                {#if isPlaying}
                    <svg viewBox="0 0 24 24" class="w-5 h-5">
                        <path fill="currentColor" d="M6 19h4V5H6v14zm8-14v14h4V5h-4z"/>
                    </svg>
                {:else}
                    <svg viewBox="0 0 24 24" class="w-5 h-5">
                        <path fill="currentColor" d="M8 5v14l11-7z"/>
                    </svg>
                {/if}
            </button>
            <span class="text-sm text-gray-600 font-mono">
                {formatTime(currentTime)} / {formatTime(duration)}
            </span>
        </div>
    {:else}
        <!-- Loading -->
        <div class="flex-1 flex items-center justify-center">
            <div class="text-gray-500">Loading audio...</div>
        </div>
    {/if}
</main>
