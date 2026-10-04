#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "sm64.h"
#include "game/mario.h"
#include "game/level_update.h"
#include "game/object_list_processor.h"
#include "mods.h"
#include "mod_bindings.h"
#include <sys/stat.h>

#define MAX_HOOKS 64
#define MAX_MOD_SCRIPTS 128
#define LUA_MEMORY_LIMIT (2 * 1024 * 1024)
#define HOOK_UPDATE 0
#define HOOK_MARIO_UPDATE 1
/* Local extension. This is not a coopdx hook ID. */
#define HOOK_OBJECT_UPDATE 2
#define HOOK_ON_LEVEL_INIT 3
#ifndef SM64_MOD_DIRECTORY
#define SM64_MOD_DIRECTORY "sdmc:/sm64/mods"
#endif
#ifndef SM64_MOD_LOG
#define SM64_MOD_LOG "sdmc:/sm64/mods.log"
#endif
struct ModHook { int event, ref; };
static struct ModHook hooks[MAX_HOOKS];
static int hook_count;
static lua_State *state;
static size_t allocated;
static int instruction_budget;
static FILE *mod_log;
static unsigned log_count, print_count;
static bool log_dirty;
/* Bounded session log: script print loops cannot fill the SD card. */
static void log_message(const char *format, ...) {
    if (log_count > 512) return;
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (log_count++ == 512) strcpy(message, "Mod log limit reached (512 messages)");
    for (char *p = message; *p; ++p)
        if ((unsigned char)*p < 32 && *p != '\n' && *p != '\t') *p = '?';
    fprintf(stderr, "%s\n", message);
    if (mod_log) { fprintf(mod_log, "%s\n", message); log_dirty = true; }
}
static const char *lua_error_message(void) {
    const char *message = lua_type(state, -1) == LUA_TSTRING ? lua_tostring(state, -1) : NULL;
    return message ? message : "Lua raised a non-string error";
}
static void flush_log(void) {
    if (mod_log && log_dirty) { fflush(mod_log); log_dirty = false; }
}
static int mod_print(lua_State *L) {
    if (print_count >= 64) return 0;
    if (++print_count == 64) { log_message("Lua print limit reached (63 messages)"); return 0; }
    char message[768];
    size_t used = 0;
    int count = lua_gettop(L);
    for (int i = 1; i <= count && used < sizeof(message) - 1; ++i) {
        size_t length;
        const char *text = luaL_tolstring(L, i, &length);
        if (i > 1 && used < sizeof(message) - 1) message[used++] = '\t';
        size_t available = sizeof(message) - 1 - used;
        if (length > available) length = available;
        memcpy(message + used, text, length); used += length;
        lua_pop(L, 1);
    }
    message[used] = 0;
    log_message("print: %s", message);
    return 0;
}

