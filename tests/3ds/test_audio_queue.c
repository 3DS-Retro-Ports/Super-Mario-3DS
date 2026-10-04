/* Exercise the real backend with asynchronous status/position controlled here. */
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/stat.h>
#define TARGET_N3DS
#include "../../src/pc/audio/audio_3ds.c"
static unsigned sleeps, release_after, adds, sequence_reads;
static u16 playing_sequence;
static u32 sample_position;
static bool changing_sequence;
static jmp_buf fatal;
Result ndspInit(void) { return 0; }
void ndspExit(void) {}
void ndspSetOutputMode(int mode) { assert(mode==NDSP_OUTPUT_STEREO); }
void ndspChnReset(int ch) { assert(!ch); }
void ndspChnWaveBufClear(int ch) { (void)ch; }
void ndspChnSetInterp(int ch,int mode) { (void)ch;(void)mode; }
void ndspChnSetRate(int ch,float rate) { (void)ch;assert(rate==32000); }
void ndspChnSetFormat(int ch,int format) { (void)ch;assert(format==NDSP_FORMAT_STEREO_PCM16); }
void ndspChnSetMix(int ch,const float *mix) { (void)ch;assert(mix[0]==1 && mix[1]==1); }
void *linearAlloc(size_t size) { return malloc(size); }
void linearFree(void *ptr) { free(ptr); }
u16 ndspChnGetWaveBufSeq(int ch) { (void)ch; return playing_sequence + (changing_sequence && sequence_reads++ % 2); }
u32 ndspChnGetSamplePos(int ch) { (void)ch;return sample_position; }
void ndspChnWaveBufAdd(int ch,ndspWaveBuf *buf) { (void)ch;buf->status=NDSP_WBUF_QUEUED;buf->sequence_id=++adds; }
void DSP_FlushDataCache(void *ptr,size_t size) { assert(ptr && size); }
void svcSleepThread(int64_t ns) { assert(ns==1000000); if (++sleeps==release_after) sDspBuffers[2].status=NDSP_WBUF_DONE; }
void diagnostics_log(const char *format,...) { (void)format; }
_Noreturn void diagnostics_fatal(const char *message) { (void)message;longjmp(fatal,1); }
_Noreturn void diagnostics_result(const char *op,int32_t result) { (void)op;(void)result;abort(); }
int main(void) {
    char dir[]="/tmp/sm64-audio-test-XXXXXX"; assert(mkdtemp(dir)); assert(!chdir(dir));
    assert(!mkdir("sdmc:",0700)); assert(!mkdir("sdmc:/sm64",0700));
    assert(audio_3ds.init());
    int16_t pcm[1088*2];
    for (unsigned i=0;i<sizeof(pcm)/sizeof(*pcm);++i) pcm[i]=(i%2)?-32768:12345;
    audio_3ds.play((uint8_t *)pcm,sizeof(pcm));
    assert(adds==1 && sDspBuffers[0].nsamples==1088 && pcm_peak==32768);
    assert(!memcmp(sDspBuffers[0].data_vaddr,pcm,sizeof(pcm)));
    sDspBuffers[0].status=NDSP_WBUF_PLAYING; playing_sequence=1; sample_position=800;
    assert(audio_3ds.buffered()==288);
    audio_3ds.play((uint8_t *)pcm,sizeof(pcm)); assert(audio_3ds.buffered()==1376);
    changing_sequence=true;sequence_reads=0; assert(audio_3ds.buffered()==2176); changing_sequence=false;
    sample_position=9000; assert(audio_3ds.buffered()==1088); sample_position=0;
    /* The next rotating slot is busy, but another slot is reusable. */
    sDspBuffers[2].status=NDSP_WBUF_QUEUED;sDspBuffers[2].nsamples=1088;
    audio_3ds.play((uint8_t *)pcm,sizeof(pcm)); assert(adds==3 && !sleeps);
    /* Full queue: release one asynchronously, preserve the pending PCM. */
    release_after=3;audio_3ds.play((uint8_t *)pcm,sizeof(pcm));
    assert(adds==4 && sleeps==3 && !queue_timeouts);
    assert(!memcmp(sDspBuffers[2].data_vaddr,pcm,sizeof(pcm)));
    sleeps=0;release_after=0;audio_3ds.play((uint8_t *)pcm,sizeof(pcm));
    assert(sleeps==100 && queue_timeouts==1 && adds==4);
    for (unsigned i=0;i<4;++i) sDspBuffers[i].status=NDSP_WBUF_DONE;
    memset(pcm,0,sizeof(pcm));audio_3ds.play((uint8_t *)pcm,sizeof(pcm));
    assert(underruns==1 && silent_blocks==1);
    if (!setjmp(fatal)) { audio_3ds.play((uint8_t *)pcm,3);assert(0); }
    audio_3ds_shutdown();
    FILE *f=fopen("sdmc:/sm64/audio.log","r");assert(f);char log[1024];
    size_t n=fread(log,1,sizeof(log)-1,f);log[n]=0;fclose(f);
    assert(strstr(log,"dropped blocks): 1") && strstr(log,"32768): 32768"));
    unlink("sdmc:/sm64/audio.log");rmdir("sdmc:/sm64");rmdir("sdmc:");chdir("/");rmdir(dir);
    puts("NDSP queue accounting, backpressure, PCM preservation and logs: PASS");
}
