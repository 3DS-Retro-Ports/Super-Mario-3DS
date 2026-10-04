#include <math.h>
#include <stdint.h>
#include <string.h>
#include "lauxlib.h"
#include "sm64.h"
#include "game/mario.h"
#include "game/level_update.h"
#include "game/object_list_processor.h"
#include "game/object_helpers.h"
#include "game/spawn_object.h"
#include "behavior_data.h"
#include "model_ids.h"
#include "camera.h"
#include "mod_bindings.h"

#define META "sm64.live"
enum { MARIO, POS, VEL, ANGLES, OBJECT };
struct Ref { unsigned kind, slot; uint32_t generation; };
static uint32_t generations[OBJECT_POOL_CAPACITY];
static unsigned spawned;
void mods_begin_frame(void) { spawned = 0; }
void mods_object_allocated(struct Object *o) {
    size_t index = o - gObjectPool;
    if (index < OBJECT_POOL_CAPACITY) ++generations[index];
}
static void push_ref(lua_State *L, unsigned kind, unsigned slot) {
    struct Ref *ref = lua_newuserdata(L, sizeof(*ref));
    *ref = (struct Ref){kind, slot, slot < OBJECT_POOL_CAPACITY ? generations[slot] : 0};
    luaL_setmetatable(L, META);
}
void mods_push_mario(lua_State *L) { push_ref(L, MARIO, 0); }
void mods_push_object(lua_State *L, struct Object *o) {
    if (!o) { lua_pushnil(L); return; }
    uintptr_t address = (uintptr_t)o, base = (uintptr_t)gObjectPool;
    if (address < base || address - base >= sizeof(struct Object) * OBJECT_POOL_CAPACITY
        || (address - base) % sizeof(struct Object)) { lua_pushnil(L); return; }
    push_ref(L, OBJECT, (address - base) / sizeof(struct Object));
}
struct MarioState *mods_check_mario(lua_State *L, int arg) {
    struct Ref *ref = luaL_checkudata(L, arg, META);
    if (ref->kind != MARIO || !gMarioStates[0].marioObj) luaL_error(L, "local Mario is not active");
    return &gMarioStates[0];
}
static struct Object *object_arg(lua_State *L, struct Ref *ref) {
    if (ref->slot >= OBJECT_POOL_CAPACITY || ref->generation != generations[ref->slot]
        || !gObjectPool[ref->slot].activeFlags) luaL_error(L, "object reference expired");
    return &gObjectPool[ref->slot];
}
static double finite_number(lua_State *L, int arg, double limit) {
    double value = luaL_checknumber(L, arg);
    if (!isfinite(value) || fabs(value) > limit) luaL_error(L, "field value out of range");
    return value;
}
#define MARIO_FIELDS(X) X(action) X(actionArg) X(actionState) X(actionTimer) X(flags) X(health) X(numStars) X(numCoins) X(forwardVel)
#define OBJECT_FIELDS(X) X(oPosX) X(oPosY) X(oPosZ) X(oVelX) X(oVelY) X(oVelZ) X(oForwardVel) X(oAction) X(oTimer) X(oFaceAngleYaw) X(oFaceAnglePitch)
static int get_field(lua_State *L) {
    struct Ref *ref = luaL_checkudata(L, 1, META);
    const char *key = luaL_checkstring(L, 2);
    struct MarioState *m = &gMarioStates[0];
    if (ref->kind == MARIO) {
        if (!strcmp(key, "playerIndex")) { lua_pushinteger(L, 0); return 1; }
        if (!strcmp(key, "pos")) { push_ref(L, POS, 0); return 1; }
        if (!strcmp(key, "vel")) { push_ref(L, VEL, 0); return 1; }
        if (!strcmp(key, "faceAngle")) { push_ref(L, ANGLES, 0); return 1; }
        if (!strcmp(key, "marioObj")) { mods_push_object(L, m->marioObj); return 1; }
#define GET(name) if (!strcmp(key, #name)) { lua_pushnumber(L, m->name); return 1; }
        MARIO_FIELDS(GET)
#undef GET
    } else if (ref->kind == OBJECT) {
        struct Object *o = object_arg(L, ref);
#define GET(name) if (!strcmp(key, #name)) { lua_pushnumber(L, o->name); return 1; }
        OBJECT_FIELDS(GET)
#undef GET
    } else {
        int axis = !strcmp(key,"x") ? 0 : !strcmp(key,"y") ? 1 : !strcmp(key,"z") ? 2 : -1;
        if (axis >= 0) {
            lua_pushnumber(L, ref->kind == POS ? m->pos[axis] : ref->kind == VEL ? m->vel[axis] : m->faceAngle[axis]);
            return 1;
        }
    }
    lua_pushnil(L); return 1;
}
static int set_field(lua_State *L) {
    struct Ref *ref = luaL_checkudata(L, 1, META);
    const char *key = luaL_checkstring(L, 2);
    double value = finite_number(L, 3, 2147483647.0);
    struct MarioState *m = &gMarioStates[0];
    if (ref->kind == OBJECT) {
        struct Object *o = object_arg(L, ref);
#define SET(name) if (!strcmp(key, #name)) { o->name = value; return 0; }
        OBJECT_FIELDS(SET)
#undef SET
    } else if (!m->marioObj) return luaL_error(L, "Mario is not active");
    else if (ref->kind == MARIO) {
        if (!strcmp(key,"forwardVel")) { mario_set_forward_vel(m, finite_number(L,3,1000)); return 0; }
        if (!strcmp(key,"action")) { set_mario_action(m, (u32)luaL_checkinteger(L,3), 0); return 0; }
        if (!strcmp(key,"health")) { m->health = (s16)finite_number(L,3,32767); return 0; }
        if (!strcmp(key,"numCoins")) { m->numCoins = (s16)finite_number(L,3,32767); return 0; }
        if (!strcmp(key,"actionArg")) { m->actionArg = (u32)luaL_checkinteger(L,3); return 0; }
    } else {
        int axis = !strcmp(key,"x") ? 0 : !strcmp(key,"y") ? 1 : !strcmp(key,"z") ? 2 : -1;
        if (axis >= 0) {
            value = finite_number(L,3,32767);
            if (ref->kind == POS) m->pos[axis] = value;
            else if (ref->kind == VEL) m->vel[axis] = value;
            else m->faceAngle[axis] = (s16)value;
            return 0;
        }
    }
    return luaL_error(L, "unsupported or read-only field: %s", key);
}
static const struct { const char *name; const BehaviorScript *script; } behaviors[] = {
    {"id_bhvYellowCoin", bhvYellowCoin}, {"id_bhvStar", bhvStar},
    {"id_bhvGoomba", bhvGoomba}, {"id_bhvBobomb", bhvBobomb},
    {"id_bhvBlueCoinJumping", bhvBlueCoinJumping},
};
static int spawn(lua_State *L) {
    int behavior = luaL_checkinteger(L,1), model = luaL_checkinteger(L,2);
    if (behavior < 0 || behavior >= (int)(sizeof(behaviors)/sizeof(*behaviors))) return luaL_error(L,"unsupported behavior ID");
    if (model < 0 || model >= 256) return luaL_error(L,"invalid model ID");
    float x = finite_number(L,3,32767), y = finite_number(L,4,32767), z = finite_number(L,5,32767);
    if (!lua_isnoneornil(L,6)) luaL_checktype(L,6,LUA_TFUNCTION);
    if (!gMarioStates[0].marioObj || !gCurrentArea) return luaL_error(L,"no active level/Mario");
    if (model && !gLoadedGraphNodes[model]) return luaL_error(L,"model is not loaded in this level");
    if (spawned >= 32 || !gFreeObjectList.next) { lua_pushnil(L); return 1; }
    struct Object *o = spawn_object(gMarioStates[0].marioObj, model, behaviors[behavior].script);
    if (!o) { lua_pushnil(L); return 1; }
    ++spawned; o->oPosX=x; o->oPosY=y; o->oPosZ=z;
    mods_push_object(L,o);
    if (lua_isfunction(L,6)) {
        lua_pushvalue(L,6); lua_pushvalue(L,-2); lua_call(L,1,0);
    }
    return 1;
}
static int enable_camera(lua_State *L) { camera_3ds_enable(lua_toboolean(L,1)); return 0; }
static int enable_collision(lua_State *L) { camera_3ds_collision(lua_toboolean(L,1)); return 0; }
static int camera_enabled(lua_State *L) { lua_pushboolean(L,camera_3ds_enabled()); return 1; }
static int collision_enabled(lua_State *L) { lua_pushboolean(L,camera_3ds_has_collision()); return 1; }
static int rotate_camera(lua_State *L) {
    camera_3ds_rotate(finite_number(L,1,100000), finite_number(L,2,100000), finite_number(L,3,1400)); return 0;
}
void mods_bindings_init(lua_State *L) {
    luaL_newmetatable(L,META);
    lua_pushcfunction(L,get_field); lua_setfield(L,-2,"__index");
    lua_pushcfunction(L,set_field); lua_setfield(L,-2,"__newindex");
    lua_pushliteral(L,"protected"); lua_setfield(L,-2,"__metatable"); lua_pop(L,1);
    lua_newtable(L); mods_push_mario(L); lua_rawseti(L,-2,0); lua_setglobal(L,"gMarioStates");
    const luaL_Reg functions[] = {
        {"spawn_non_sync_object",spawn}, {"camera_config_enable_free_cam",enable_camera},
        {"camera_config_enable_collisions",enable_collision}, {"camera_config_is_free_cam_enabled",camera_enabled},
        {"camera_config_is_collision_enabled",collision_enabled}, {"sm64_3ds_camera_set",rotate_camera}, {NULL,NULL}
    };
    for (const luaL_Reg *f=functions; f->name; ++f) { lua_pushcfunction(L,f->func); lua_setglobal(L,f->name); }
    for (unsigned i=0; i<sizeof(behaviors)/sizeof(*behaviors); ++i) { lua_pushinteger(L,i); lua_setglobal(L,behaviors[i].name); }
#define CONSTANT(name) lua_pushinteger(L,name); lua_setglobal(L,#name)
    CONSTANT(MODEL_NONE); CONSTANT(MODEL_YELLOW_COIN); CONSTANT(MODEL_BLUE_COIN); CONSTANT(MODEL_STAR);
    CONSTANT(MODEL_GOOMBA); CONSTANT(MODEL_BLACK_BOBOMB);
#undef CONSTANT
}
