#include <lzma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t lzip_data[] = {
    0x4c, 0x5a, 0x49, 0x50, 0x01, 0x0c, 0x00, 0x24,
    0x19, 0x49, 0x98, 0x6f, 0x05, 0x15, 0x27, 0x27,
    0x0d, 0x76, 0x78, 0xd0, 0x2a, 0x68, 0x17, 0x15,
    0xff, 0xff, 0x75, 0xf8, 0x00, 0x00, 0x43, 0xa3,
    0xa2, 0x15, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

struct alloc_state {
    int fail_large_once;
    int failed;
};

static void *alloc_mem(void *opaque, size_t nmemb, size_t size)
{
    struct alloc_state *state = opaque;
    if (state->fail_large_once && size >= (1U << 20)) {
        state->fail_large_once = 0;
        state->failed = 1;
        return NULL;
    }
    return malloc(nmemb * size);
}

static void free_mem(void *opaque, void *ptr)
{
    (void)opaque;
    free(ptr);
}

static lzma_ret decode(lzma_stream *strm, const uint8_t *input)
{
    uint8_t output[64];
    strm->next_in = input;
    strm->avail_in = sizeof(lzip_data);
    strm->next_out = output;
    strm->avail_out = sizeof(output);
    return lzma_code(strm, LZMA_FINISH);
}

int main(void)
{
    struct alloc_state state = { 0, 0 };
    lzma_allocator allocator = { alloc_mem, free_mem, &state };
    lzma_stream strm = LZMA_STREAM_INIT;
    strm.allocator = &allocator;
    uint8_t large_dict[sizeof(lzip_data)];
    memcpy(large_dict, lzip_data, sizeof(large_dict));
    large_dict[5] = 0x14;

    lzma_stream verify = LZMA_STREAM_INIT;
    if (lzma_lzip_decoder(&verify, UINT64_MAX, 0) != LZMA_OK
            || decode(&verify, large_dict) != LZMA_STREAM_END)
        return 6;
    lzma_end(&verify);

    if (lzma_lzip_decoder(&strm, UINT64_MAX, 0) != LZMA_OK
            || decode(&strm, lzip_data) != LZMA_STREAM_END)
        return 1;

    if (lzma_lzip_decoder(&strm, UINT64_MAX, 0) != LZMA_OK)
        return 2;
    state.fail_large_once = 1;
    if (decode(&strm, large_dict) != LZMA_MEM_ERROR || !state.failed)
        return 3;

    if (lzma_lzip_decoder(&strm, UINT64_MAX, 0) != LZMA_OK)
        return 4;
    lzma_ret ret = decode(&strm, lzip_data);
    lzma_end(&strm);
    return ret == LZMA_STREAM_END ? 0 : 5;
}
