/* Circle Pad Pro IR:USER client. Packet layout and command IDs documented at
 * https://www.3dbrew.org/wiki/Circle_Pad_Pro and IRUSER_Shared_Memory.
 * Polling stays on core 0 and never waits for an accessory response. */
#include <3ds.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include "cpp.h"
#define PACKETS 16
#define RECEIVE_SIZE 0x500
#define RING_SIZE (RECEIVE_SIZE - PACKETS * 8)
static Handle service_handle, memory_handle;
static volatile u8 *memory;
static unsigned packet_index;
static bool calibrated;
static int center_x, center_y;
static float scale_x, scale_y;
static u64 last_request, last_response, last_connect;
static circlePosition position;
static u32 buttons;

static Result submit(void) {
    Result result = svcSendSyncRequest(service_handle);
    return R_FAILED(result) ? result : (Result)getThreadCommandBuffer()[1];
}
static Result command(unsigned id, bool argument, u32 value) {
    u32 *ipc = getThreadCommandBuffer();
    ipc[0] = IPC_MakeHeader(id, argument ? 1 : 0, 0);
    if (argument) ipc[1] = value;
    return submit();
}
static Result send_packet(const u8 *data, size_t size) {
    u32 *ipc = getThreadCommandBuffer();
    ipc[0] = IPC_MakeHeader(0x0d, 1, 2);
    ipc[1] = size;
    ipc[2] = IPC_Desc_StaticBuffer(size, 0);
    ipc[3] = (u32)data;
    return submit();
}
static u8 checksum(const u8 *data, size_t count) {
    u8 crc = 0;
    for (size_t i = 0; i < count; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc << 1) ^ ((crc & 0x80) ? 7 : 0);
    }
    return crc;
}
static size_t receive_packet(u8 *output, size_t capacity) {
    /* Shared memory contains header, receive metadata, packet descriptors,
     * then a circular byte buffer. Copy before releasing ownership. */
    __dmb();
    if (!*(volatile u32 *)(memory + 0x18)) return 0;
    volatile u32 *descriptor = (volatile u32 *)(memory + 0x20 + packet_index * 8);
    u32 offset = descriptor[0], size = descriptor[1];
    u8 packet[96];
    size_t length = 0;
    if (offset < RING_SIZE && size >= 4 && size <= sizeof(packet)) {
        for (u32 i = 0; i < size; ++i)
            packet[i] = memory[0x20 + PACKETS * 8 + (offset + i) % RING_SIZE];
        unsigned header_size = packet[2] & 0x40 ? 4 : 3;
        unsigned payload = packet[2] & 0x3f;
        if (header_size == 4) payload = (payload << 8) | packet[3];
        if (!checksum(packet, size) && payload + header_size + 1 == size && payload <= capacity) {
            memcpy(output, packet + header_size, payload);
            length = payload;
        }
    }
    if (R_SUCCEEDED(command(0x19, true, 1))) packet_index = (packet_index + 1) % PACKETS;
    return length;
}
bool cpp_init(void) {
    if (R_FAILED(srvGetServiceHandle(&service_handle, "ir:USER"))) return false;
    memory = memalign(0x1000, 0x1000);
    if (!memory) { cpp_shutdown(); return false; }
    memset((void *)memory, 0, 0x1000);
    if (R_FAILED(svcCreateMemoryBlock(&memory_handle, (u32)memory, 0x1000, MEMPERM_READ, MEMPERM_READWRITE))) {
        cpp_shutdown(); return false;
    }
    u32 *ipc = getThreadCommandBuffer();
    ipc[0] = IPC_MakeHeader(0x18, 6, 2);
    ipc[1] = 0x1000;
    ipc[2] = RECEIVE_SIZE;
    ipc[3] = PACKETS;
    ipc[4] = 0x200;
    ipc[5] = 4;
    ipc[6] = 4; /* baud rate */
    ipc[7] = IPC_Desc_SharedHandles(1);
    ipc[8] = memory_handle;
    if (R_FAILED(submit())) { cpp_shutdown(); return false; }
    command(6, true, 1); /* accessory device ID */
    last_connect = svcGetSystemTick();
    return true;
}
void cpp_shutdown(void) {
    if (service_handle) {
        command(9, false, 0);
        command(2, false, 0);
        svcCloseHandle(service_handle);
    }
    if (memory_handle) svcCloseHandle(memory_handle);
    free((void *)memory);
    memory = NULL; service_handle = memory_handle = 0;
}
void cpp_poll(circlePosition *out, u32 *keys) {
    if (!memory) return;
    u64 now = svcGetSystemTick();
    __dmb();
    if (memory[8] != 2) {
        calibrated = false;
        buttons = 0; position = (circlePosition){0};
        if (now - last_connect > SYSCLOCK_ARM11) {
            command(9, false, 0);
            command(3, false, 0);
            command(4, false, 0);
            packet_index = 0;
            command(6, true, 1);
            last_connect = now;
        }
    } else {
        for (unsigned count = 0; count < PACKETS; ++count) {
            u8 payload[80];
            size_t length = receive_packet(payload, sizeof(payload));
            if (!length) break;
            if (length == 69 && payload[0] == 0x11 && !calibrated) {
                for (int i = 0; i < 4; ++i) {
                    const u8 *candidate = payload + 5 + i * 16;
                    float x, y;
                    memcpy(&x, candidate + 4, 4); memcpy(&y, candidate + 8, 4);
                    if (checksum(candidate, 16) || !isfinite(x) || !isfinite(y) || x <= 0 || y <= 0 || x > 32 || y > 32) continue;
                    center_x = candidate[1] | ((candidate[2] & 15) << 8);
                    center_y = (candidate[2] >> 4) | (candidate[3] << 4);
                    scale_x = x / 8; scale_y = y / 8;
                    calibrated = true; break;
                }
            } else if (length == 6 && payload[0] == 0x10 && calibrated) {
                int x = payload[1] | ((payload[2] & 15) << 8);
                int y = (payload[2] >> 4) | (payload[3] << 4);
                position.dx = fmaxf(-156, fminf(156, (x - center_x) * scale_x));
                position.dy = fmaxf(-156, fminf(156, (y - center_y) * scale_y));
                buttons = (!(payload[4] & 0x20) ? KEY_ZL : 0)
                    | (!(payload[4] & 0x40) ? KEY_ZR : 0)
                    | (!(payload[4] & 0x80) ? KEY_R : 0);
                last_response = now;
            }
        }
        if (now - last_request > SYSCLOCK_ARM11 / 60) {
            const u8 calibrate[] = {2, 100, 0, 0, 0x40, 0};
            const u8 request[] = {1, 0x20, 0x87};
            send_packet(calibrated ? request : calibrate, calibrated ? sizeof(request) : sizeof(calibrate));
            last_request = now;
        }
    }
    if (now - last_response > SYSCLOCK_ARM11 / 4) { buttons = 0; position = (circlePosition){0}; }
    *out = position; *keys |= buttons;
}

bool cpp_connected(void) {
    return memory && memory[8] == 2 && calibrated && last_response
        && svcGetSystemTick() - last_response <= SYSCLOCK_ARM11 / 4;
}
