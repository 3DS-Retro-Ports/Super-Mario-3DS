#include <assert.h>
#include <math.h>
#include "platform/3ds/camera_motion.h"
int main(void) {
    float a=0,b=0;
    for(int i=0;i<30;++i)a=camera_smooth(a,10,4,1.f/30);
    for(int i=0;i<60;++i)b=camera_smooth(b,10,4,1.f/60);
    assert(fabsf(a-b)<.0001f && a>0 && a<10);
    assert(camera_limit_pitch(10)==1.2f && camera_limit_pitch(-10)==-.45f);
    float angle=camera_smooth_angle(3.13f,-3.13f,4,.03f);
    assert(fabsf(angle)>3); /* shortest path crosses the wrap boundary */
    assert(camera_smooth(0,1,4,-1)==0);
}
