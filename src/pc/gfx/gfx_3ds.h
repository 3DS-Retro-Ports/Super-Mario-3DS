#ifndef GFX_3DS_H
#define GFX_3DS_H

#include "gfx_window_manager_api.h"




typedef enum
{
    GFX_3DS_MODE_NORMAL,
    GFX_3DS_MODE_AA_22,
    GFX_3DS_MODE_WIDE,
    GFX_3DS_MODE_WIDE_AA_12
} Gfx3DSMode;

extern struct GfxWindowManagerAPI gfx_3ds;
extern Gfx3DSMode gGfx3DSMode;
void gfx_3ds_shutdown(void);

#endif
