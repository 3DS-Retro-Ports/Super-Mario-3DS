#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime_assets.h"
#include "asset_loader.h"
#include "diagnostics.h"
extern const struct RuntimeAsset __start_sm64_assets[], __stop_sm64_assets[];
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]; }
void runtime_assets_load(void) {
    diagnostics_log("Loading runtime assets from %s", SM64_ROM_PATH);
    FILE *f=fopen(SM64_ROM_PATH,"rb");
    if(!f)diagnostics_errno("Runtime ROM fopen",errno);
    uint8_t *rom=malloc(SM64_US_ROM_SIZE);
    if(!rom){fclose(f);diagnostics_fatal("Cannot allocate ROM staging buffer");}
    size_t count=fread(rom,1,SM64_US_ROM_SIZE,f);
    bool valid=count==SM64_US_ROM_SIZE && fgetc(f)==EOF && !ferror(f);
    int result=fclose(f);
    if(!valid || result){free(rom);diagnostics_fatal("Runtime ROM read/size error (expected 8 MiB)");}
    const char *error=asset_validate_rom_header(rom,64,count);
    if(error){free(rom);diagnostics_fatal(error);}
    uint8_t digest[20]; runtime_sha1(rom,count,digest);
    static const uint8_t expected[]={0x9b,0xef,0x11,0x28,0x71,0x7f,0x95,0x81,0x71,0xa4,0xaf,0xac,0x3e,0xd7,0x8e,0xe2,0xbb,0x4e,0x86,0xce};
    if(memcmp(digest,expected,20)){free(rom);diagnostics_fatal("Runtime ROM SHA-1 does not match USA revision 0");}
    uint8_t *decoded=NULL; size_t decoded_size=0; uint32_t cached=~0u;
    for(const struct RuntimeAsset *a=__start_sm64_assets;a<__stop_sm64_assets;++a) {
        if(a->rom>=count)diagnostics_fatal("Asset descriptor ROM offset out of bounds");
        const uint8_t *data=rom+a->rom; size_t length=count-a->rom;
        if(a->kind!=0) {
            if(cached!=a->rom) {
                free(decoded); decoded=NULL;
                if(length<16 || memcmp(data,"MIO0",4))diagnostics_fatal("Asset MIO0 header missing");
                decoded_size=be32(data+4);
                if(decoded_size>4*1024*1024 || !decoded_size)diagnostics_fatal("Asset MIO0 length invalid");
                decoded=malloc(decoded_size);
                if(!decoded)diagnostics_fatal("Asset decompression allocation failed");
                if(runtime_mio0(data,length,decoded,decoded_size,&decoded_size))diagnostics_fatal("Malformed MIO0 asset");
                cached=a->rom;
            }
            data=decoded; length=decoded_size;
        }
        if(a->kind==3) {
            if(length<320 || (length-320)%2048 || length-320>a->size)diagnostics_fatal("Skybox data length invalid");
            memcpy(a->destination,data,length-320);
            continue;
        }
        if(a->offset>length || a->size>length-a->offset) {
            diagnostics_log("Asset ROM=%08lx offset=%lu size=%lu available=%lu", (unsigned long)a->rom,
                (unsigned long)a->offset,(unsigned long)a->size,(unsigned long)length);
            diagnostics_fatal("Asset range outside source data");
        }
        if(a->kind==2) {
            /* Skybox pointer tables contain segmented N64 addresses. */
            if(length<320 || (length-320)%2048)diagnostics_fatal("Skybox pointer table length invalid");
            size_t table=length-320;
            const uint8_t **p=a->destination; uint32_t base=be32(data+table);
            for(unsigned i=0;i<a->size/4;++i) {
                uint32_t offset=be32(data+table+4*i)-base;
                if(offset>table || 2048>table-offset)diagnostics_fatal("Skybox tile offset outside texture data");
                p[i]=(uint8_t*)a->base+offset;
            }
        } else memcpy(a->destination,data+a->offset,a->size);
    }
    free(decoded);
    runtime_audio_convert(rom,count);
    free(rom);
    diagnostics_log("Runtime texture, demo and audio assets loaded");
}
