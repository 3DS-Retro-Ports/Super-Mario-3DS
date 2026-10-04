#include <string.h>
#include "runtime_assets.h"
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }
int runtime_mio0(const uint8_t *in, size_t size, uint8_t *out, size_t capacity, size_t *written) {
    if (size < 16 || memcmp(in,"MIO0",4)) return -1;
    size_t length=be32(in+4), compressed=be32(in+8), raw=be32(in+12), flags=16, pos=0;
    if (length > capacity || compressed < 16 || raw < compressed || raw > size) return -1;
    uint8_t mask=0, bits=0;
    while (pos<length) {
        if (!mask) { if (flags >= be32(in+8)) return -1; bits=in[flags++]; mask=0x80; }
        if (bits & mask) { if (raw >= size) return -1; out[pos++]=in[raw++]; }
        else {
            if (compressed+2 > be32(in+12)) return -1;
            unsigned code=(in[compressed]<<8)|in[compressed+1]; compressed+=2;
            size_t count=(code>>12)+3, distance=(code&4095)+1;
            if (distance>pos || count>length-pos) return -1;
            while (count--) { out[pos]=out[pos-distance]; ++pos; }
        }
        mask >>= 1;
    }
    *written=length; return 0;
}
static uint32_t rol(uint32_t n, unsigned bits) { return (n<<bits)|(n>>(32-bits)); }
void runtime_sha1(const uint8_t *data, size_t size, uint8_t digest[20]) {
    uint32_t h[5]={0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0};
    size_t blocks=(size+9+63)/64;
    for (size_t block=0;block<blocks;++block) {
        uint8_t bytes[64]={0}; size_t start=block*64;
        if (start<size) { size_t count=size-start; if(count>64)count=64; memcpy(bytes,data+start,count); }
        if (size>=start && size<start+64) bytes[size-start]=0x80;
        if (block+1==blocks) for(unsigned i=0;i<8;++i) bytes[63-i]=(uint64_t)size*8>>(8*i);
        uint32_t w[80]; for(int i=0;i<16;++i)w[i]=be32(bytes+4*i);
        for(int i=16;i<80;++i)w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
        for(int i=0;i<80;++i) {
            uint32_t f,k;
            if(i<20){f=(b&c)|(~b&d);k=0x5a827999;}
            else if(i<40){f=b^c^d;k=0x6ed9eba1;}
            else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8f1bbcdc;}
            else {f=b^c^d;k=0xca62c1d6;}
            uint32_t t=rol(a,5)+f+e+k+w[i];e=d;d=c;c=rol(b,30);b=a;a=t;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;
    }
    for(int i=0;i<20;++i)digest[i]=h[i/4]>>(24-8*(i%4));
}
