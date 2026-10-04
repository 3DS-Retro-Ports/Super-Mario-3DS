#ifndef SM64_3DS_MODS_H
#define SM64_3DS_MODS_H
struct Object;
void mods_init(void);
void mods_shutdown(void);
void mods_update(void);
void mods_object_update(struct Object *object);
void mods_level_init(int type, int level, int area, int node, int arg);
#endif
