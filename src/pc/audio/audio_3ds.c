#ifdef TARGET_N3DS

#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include "macros.h"
#include "audio_api.h"
#include "platform/3ds/diagnostics.h"

#define N3DS_DSP_DMA_BUFFER_COUNT   4

static int sNextBuffer;
static u8 *bufferData;
static bool initialized;
static unsigned submitted, underruns, queue_timeouts, pcm_peak, silent_blocks;
static unsigned log_frames;
static void report_audio(void) {
    FILE *log = fopen("sdmc:/sm64/audio.log", "w");
    if (!log) return;
    fprintf(log, "NDSP: 32000 Hz, stereo PCM16, unity gain\n"
        "Submitted blocks: %u\nQueue empty before submit: %u\n"
        "Queue wait timeouts (dropped blocks): %u\n"
        "PCM peak since startup (0..32768): %u\nSilent blocks: %u\n",
        submitted, underruns, queue_timeouts, pcm_peak, silent_blocks);
    fclose(log);
}
static ndspWaveBuf sDspBuffers[N3DS_DSP_DMA_BUFFER_COUNT];

static bool audio_3ds_init(void)
{
    Result result = ndspInit();
    if (R_FAILED(result)) diagnostics_result("ndspInit (check DSP firmware)", result);

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnWaveBufClear(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, 32000);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0;
    mix[1] = 1.0;
    ndspChnSetMix(0, mix);

    bufferData = linearAlloc(4096 * 4 * N3DS_DSP_DMA_BUFFER_COUNT);
    if (!bufferData) { ndspExit(); diagnostics_fatal("Audio linearAlloc returned NULL"); }
    initialized = true;
    for(int i = 0; i < N3DS_DSP_DMA_BUFFER_COUNT; i++)
    {
        sDspBuffers[i].data_vaddr = &bufferData[i * 4096 * 4];
        sDspBuffers[i].nsamples = 0;
        sDspBuffers[i].status = NDSP_WBUF_FREE;
    }

    sNextBuffer = 0;
    submitted = underruns = queue_timeouts = pcm_peak = silent_blocks = log_frames = 0;
    report_audio();
    return true;
}

static int audio_3ds_buffered(void)
{
    int total = 0;
    /* Position is relative to the current wave buffer. Only subtract it when
     * its sequence ID is stable; the DSP can advance between these reads. */
    u16 sequence = ndspChnGetWaveBufSeq(0);
    u32 position = ndspChnGetSamplePos(0);
    if (sequence != ndspChnGetWaveBufSeq(0)) position = 0;
    for (int i = 0; i < N3DS_DSP_DMA_BUFFER_COUNT; i++) {
        u8 status = sDspBuffers[i].status;
        if (status != NDSP_WBUF_QUEUED && status != NDSP_WBUF_PLAYING) continue;
        u32 remaining = sDspBuffers[i].nsamples;
        if (sDspBuffers[i].sequence_id == sequence) {
            remaining -= position < remaining ? position : remaining;
        }
        total += remaining;
    }
    return total;
}

static int audio_3ds_get_desired_buffered(void)
{
    return 1100;
}

static int free_buffer(void) {
    for (int offset = 0; offset < N3DS_DSP_DMA_BUFFER_COUNT; ++offset) {
        int index = (sNextBuffer + offset) % N3DS_DSP_DMA_BUFFER_COUNT;
        u8 status = sDspBuffers[index].status;
        if (status == NDSP_WBUF_FREE || status == NDSP_WBUF_DONE) return index;
    }
    return -1;
}
static void audio_3ds_play(const uint8_t *buf, size_t len)
{
    if (!initialized || !buf || !len || len > 4096 * 4 || len % 4)
        diagnostics_fatal("Invalid NDSP stereo PCM block");
    int slot = free_buffer();
    /* Preserve a completed mix when DSP is slightly behind. Bound the wait so
     * a stalled DSP cannot freeze the game indefinitely. */
    for (unsigned wait = 0; slot < 0 && wait < 100; ++wait) {
        svcSleepThread(1000000);
        slot = free_buffer();
    }
    if (slot < 0) {
        ++queue_timeouts;
        if (queue_timeouts == 1) diagnostics_log("NDSP queue stalled: block dropped after 100 ms");
        if (queue_timeouts == 1 || queue_timeouts % 300 == 0) report_audio();
        return;
    }
    if (submitted && !audio_3ds_buffered()) {
        ++underruns;
        if (underruns == 1) diagnostics_log("NDSP queue ran empty; see sdmc:/sm64/audio.log");
    }
    s16 *dst = (s16 *)sDspBuffers[slot].data_vaddr;
    memcpy(dst, buf, len);
    unsigned peak = 0;
    for (size_t i = 0; i < len / sizeof(*dst); ++i) {
        unsigned magnitude = dst[i] < 0 ? -(int)dst[i] : dst[i];
        if (magnitude > peak) peak = magnitude;
    }
    if (!peak) ++silent_blocks;
    if (peak > pcm_peak) pcm_peak = peak;
    DSP_FlushDataCache(dst, len);
    sDspBuffers[slot].nsamples = len / 4;
    sDspBuffers[slot].status = NDSP_WBUF_FREE;
    ndspChnWaveBufAdd(0, &sDspBuffers[slot]);
    sNextBuffer = (slot + 1) % N3DS_DSP_DMA_BUFFER_COUNT;
    ++submitted;
    if (++log_frames >= 300 || submitted == 1) { report_audio(); log_frames = 0; }
}

void audio_3ds_shutdown(void) {
    if (!initialized) return;
    report_audio();
    ndspChnWaveBufClear(0);
    ndspExit();
    linearFree(bufferData);
    initialized = false;
}
struct AudioAPI audio_3ds =
{
    audio_3ds_init,
    audio_3ds_buffered,
    audio_3ds_get_desired_buffered,
    audio_3ds_play
};

#endif
