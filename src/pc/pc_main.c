#include "sm64.h"
#include "game/memory.h"
#include "game/main.h"
#include "audio/external.h"
#include "gfx/gfx_pc.h"
#include "gfx/gfx_3ds.h"
#include "gfx/gfx_citro3d.h"
#include "audio/audio_api.h"
#include "audio/audio_3ds.h"
#include "audio/audio_null.h"
#include "platform/3ds/mods.h"
#include "platform/3ds/mod_bindings.h"
#include "platform/3ds/diagnostics.h"
#include "platform/3ds/save_store.h"

OSMesgQueue gSIEventMesgQueue;
OSMesg gMainReceivedMesg;
s8 gNmiResetBarsTimer;
s8 gResetTimer, gDebugLevelSelect, gShowProfiler, gShowDebugText;
static struct AudioAPI *audio;
static bool ready;
extern void thread5_game_loop(void *);
extern void game_loop_one_iteration(void);
extern void create_next_audio_buffer(s16 *, u32);
extern void audio_3ds_shutdown(void);
extern void gfx_citro3d_shutdown(void);

void dispatch_audio_sptask(struct SPTask *task) { (void)task; }
void set_vblank_handler(s32 index, struct VblankHandler *handler, OSMesgQueue *queue, OSMesg *msg) {
    (void)index; (void)handler; (void)queue; (void)msg;
}
void exec_display_list(struct SPTask *task) {
    if (ready) gfx_run((Gfx *)task->task.t.data_ptr);
}
static void frame(void) {
    mods_begin_frame();
    gfx_start_frame();
    game_loop_one_iteration();
    mods_update();
    if (save_store_error()) diagnostics_errno("Save storage (original files preserved)", save_store_error());
    u32 samples = audio->buffered() < audio->get_desired_buffered() ? 544 : 528;
    s16 buffer[544 * 4];
    for (int i = 0; i < 2; ++i) create_next_audio_buffer(buffer + i * samples * 2, samples);
    audio->play((u8 *)buffer, samples * 8);
    gfx_end_frame();
}
int main(void) {
    static u64 pool[0x200000 / sizeof(u64)];
    gfx_init(&gfx_3ds, &gfx_citro3d_api);
    diagnostics_log("SM64 GAME: runtime assets ready");
    main_pool_init(pool, pool + sizeof(pool) / sizeof(pool[0]));
    gEffectsMemoryPool = mem_pool_init(0x4000, MEMORY_POOL_LEFT);
    if (!gEffectsMemoryPool) diagnostics_fatal("Effects memory pool allocation failed");
    audio = audio_3ds.init() ? &audio_3ds : &audio_null;
    if (!save_store_init()) diagnostics_errno("Save storage initialization", save_store_error());
    audio_init();
    sound_init();
    mods_init();
    thread5_game_loop(NULL);
    diagnostics_log("Game initialization complete; entering main loop");
    ready = true;
    gfx_3ds.main_loop(frame);
    mods_shutdown();
    audio_3ds_shutdown();
    gfx_citro3d_shutdown();
    gfx_3ds_shutdown();
    return 0;
}
