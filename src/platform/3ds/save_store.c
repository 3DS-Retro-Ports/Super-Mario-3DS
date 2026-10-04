#include "save_store.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static uint8_t current[512];
static uint32_t generation;
static int active = -1, last_error;
static bool ready;
static uint32_t crc32(const uint8_t *p, size_t size) {
    uint32_t crc = ~0u;
    while (size--) {
        crc ^= *p++;
        for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
static uint32_t get32(const uint8_t *p) {
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t n) {
    for (int i = 0; i < 4; ++i) p[i] = n >> (i * 8);
}
static void path(char *out, size_t size, int index, bool temporary) {
    snprintf(out, size, SAVE_STORE_DIRECTORY "/eeprom.%d%s", index, temporary ? ".tmp" : "");
}
static int read_record(int index, uint8_t *record) {
    char name[128]; path(name, sizeof(name), index, false);
    FILE *f = fopen(name, "rb");
    if (!f) return errno == ENOENT ? 0 : -1;
    size_t count = fread(record, 1, 528, f);
    uint32_t checksum = count == 528 ? get32(record + 12) : 0;
    put32(record + 12, 0);
    bool valid = count == 528 && fgetc(f) == EOF && !ferror(f)
        && !memcmp(record, "SM64SV1\0", 8) && checksum == crc32(record, 528);
    int rc = fclose(f);
    return valid && rc == 0 ? 1 : -1;
}
bool save_store_init(void) {
    ready = false; active = -1; generation = 0; last_error = 0;
    memset(current, 0, sizeof(current));
    const char *dirs[] = {"sdmc:/3ds", "sdmc:/3ds/sm64", SAVE_STORE_DIRECTORY};
    for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
        if (mkdir(dirs[i], 0777) && errno != EEXIST) { last_error = errno; return false; }
    }
    uint8_t a[528], b[528];
    int ra = read_record(0, a), rb = read_record(1, b);
    if (ra == 1 || rb == 1) {
        active = rb == 1 && (ra != 1 || (int32_t)(get32(b + 8) - get32(a + 8)) > 0) ? 1 : 0;
        const uint8_t *record = active ? b : a;
        generation = get32(record + 8);
        memcpy(current, record + 16, sizeof(current));
    } else if (ra < 0 || rb < 0) {
        /* Preserve damaged files for recovery; never silently overwrite them. */
        last_error = EILSEQ;
        return false;
    } else {
        FILE *legacy = fopen("sdmc:/sm64/sm64_save_file.bin", "rb");
        if (legacy) {
            bool valid = fread(current, 1, 512, legacy) == 512 && fgetc(legacy) == EOF && !ferror(legacy);
            if (fclose(legacy)) valid = false;
            if (!valid) { last_error = EILSEQ; return false; }
        } else if (errno != ENOENT) { last_error = errno; return false; }
    }
    ready = true;
    return true;
}
int save_store_read(unsigned offset, void *data, size_t size) {
    if (!ready || offset > 512 || size > 512 - offset || !data) return -1;
    memcpy(data, current + offset, size);
    return 0;
}
int save_store_write(unsigned offset, const void *data, size_t size) {
    if (!ready || offset > 512 || size > 512 - offset || !data) return -1;
    uint8_t record[528] = "SM64SV1";
    put32(record + 8, generation + 1);
    memcpy(record + 16, current, 512);
    memcpy(record + 16 + offset, data, size);
    put32(record + 12, crc32(record, sizeof(record)));
    int next = active == 0 ? 1 : 0;
    char name[128], temporary[128];
    path(name, sizeof(name), next, false); path(temporary, sizeof(temporary), next, true);
    FILE *f = fopen(temporary, "wb");
    if (!f) { last_error = errno; return -1; }
    bool good = fwrite(record, 1, sizeof(record), f) == sizeof(record) && fflush(f) == 0;
    if (good && fsync(fileno(f))) good = false;
    int error = good ? 0 : (errno ? errno : EIO);
    if (fclose(f) && !error) error = errno ? errno : EIO;
    /* FAT may not replace an existing name. Only remove the inactive generation;
     * the active generation remains valid through every interruption point. */
    if (!error && remove(name) && errno != ENOENT) error = errno;
    if (!error && rename(temporary, name)) error = errno;
    if (error) { last_error = error; return -1; }
    memcpy(current, record + 16, 512);
    ++generation; active = next; last_error = 0;
    return 0;
}
int save_store_error(void) { return last_error; }
