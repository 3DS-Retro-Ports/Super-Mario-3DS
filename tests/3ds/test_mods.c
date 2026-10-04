#include <stdbool.h>
/* Native host test: run actual Lua callbacks with a minimal Mario binding. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sm64.h"
#include "game/mario.h"
#include "game/level_update.h"
#include "platform/3ds/mods.h"
#include "platform/3ds/mod_bindings.h"
#include "game/area.h"
#include "game/object_list_processor.h"
#include "model_ids.h"
struct MarioState gMarioStates[1];
static int actions;
struct Object gObjectPool[OBJECT_POOL_CAPACITY];
struct ObjectNode gFreeObjectList;
struct Area area;
struct Area *gCurrentArea = &area;
struct GraphNode *graph_nodes[256];
struct GraphNode **gLoadedGraphNodes = graph_nodes;
const BehaviorScript bhvYellowCoin[]={0},bhvStar[]={0},bhvGoomba[]={0},bhvBobomb[]={0},bhvBlueCoinJumping[]={0};
static bool camera_enabled=true, collision_enabled=true;
void camera_3ds_enable(bool v){camera_enabled=v;}
void camera_3ds_collision(bool v){collision_enabled=v;}
bool camera_3ds_enabled(void){return camera_enabled;}
bool camera_3ds_has_collision(void){return collision_enabled;}
void camera_3ds_rotate(float y,float p,float r){(void)y;(void)p;(void)r;}
struct Object *spawn_object(struct Object *parent,s32 model,const BehaviorScript *behavior) {
    (void)parent;(void)model;(void)behavior;
    struct Object *o=&gObjectPool[1];o->activeFlags=1;mods_object_allocated(o);return o;
}
u32 set_mario_action(struct MarioState *m, u32 action, u32 arg) {
    (void)arg; m->action = action; ++actions; return 1;
}
void mario_set_forward_vel(struct MarioState *m, f32 speed) { m->forwardVel = speed; }
static void script(const char *path, const char *text) {
    FILE *f = fopen(path, "w"); assert(f); assert(fputs(text, f) >= 0); assert(!fclose(f));
}
static void test_mod_folders(void) {
    assert(!mkdir("mods/Zeta", 0700));
    assert(!mkdir("mods/Alpha", 0700));
    assert(!mkdir("mods/Alpha/resources", 0700));
    assert(!mkdir("mods/.disabled", 0700));
    /* Deliberately create paths out of order. Each script depends on its predecessor. */
    script("mods/Zeta/main.lua", "assert(order=='abcd'); order=order..'e'; gMarioStates[0].health=1234");
    script("mods/Alpha/main.lua", "assert(order=='ab'); order=order..'c'");
    script("mods/Alpha/00-helper.lua", "assert(order=='a'); order=order..'b'");
    script("mods/00-root.lua", "order='a'; hook_event(HOOK_UPDATE,function() assert(order=='abcde'); gMarioStates[0].numCoins=17 end)");
    const char *ignored = "gMarioStates[0].health=1; order='unexpected'";
    script("mods/Alpha/readme.txt", ignored);
    script("mods/Alpha/resources/deep.lua", "assert(order=='abc'); order=order..'d'");
    assert(!symlink("../..", "mods/Alpha/resources/parent-cycle"));
    script("mods/.disabled/main.lua", ignored);
    script("mods/Alpha/.hidden.lua", ignored);
    script("outside.lua", ignored);
    assert(!symlink("../outside.lua", "mods/linked.lua"));
    assert(!symlink("Alpha", "mods/linked-directory"));
    mods_init(); mods_update();
    assert(gMarioStates[0].health == 1234 && gMarioStates[0].numCoins == 17);
    mods_shutdown();
    const char *files[] = {"mods/Zeta/main.lua", "mods/Alpha/main.lua", "mods/Alpha/00-helper.lua",
        "mods/00-root.lua", "mods/Alpha/readme.txt", "mods/Alpha/resources/deep.lua", "mods/Alpha/resources/parent-cycle",
        "mods/.disabled/main.lua", "mods/Alpha/.hidden.lua", "outside.lua", "mods/linked.lua", "mods/linked-directory"};
    for (unsigned i=0; i<sizeof(files)/sizeof(*files); ++i) assert(!unlink(files[i]));
    assert(!rmdir("mods/Alpha/resources")); assert(!rmdir("mods/Alpha"));
    assert(!rmdir("mods/Zeta")); assert(!rmdir("mods/.disabled"));
    /* The limit must pick the first 128 names, not filesystem enumeration order. */
    gMarioStates[0].numCoins = 0;
    for (int i=129; i>=0; --i) {
        char path[64], text[128]; snprintf(path,sizeof(path),"mods/%03d.lua",i);
        snprintf(text,sizeof(text),"gMarioStates[0].numCoins=gMarioStates[0].numCoins+1; gMarioStates[0].health=%d",i);
        script(path,text);
    }
    mods_init();
    assert(gMarioStates[0].numCoins == 128 && gMarioStates[0].health == 127);
    mods_shutdown();
    for (int i=0; i<130; ++i) { char path[64];snprintf(path,sizeof(path),"mods/%03d.lua",i);assert(!unlink(path)); }
}
static void test_deep_mod_folders(void) {
    enum { DEPTH = 80 };
    char path[2048] = "mods/Deep Mod";
    size_t lengths[DEPTH + 1];
    assert(!mkdir(path, 0700)); lengths[0] = strlen(path);
    for (int i = 1; i <= DEPTH; ++i) {
        strcat(path, "/nested-level"); assert(!mkdir(path, 0700)); lengths[i] = strlen(path);
    }
    assert(strlen(path) > 512);
    char file[2100]; snprintf(file, sizeof(file), "%s/main.lua", path);
    script(file, "gMarioStates[0].health=4321; hook_event(HOOK_UPDATE,function() gMarioStates[0].numCoins=19 end)");
    mods_init(); mods_update();
    assert(gMarioStates[0].health == 4321 && gMarioStates[0].numCoins == 19);
    mods_shutdown(); assert(!unlink(file));
    for (int i = DEPTH; i >= 0; --i) { path[lengths[i]] = 0; assert(!rmdir(path)); }
}
static void test_mario_hook_lifecycle(void) {
    struct Object *mario = gMarioStates[0].marioObj;
    script("mods/lifecycle.lua", "hook_event(HOOK_MARIO_UPDATE,function(m) mario_set_forward_vel(m, 47) end)");
    gMarioStates[0].marioObj = NULL;
    mods_init(); mods_update(); /* Title screen must not disable this hook. */
    gMarioStates[0].marioObj = mario;
    mario->activeFlags = 0; gMarioStates[0].forwardVel = 0;
    mods_update(); assert(gMarioStates[0].forwardVel == 0);
    mario->activeFlags = 1;
    mods_update(); assert(gMarioStates[0].forwardVel == 47);
    /* Transition through an unloaded area, then resume without reloading Lua. */
    gMarioStates[0].marioObj = NULL; mods_update();
    gMarioStates[0].marioObj = mario; gMarioStates[0].forwardVel = 0;
    mods_update(); assert(gMarioStates[0].forwardVel == 47);
    mods_shutdown(); unlink("mods/lifecycle.lua");
}
static void test_mod_logs(void) {
    script("mods/a-good.lua", "print('test marker', 42); hook_event(HOOK_UPDATE,function() error('hook marker') end)");
    script("mods/b-unsupported.lua", "hook_behavior() -- unsupported coopdx API");
    script("mods/c-syntax.lua", "this is not valid Lua!!!");
    script("mods/d-non-string.lua", "error({})");
    mods_init(); mods_update(); mods_shutdown();
    FILE *log = fopen("mods.log", "r"); assert(log);
    char content[8192]; size_t n = fread(content, 1, sizeof(content)-1, log); content[n] = 0;
    assert(!fclose(log));
    assert(strstr(content, "print: test marker\t42"));
    assert(strstr(content, "LOADED mods/a-good.lua"));
    assert(strstr(content, "FAILED mods/b-unsupported.lua") && strstr(content, "hook_behavior"));
    assert(strstr(content, "FAILED mods/c-syntax.lua"));
    assert(strstr(content, "Lua raised a non-string error"));
    assert(strstr(content, "1 loaded, 3 failed, 1 registered hooks"));
    assert(strstr(content, "Lua hook 0 disabled:") && strstr(content, "hook marker"));
    unlink("mods/a-good.lua"); unlink("mods/b-unsupported.lua");
    unlink("mods/c-syntax.lua"); unlink("mods/d-non-string.lua");
    script("mods/spam.lua", "for i=1,2000 do print(i) end; hook_behavior()");
    mods_init(); mods_shutdown();
    log = fopen("mods.log", "r"); assert(log);
    n = fread(content, 1, sizeof(content)-1, log); content[n] = 0; fclose(log);
    assert(strstr(content, "print limit reached"));
    assert(strstr(content, "FAILED mods/spam.lua") && strstr(content, "0 loaded, 1 failed"));
    unlink("mods/spam.lua");
}
int main(void) {
    FILE *smoke = fopen("platform/3ds/mods/Loader Test/main.lua", "r"); assert(smoke);
    char smoke_script[2048];
    size_t smoke_size = fread(smoke_script, 1, sizeof(smoke_script)-1, smoke);
    assert(!ferror(smoke) && feof(smoke)); smoke_script[smoke_size]=0; fclose(smoke);
    char directory[] = "/tmp/sm64-mod-test-XXXXXX";
    assert(mkdtemp(directory));
    assert(chdir(directory) == 0);
    assert(mkdir("mods", 0700) == 0);
    FILE *file = fopen("mods/test.lua", "w");
    assert(file);
    fputs("local heldMario = gMarioStates[0]\n"
          "hook_event(HOOK_ON_LEVEL_INIT, function(t,l,a,n,arg) assert(l==9 and a==1); heldMario.health=2000 end)\n"
          "camera_config_enable_collisions(false); assert(not camera_config_is_collision_enabled())\n"
          "local spawned=false; local obj\n"
          "hook_event(HOOK_UPDATE,function() if not spawned then obj=spawn_non_sync_object(id_bhvYellowCoin,MODEL_NONE,1,2,3,function(o) o.oAction=2 end); spawned=true; assert(obj.oAction==2) end; heldMario.pos.x=heldMario.pos.x+5 end)\n"
          "hook_event(HOOK_UPDATE,function() if spawned then local x=obj.oPosX end end)\n"
          "hook_event(HOOK_MARIO_UPDATE, function(m) "
          "set_mario_action(m, ACT_JUMP, 0); mario_set_forward_vel(m, 25) end)\n"
          "hook_event(HOOK_UPDATE, function() error('test error') end)\n"
          "hook_event(HOOK_UPDATE, function() while true do end end)\n"
          "hook_event(HOOK_OBJECT_UPDATE, function(o) assert(o.oPosX == 42); mario_set_forward_vel(gMarioStates[0], 31) end)\n", file);
    fclose(file);
    struct Object *object=&gObjectPool[0];object->activeFlags=1; object->oPosX=42;
    gMarioStates[0].marioObj=object;gFreeObjectList.next=&gObjectPool[1].header;
    mods_init();
    mods_level_init(0,9,1,10,0);
    mods_update(); mods_object_update(object);
    assert(gMarioStates[0].health==2000 && gMarioStates[0].pos[0]==5);
    mods_object_allocated(&gObjectPool[1]); /* retained Lua handle must now expire */
    assert(gMarioStates[0].forwardVel == 31);
    mods_update();
    assert(actions == 2);
    assert(gMarioStates[0].pos[0]==10);
    assert(gMarioStates[0].action == ACT_JUMP);
    assert(gMarioStates[0].forwardVel == 25);
    mods_shutdown();
    unlink("mods/test.lua");
    test_mod_folders();
    test_deep_mod_folders();
    test_mario_hook_lifecycle();
    test_mod_logs();
    script("mods/smoke.lua", smoke_script);
    gMarioStates[0].numCoins = 0;
    mods_init(); mods_update(); assert(gMarioStates[0].numCoins == 10);
    gMarioStates[0].numCoins = 11; mods_update(); assert(gMarioStates[0].numCoins == 11);
    mods_shutdown(); unlink("mods/smoke.lua");
    unlink("mods.log");
    rmdir("mods");
    chdir("/"); rmdir(directory);
    puts("Lua callbacks, live bindings, arbitrary folder depth/order, script cap and error limits: PASS");
}
