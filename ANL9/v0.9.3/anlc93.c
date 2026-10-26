#include "anl93.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint8_t *data; size_t size, capacity; } Buffer;
typedef struct { uint8_t *data; uint32_t size; uint8_t type; } Encoded;

uint32_t anl93_crc32(const void *data, size_t n) {
    const uint8_t *p = data; uint32_t c = ~0u;
    for (size_t i = 0; i < n; ++i) { c ^= p[i]; for (int b = 0; b < 8; ++b) c = (c & 1u) ? (c >> 1) ^ 0xedb88320u : c >> 1; }
    return ~c;
}
static int put(Buffer *b, const void *p, size_t n) {
    if (n > SIZE_MAX - b->size) return 0;
    if (b->size + n > b->capacity) { size_t c = b->capacity ? b->capacity : 256u; while (c < b->size + n) { if (c > SIZE_MAX / 2u) return 0; c *= 2u; } uint8_t *q = realloc(b->data, c); if (!q) return 0; b->data = q; b->capacity = c; }
    memcpy(b->data + b->size, p, n); b->size += n; return 1;
}
static int op(Buffer *b, uint8_t opcode, uint32_t length, uint32_t a, uint32_t z) { ANL93MiniOp x = {opcode,{0,0,0},length,a,z}; return put(b, &x, sizeof x); }
static uint32_t h4(const uint8_t *p) { return ((uint32_t)p[0] * 251u + (uint32_t)p[1] * 17u + (uint32_t)p[2] * 7u + p[3]) & 65535u; }
static size_t match_len(const uint8_t *d, size_t n, size_t a, size_t b, size_t maximum) { size_t k = 0; while (k < maximum && a + k < n && b + k < n && d[a + k] == d[b + k]) ++k; return k; }
static int same_block(const uint8_t *d, size_t n, size_t a, size_t b, size_t len) { return a + len <= n && b + len <= n && memcmp(d + a, d + b, len) == 0; }
static int flush_literal(Buffer *payload, const uint8_t *source, size_t start, size_t end, uint32_t *count) { if (end == start) return 1; if (end - start > UINT32_MAX) return 0; return op(payload, ANL93_MB_DATA, (uint32_t)(end - start), 0, 0) && put(payload, source + start, end - start) && (++*count, 1); }