static void *allocate(void *ud, void *ptr, size_t old_size, size_t size) {
    (void)ud;
    if (!ptr) old_size = 0;
    if (!size) { allocated -= old_size; free(ptr); return NULL; }
    if (size > LUA_MEMORY_LIMIT - (allocated - old_size)) return NULL;
    void *result = realloc(ptr, size);
    if (result) allocated = allocated - old_size + size;
    return result;
}
static void limit(lua_State *L, lua_Debug *debug) {
    (void)debug;
    instruction_budget -= 1000;
    if (instruction_budget <= 0) luaL_error(L, "mod instruction budget exceeded");
}
static int hook_event(lua_State *L) {
    int event = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (event < 0 || event > 3) return luaL_error(L, "unsupported hook event");
    if (hook_count == MAX_HOOKS) return luaL_error(L, "too many mod hooks");
    lua_pushvalue(L, 2);
    hooks[hook_count++] = (struct ModHook){event, luaL_ref(L, LUA_REGISTRYINDEX)};
    return 0;
}
static int set_action(lua_State *L) {
    mods_check_mario(L, 1);
    u32 action = (u32)luaL_checkinteger(L, 2);
    u32 arg = (u32)luaL_optinteger(L, 3, 0);
    if (!gMarioStates[0].marioObj) return luaL_error(L, "Mario is not active");
    lua_pushinteger(L, set_mario_action(&gMarioStates[0], action, arg));
    return 1;
}
static int set_velocity(lua_State *L) {
    mods_check_mario(L, 1);
    float speed = luaL_checknumber(L, 2);
    if (!isfinite(speed) || fabsf(speed) > 1000) return luaL_error(L, "invalid velocity");
    if (!gMarioStates[0].marioObj) return luaL_error(L, "Mario is not active");
    mario_set_forward_vel(&gMarioStates[0], speed);
    return 0;
}
static void constant(const char *name, int value) {
    lua_pushinteger(state, value); lua_setglobal(state, name);
}
static void call_hook(int i, int args) {
    instruction_budget = 100000;
    lua_sethook(state, limit, LUA_MASKCOUNT, 1000);
    if (lua_pcall(state, args, 0, 0) != LUA_OK) {
        lua_Debug origin = {0};
        lua_rawgeti(state, LUA_REGISTRYINDEX, hooks[i].ref);
        lua_getinfo(state, ">S", &origin);
        log_message("Lua hook %d disabled: %s [%s]", hooks[i].event, lua_error_message(), origin.short_src);
        flush_log();
        lua_pop(state, 1);
        luaL_unref(state, LUA_REGISTRYINDEX, hooks[i].ref);
        hooks[i].ref = LUA_NOREF;
    }
    lua_sethook(state, NULL, 0, 0);
}
static int update_inner(lua_State *unused) {
    (void)unused;
    int count = hook_count;
    for (int i = 0; i < count; ++i) {
        if (hooks[i].ref == LUA_NOREF || hooks[i].event >= HOOK_OBJECT_UPDATE) continue;
        /* Title/menu frames have no Mario. Calling gameplay hooks here would
         * permanently disable otherwise valid scripts before a level starts. */
        if (hooks[i].event == HOOK_MARIO_UPDATE &&
            (!gMarioStates[0].marioObj || !gMarioStates[0].marioObj->activeFlags)) continue;
        lua_rawgeti(state, LUA_REGISTRYINDEX, hooks[i].ref);
        int args = 0;
        if (hooks[i].event == HOOK_MARIO_UPDATE) {
            mods_push_mario(state); args = 1;
        }
        call_hook(i, args);
    }
    return 0;
}
static struct Object *callback_object;
static int object_inner(lua_State *unused) {
    (void)unused;
    struct Object *object = callback_object;
    int count = hook_count;
    for (int i = 0; i < count; ++i) {
        if (hooks[i].ref == LUA_NOREF || hooks[i].event != HOOK_OBJECT_UPDATE) continue;
        lua_rawgeti(state, LUA_REGISTRYINDEX, hooks[i].ref);
        mods_push_object(state, object);
        call_hook(i, 1);
    }
    return 0;
}
/* Protect table allocation as well as the script itself against Lua OOM. */
static void dispatch(lua_CFunction function) {
    if (!state) return;
    lua_pushcfunction(state, function);
    if (lua_pcall(state, 0, 0, 0) != LUA_OK) {
        log_message("Lua disabled: %s", lua_error_message());
        mods_shutdown();
    }
    flush_log();
}
static bool level_pending;
static int level_inner(lua_State *unused);
void mods_update(void) {
    if (level_pending) { level_pending = false; dispatch(level_inner); }
    dispatch(update_inner);
}
static int level_args[5];
static int level_inner(lua_State *unused) {
    (void)unused;
    int count = hook_count;
    for (int i = 0; i < count; ++i) {
        if (hooks[i].ref == LUA_NOREF || hooks[i].event != HOOK_ON_LEVEL_INIT) continue;
        lua_rawgeti(state, LUA_REGISTRYINDEX, hooks[i].ref);
        for (int j = 0; j < 5; ++j) lua_pushinteger(state, level_args[j]);
        call_hook(i, 5);
    }
    return 0;
}
void mods_level_init(int type, int level, int area, int node, int arg) {
    level_args[0]=type; level_args[1]=level; level_args[2]=area; level_args[3]=node; level_args[4]=arg;
    level_pending = true;
}
void mods_object_update(struct Object *object) {
    callback_object = object;
    dispatch(object_inner);
    callback_object = NULL;
}
/* Keep the lexically first paths even when readdir order differs between SDs.
 * An explicit work list avoids C stack growth and holds only one DIR open. */
