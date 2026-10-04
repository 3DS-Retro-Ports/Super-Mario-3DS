#ifndef SM64_MOD_BINDINGS_H
#define SM64_MOD_BINDINGS_H
#include "lua.h"
struct Object;
struct MarioState;
void mods_bindings_init(lua_State *L);
void mods_push_mario(lua_State *L);
void mods_push_object(lua_State *L, struct Object *object);
struct MarioState *mods_check_mario(lua_State *L, int arg);
void mods_object_allocated(struct Object *object);
void mods_begin_frame(void);
#endif
