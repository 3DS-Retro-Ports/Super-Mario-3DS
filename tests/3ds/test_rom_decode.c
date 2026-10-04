#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "platform/3ds/runtime_assets.h"
int main(void) {
    uint8_t digest[20];
    const uint8_t abc[]={0xa9,0x99,0x3e,0x36,0x47,0x06,0x81,0x6a,0xba,0x3e,0x25,0x71,0x78,0x50,0xc2,0x6c,0x9c,0xd0,0xd8,0x9d};
    runtime_sha1((const uint8_t*)"abc",3,digest);assert(!memcmp(abc,digest,20));
    /* One literal followed by an overlapping distance-1 backreference. */
    uint8_t mio[]={ 'M','I','O','0',0,0,0,4,0,0,0,17,0,0,0,19,0x80,0,0,'A' };
    uint8_t out[4];size_t length=0;
    assert(!runtime_mio0(mio,sizeof(mio),out,sizeof(out),&length));assert(length==4&&!memcmp(out,"AAAA",4));
    mio[16]=0;assert(runtime_mio0(mio,sizeof(mio),out,sizeof(out),&length));
    mio[16]=0x80;assert(runtime_mio0(mio,sizeof(mio),out,3,&length));
    assert(runtime_mio0(mio,18,out,4,&length));
}