struct ScriptPaths { char *paths[MAX_MOD_SCRIPTS]; size_t count; bool truncated; };
struct ModDirectory { char *path; struct ModDirectory *next; };
static char *child_path(const char *directory, const char *name) {
    size_t a = strlen(directory), b = strlen(name);
    if (a > SIZE_MAX - b - 2) return NULL;
    char *path = malloc(a + b + 2);
    if (path) {
        memcpy(path, directory, a);
        path[a] = '/';
        memcpy(path + a + 1, name, b + 1);
    }
    return path;
}
static void queue_directory(struct ModDirectory **pending, char *path) {
    struct ModDirectory *entry = malloc(sizeof(*entry));
    if (!entry) {
        log_message("Cannot allocate mod directory: %s", path);
        free(path);
        return;
    }
    *entry = (struct ModDirectory){path, *pending};
    *pending = entry;
}
static void collect_script_path(struct ScriptPaths *scripts, char *path) {
    size_t position = 0;
    while (position < scripts->count && strcmp(scripts->paths[position], path) < 0) ++position;
    if (scripts->count == MAX_MOD_SCRIPTS) {
        scripts->truncated = true;
        if (position == MAX_MOD_SCRIPTS) { free(path); return; }
        free(scripts->paths[--scripts->count]);
    }
    memmove(scripts->paths + position + 1, scripts->paths + position,
            (scripts->count - position) * sizeof(*scripts->paths));
    scripts->paths[position] = path;
    ++scripts->count;
}
static void collect_scripts(struct ScriptPaths *scripts, const char *directory) {
    struct ModDirectory *pending = NULL;
    char *root = strdup(directory);
    if (!root) { log_message("Cannot allocate mod root path"); return; }
    queue_directory(&pending, root);
    while (pending) {
        struct ModDirectory *current = pending;
        pending = current->next;
        DIR *dir = opendir(current->path);
        if (!dir) {
            log_message("Cannot scan mod directory %s: %s", current->path, strerror(errno));
            free(current->path); free(current);
            continue;
        }
        struct dirent *entry;
        for (;;) {
            errno = 0;
            entry = readdir(dir);
            if (!entry) {
                if (errno) log_message("Cannot read mod directory %s: %s", current->path, strerror(errno));
                break;
            }
            if (entry->d_name[0] == '.') continue;
            char *path = child_path(current->path, entry->d_name);
            if (!path) {
                log_message("Cannot allocate mod path: %s/%s", current->path, entry->d_name);
                continue;
            }
            struct stat info;
            /* Ignore symbolic links, including links back to parent directories. */
            if (lstat(path, &info)) {
                log_message("Cannot stat mod path %s: %s", path, strerror(errno));
                free(path); continue;
            }
            if (S_ISDIR(info.st_mode)) { queue_directory(&pending, path); continue; }
            size_t n = strlen(entry->d_name);
            if (!S_ISREG(info.st_mode) || n <= 4 || strcmp(entry->d_name + n - 4, ".lua")) {
                free(path); continue;
            }
            if (info.st_size < 0 || info.st_size > 256 * 1024) {
                log_message("Mod script exceeds 256 KiB: %s", path);
                free(path); continue;
            }
            collect_script_path(scripts, path);
        }
        closedir(dir);
        free(current->path); free(current);
    }
}
static int initialize_lua(lua_State *unused) {
    (void)unused;
    luaL_requiref(state, "_G", luaopen_base, 1); lua_pop(state, 1);
    luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1); lua_pop(state, 1);
    luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1); lua_pop(state, 1);
    luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1); lua_pop(state, 1);
    const char *blocked[] = {"dofile", "loadfile", "load", "pcall", "xpcall", NULL};
    for (int i = 0; blocked[i]; ++i) { lua_pushnil(state); lua_setglobal(state, blocked[i]); }
    lua_pushcfunction(state, mod_print); lua_setglobal(state, "print");
    lua_pushcfunction(state, hook_event); lua_setglobal(state, "hook_event");
    lua_pushcfunction(state, set_action); lua_setglobal(state, "set_mario_action");
    lua_pushcfunction(state, set_velocity); lua_setglobal(state, "mario_set_forward_vel");
    constant("ACT_IDLE", ACT_IDLE);
    constant("ACT_JUMP", ACT_JUMP);
    constant("ACT_WALKING", ACT_WALKING);
    constant("HOOK_UPDATE", HOOK_UPDATE);
    constant("HOOK_MARIO_UPDATE", HOOK_MARIO_UPDATE);
    constant("HOOK_OBJECT_UPDATE", HOOK_OBJECT_UPDATE);
    constant("HOOK_ON_LEVEL_INIT", HOOK_ON_LEVEL_INIT);
    constant("SM64_3DS_API_VERSION", 2);
    mods_bindings_init(state);
    return 0;
}
void mods_init(void) {
    mods_shutdown();
    log_count = print_count = 0;
    log_dirty = false;
    mod_log = fopen(SM64_MOD_LOG, "w");
    int log_error = errno;
    log_message("SM64 3DS Lua API 2 (coopdx subset); scanning %s", SM64_MOD_DIRECTORY);
    if (!mod_log) log_message("Cannot open mod log %s: %s", SM64_MOD_LOG, strerror(log_error));
    state = lua_newstate(allocate, NULL);
    if (!state) { log_message("Cannot allocate Lua state"); mods_shutdown(); return; }
    lua_pushcfunction(state, initialize_lua);
    if (lua_pcall(state, 0, 0, 0) != LUA_OK) {
        log_message("Lua initialization failed: %s", lua_error_message());
        mods_shutdown(); return;
    }
    struct ScriptPaths scripts = {0};
    unsigned loaded = 0, failed = 0;
    collect_scripts(&scripts, SM64_MOD_DIRECTORY);
    if (scripts.truncated) log_message("Mod script limit (%d) reached; loading first paths alphabetically", MAX_MOD_SCRIPTS);
    for (size_t i = 0; i < scripts.count; ++i) {
        const char *path = scripts.paths[i];
        int before = hook_count;
        instruction_budget = 100000;
        lua_sethook(state, limit, LUA_MASKCOUNT, 1000);
        int result = luaL_loadfilex(state, path, "t");
        if (result == LUA_OK) result = lua_pcall(state, 0, 0, 0);
        lua_sethook(state, NULL, 0, 0);
        if (result != LUA_OK) {
            ++failed;
            log_message("FAILED %s: %s", path, lua_error_message());
            lua_pop(state, 1);
            while (hook_count > before) luaL_unref(state, LUA_REGISTRYINDEX, hooks[--hook_count].ref);
        }
        else { ++loaded; log_message("LOADED %s", path); }
        flush_log();
        free(scripts.paths[i]);
    }
    log_message("Mod summary: %u loaded, %u failed, %d registered hooks", loaded, failed, hook_count);
    if (!scripts.count) log_message("No Lua scripts found; unzip mods under " SM64_MOD_DIRECTORY "/My Mod/main.lua");
    flush_log();
}
void mods_shutdown(void) {
    if (state) lua_close(state);
    if (mod_log) { fclose(mod_log); mod_log = NULL; }
    state = NULL; hook_count = 0; level_pending = false;
    mods_begin_frame();
}
