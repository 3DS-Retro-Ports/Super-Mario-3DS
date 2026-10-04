#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "platform/3ds/save_store.h"
int main(void) {
    char dir[]="/tmp/sm64-save-test-XXXXXX";assert(mkdtemp(dir));assert(!chdir(dir));assert(!mkdir("sdmc:",0700));
    assert(save_store_init());unsigned char data[512],readback[512];memset(data,0,sizeof(data));
    /* Four logical slots survive arbitrary partial EEPROM writes. */
    for(int i=0;i<4;++i)memset(data+i*112,10+i,112);
    assert(!save_store_write(0,data,512));data[117]=91;
    assert(!save_store_write(117,data+117,1));assert(save_store_init());
    assert(!save_store_read(0,readback,512));assert(!memcmp(data,readback,512));
    assert(save_store_write(511,data,2)==-1);assert(save_store_read(513,readback,0)==-1);
    FILE *f=fopen(SAVE_STORE_DIRECTORY "/eeprom.1","r+b");assert(f);assert(!fseek(f,8,SEEK_SET));fputc(99,f);fclose(f);
    assert(save_store_init());assert(!save_store_read(0,readback,512));assert(readback[117]==11);
    f=fopen(SAVE_STORE_DIRECTORY "/eeprom.0","wb");assert(f);fputs("torn",f);fclose(f);
    assert(!save_store_init());assert(save_store_error());assert(save_store_write(0,data,512)==-1);
    /* Unknown/corrupt generations are preserved instead of reset. */
    f=fopen(SAVE_STORE_DIRECTORY "/eeprom.0","rb");assert(f);assert(fgetc(f)=='t');fclose(f);
    unlink(SAVE_STORE_DIRECTORY "/eeprom.0");unlink(SAVE_STORE_DIRECTORY "/eeprom.1");
    rmdir(SAVE_STORE_DIRECTORY);rmdir("sdmc:/3ds/sm64");rmdir("sdmc:/3ds");rmdir("sdmc:");chdir("/");rmdir(dir);
    puts("four slots, partial writes, header corruption, fallback and refusal to overwrite: PASS");
}