int main(int argc, char **argv) {
    if (argc != 3) { printf("ANLC 0.9.3\nUsage: %s <input> <output>\n", argv[0]); return 1; }
    FILE *in = fopen(argv[1], "rb"); if (!in) { fputs("ERROR: cannot open input.\n", stderr); return 1; }
    if (fseek(in, 0, SEEK_END) || ftell(in) < 0) { fclose(in); return 1; }
    long end = ftell(in); rewind(in); size_t n = (size_t)end; uint8_t *d = n ? malloc(n) : NULL;
    if ((n && (!d || fread(d, 1, n, in) != n)) || fclose(in)) { free(d); fputs("ERROR: cannot read input.\n", stderr); return 1; }
    uint32_t blocks = n ? (uint32_t)((n + ANL93_BLOCK_SIZE - 1u) / ANL93_BLOCK_SIZE) : 0;
    Encoded *records = calloc(blocks ? blocks : 1u, sizeof *records); uint32_t *last = malloc(65536u * sizeof *last);
    if (!records || !last) { free(records); free(last); free(d); return 1; }
    for (size_t i = 0; i < 65536u; ++i) last[i] = UINT32_MAX;
    uint32_t mini = 0, refs = 0, repeats = 0, blockrefs = 0, raws = 0;
    for (uint32_t bi = 0; bi < blocks; ++bi) {
        size_t base = (size_t)bi * ANL93_BLOCK_SIZE, len = n - base; if (len > ANL93_BLOCK_SIZE) len = ANL93_BLOCK_SIZE;
        uint32_t exact = UINT32_MAX;
        for (uint32_t p = 0; p < bi; ++p) { size_t poff = (size_t)p * ANL93_BLOCK_SIZE; size_t plen = n - poff; if (plen > ANL93_BLOCK_SIZE) plen = ANL93_BLOCK_SIZE; if (plen == len && same_block(d, n, base, poff, len)) { exact = p; break; } }
        if (exact != UINT32_MAX) { records[bi].data = malloc(4); if (!records[bi].data) goto oom; memcpy(records[bi].data, &exact, 4); records[bi].size = 4; records[bi].type = ANL93_BLOCK_REF; ++blockrefs; goto index; }
        Buffer b = {0}; ANL93MiniHeader mh = {(uint32_t)len,0}; if (!put(&b, &mh, sizeof mh)) { free(b.data); goto oom; }
        size_t i = 0, literal = 0; uint32_t count = 0;
        while (i < len) {
            size_t run = 1; while (i + run < len && d[base + i + run] == d[base + i] && run < UINT32_MAX) ++run;
            size_t best = 0; uint32_t source = 0;
            if (i + 4 <= len) {
                uint32_t slot = h4(d + base + i), prior = last[slot];
                if (prior != UINT32_MAX && prior < base && memcmp(d + prior, d + base + i, 4) == 0) {
                    size_t maximum = len - i;
                    /* A reference may read only bytes decoded before this block. */
                    if (maximum > base - prior) maximum = base - prior;
                    best = match_len(d, n, prior, base + i, maximum);
                    source = prior;
                }
            }
            if (run >= 17 || best >= 17) {
                if (!flush_literal(&b, d + base, literal, i, &count)) { free(b.data); goto oom; }
                if (run >= best) { if (!op(&b, ANL93_MB_REPEAT, (uint32_t)run, d[base + i], 0)) { free(b.data); goto oom; } ++repeats; i += run; }
                else { if (!op(&b, ANL93_MB_REF, (uint32_t)best, source, 0)) { free(b.data); goto oom; } ++refs; i += best; }
                ++count; literal = i;
            } else ++i;
        }
        if (!flush_literal(&b, d + base, literal, len, &count)) { free(b.data); goto oom; }
        memcpy(b.data, &(ANL93MiniHeader){(uint32_t)len,count}, sizeof mh);
        /* Compare complete record costs: the archive/record headers are shared
           by both choices, but including them here prevents payload-only policy
           drifting if either representation later adds per-record metadata. */
        uint64_t mini_cost = (uint64_t)sizeof(ANL93Record) + b.size;
        uint64_t raw_cost = (uint64_t)sizeof(ANL93Record) + len;
        if (count && sizeof(ANL93Header) + mini_cost < sizeof(ANL93Header) + raw_cost) { records[bi].data = b.data; records[bi].size = (uint32_t)b.size; records[bi].type = ANL93_MINIBLOCK; ++mini; } else { free(b.data); records[bi].data = malloc(len ? len : 1u); if (!records[bi].data) goto oom; memcpy(records[bi].data, d + base, len); records[bi].size = (uint32_t)len; records[bi].type = ANL93_RAW; ++raws; }
index:
        for (size_t k = 0; k + 4 <= len; ++k) last[h4(d + base + k)] = (uint32_t)(base + k);
    }
    FILE *out = fopen(argv[2], "wb"); if (!out) { fputs("ERROR: cannot create output.\n", stderr); goto oom; }
    ANL93Header h = {{'A','N','L','9'},ANL93_VERSION,0,ANL93_BLOCK_SIZE,blocks,0,blocks,n,anl93_crc32(d,n),0}; h.header_crc32 = anl93_crc32(&h, sizeof h);
    if (fwrite(&h,sizeof h,1,out)!=1) { fclose(out); goto oom; }
    for (uint32_t i = 0; i < blocks; ++i) { ANL93Record r={records[i].type,0,sizeof r,i,records[i].size,anl93_crc32(records[i].data,records[i].size)}; if (fwrite(&r,sizeof r,1,out)!=1 || (r.payload_size && fwrite(records[i].data,1,r.payload_size,out)!=r.payload_size)) { fclose(out); goto oom; } }
    long archive = ftell(out); fclose(out);
    printf("# ANLC 0.9.3\nInput size:     %zu\nBlocks:         %u\nRAW blocks:     %u\nBlock refs:     %u\nMiniBlocks:     %u\nMini refs:      %u\nRepeats:        %u\nArchive size:   %ld\nReduction:      %.2f%%\nCRC32:          %08" PRIX32 "\nCompilation successful.\n",n,blocks,raws,blockrefs,mini,refs,repeats,archive,n?100.0*(1.0-(double)archive/(double)n):0,h.final_crc32);
    for (uint32_t i = 0; i < blocks; ++i) free(records[i].data);
    free(records); free(last); free(d); return 0;
oom:
    for (uint32_t i = 0; i < blocks; ++i) free(records[i].data);
    free(records); free(last); free(d); return 1;
}
