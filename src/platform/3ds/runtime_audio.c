#include <string.h>
#include "runtime_assets.h"
#include "diagnostics.h"
/* Original USA CTL/TBL layout. All offsets remain relative until audio/load.c
 * patches them; only numeric fields change from N64 big endian to ARM little endian. */
unsigned char gSoundDataADSR[97856];
unsigned char gSoundDataRaw[2216704];
static const uint8_t *source;
static uint8_t *destination;
static size_t extent;
static void range(size_t offset,size_t length) {
    if(offset>extent || length>extent-offset)diagnostics_fatal("Audio metadata offset out of bounds");
}
static uint32_t read32(size_t p) { range(p,4);return (uint32_t)source[p]<<24|(uint32_t)source[p+1]<<16|(uint32_t)source[p+2]<<8|source[p+3]; }
static uint32_t convert32(size_t p) {
    uint32_t v=read32(p);for(int i=0;i<4;++i)destination[p+i]=v>>(8*i);return v;
}
static uint16_t convert16(size_t p) {
    range(p,2);uint16_t v=(source[p]<<8)|source[p+1];destination[p]=v;destination[p+1]=v>>8;return v;
}
static void envelope(size_t base,uint32_t offset) {
    for(unsigned i=0;i<4096;++i) {
        int16_t delay=convert16(base+offset+4*i);convert16(base+offset+4*i+2);
        if(delay<0 && delay>=-3)return;
    }
    diagnostics_fatal("Audio envelope does not terminate");
}
static void sound(size_t base,size_t p) {
    uint32_t sample=convert32(p);convert32(p+4);
    if(!sample)return;
    p=base+sample;range(p,20);
    convert32(p+4);uint32_t loop=convert32(p+8),book=convert32(p+12);convert32(p+16);
    if(loop) {
        size_t pos=base+loop;convert32(pos);convert32(pos+4);
        uint32_t count=convert32(pos+8);convert32(pos+12);
        if(count)for(int i=0;i<16;++i)convert16(pos+16+i*2);
    }
    if(book) {
        size_t pos=base+book;uint32_t order=convert32(pos),predictors=convert32(pos+4);
        if(order>16 || predictors>16)diagnostics_fatal("Audio predictor book size invalid");
        for(unsigned i=0;i<order*predictors*8;++i)convert16(pos+8+2*i);
    }
}
static unsigned table(void) {
    convert16(0);unsigned count=convert16(2);
    range(4,count*8);
    for(unsigned i=0;i<count;++i){convert32(4+i*8);convert32(8+i*8);}
    return count;
}
void runtime_audio_convert(uint8_t *rom,size_t size) {
    if(size<5846368+sizeof(gSoundDataRaw))diagnostics_fatal("ROM audio ranges missing");
    source=rom+5748512;destination=gSoundDataADSR;extent=sizeof(gSoundDataADSR);
    memcpy(destination,source,extent);
    unsigned count=table();
    for(unsigned bank=0;bank<count;++bank) {
        size_t start=read32(4+bank*8);size_t length=read32(8+bank*8);range(start,length);
        unsigned instruments=convert32(start),drums=convert32(start+4);
        if(instruments>255 || drums>255)diagnostics_fatal("Audio bank count invalid");
        convert32(start+8);convert32(start+12);size_t base=start+16;
        uint32_t drum_table=convert32(base);
        for(unsigned i=0;i<instruments;++i) {
            uint32_t offset=convert32(base+4+i*4);if(!offset)continue;
            size_t p=base+offset;range(p,32);
            uint32_t env=convert32(p+4);envelope(base,env);
            sound(base,p+8);sound(base,p+16);sound(base,p+24);
        }
        if(drums && !drum_table)diagnostics_fatal("Audio drum table missing");
        for(unsigned i=0;i<drums;++i) {
            uint32_t offset=convert32(base+drum_table+i*4);if(!offset)continue;
            size_t p=base+offset;range(p,16);
            sound(base,p+4);envelope(base,convert32(p+12));
        }
    }
    source=rom+5846368;destination=gSoundDataRaw;extent=sizeof(gSoundDataRaw);
    memcpy(destination,source,extent);table();
}
