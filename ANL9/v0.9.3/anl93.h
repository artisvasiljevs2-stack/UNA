#ifndef ANL93_H
#define ANL93_H

#include <stddef.h>
#include <stdint.h>

#define ANL93_MAGIC "ANL9"
#define ANL93_VERSION 0x0902u /* wire-compatible with 0.9.2 */
#define ANL93_BLOCK_SIZE 65536u

#define ANL93_RAW 1u
#define ANL93_BLOCK_REF 4u
#define ANL93_MINIBLOCK 6u

#define ANL93_MB_DATA 1u
#define ANL93_MB_REF 2u
#define ANL93_MB_REPEAT 3u

#pragma pack(push, 1)
typedef struct {
    char magic[4]; uint16_t version, flags; uint32_t block_size, block_count;
    uint32_t table_count, record_count; uint64_t original_size;
    uint32_t final_crc32, header_crc32;
} ANL93Header;
typedef struct {
    uint8_t type, flags; uint16_t header_size;
    uint32_t block_index, payload_size, crc32;
} ANL93Record;
typedef struct { uint32_t output_size, operation_count; } ANL93MiniHeader;
typedef struct {
    uint8_t opcode, reserved[3]; uint32_t length, argument0, argument1;
} ANL93MiniOp;
#pragma pack(pop)

uint32_t anl93_crc32(const void *data, size_t size);
#endif
