#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pc/mixer.h"
/* PCM reference vectors exercise actual gain, envelope persistence and interleave. */
int main(void) {
    int16_t input[16], output[32]; uint16_t state[40]={0};
    for (int i=0;i<16;++i) input[i]=(i&1)?-12000:12000;
    for (int pass=0;pass<2;++pass) {
        aSetBuffer(NULL,0,0x20,0,sizeof(input));aLoadBuffer(NULL,(uint16_t *)input);
        aClearBuffer(NULL,0x4c0,0x280);
        aSetBuffer(NULL,0,0x20,0x4c0,sizeof(input));
        aSetBuffer(NULL,8,0x600,0x740,0x880);
        aSetVolume(NULL,6,32767,0,0);aSetVolume(NULL,4,16384,0,0);
        aSetVolume32(NULL,2,32767,65536);aSetVolume32(NULL,0,16384,65536);
        aSetVolume(NULL,8,32767,0,0);
        aEnvMixer(NULL,pass?0:1,state);
        aSetBuffer(NULL,0,0,0,sizeof(input));aInterleave(NULL,0x4c0,0x600);
        aSetBuffer(NULL,0,0,0,sizeof(output));aSaveBuffer(NULL,(uint16_t *)output);
        for (int i=0;i<16;++i) {
            int left=input[i]*32766>>15, right=input[i]*16384>>15;
            assert(output[2*i]==left && output[2*i+1]==right);
        }
    }
    puts("Mixer stereo level and envelope continuation: PASS");
}
