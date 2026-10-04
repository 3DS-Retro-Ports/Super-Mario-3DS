#ifndef AUDIO_TEST_3DS_H
#define AUDIO_TEST_3DS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t Result;
#define R_FAILED(r) ((r) < 0)
enum { NDSP_WBUF_FREE, NDSP_WBUF_QUEUED, NDSP_WBUF_PLAYING, NDSP_WBUF_DONE };
enum { NDSP_OUTPUT_STEREO, NDSP_INTERP_LINEAR, NDSP_FORMAT_STEREO_PCM16 };
typedef struct { void *data_vaddr; u32 nsamples; volatile u8 status; u16 sequence_id; } ndspWaveBuf;
Result ndspInit(void);
void ndspExit(void);
void ndspSetOutputMode(int);
void ndspChnReset(int);
void ndspChnWaveBufClear(int);
void ndspChnSetInterp(int,int);
void ndspChnSetRate(int,float);
void ndspChnSetFormat(int,int);
void ndspChnSetMix(int,const float *);
void *linearAlloc(size_t);
void linearFree(void *);
u16 ndspChnGetWaveBufSeq(int);
u32 ndspChnGetSamplePos(int);
void ndspChnWaveBufAdd(int,ndspWaveBuf *);
void DSP_FlushDataCache(void *,size_t);
void svcSleepThread(int64_t);
#endif
