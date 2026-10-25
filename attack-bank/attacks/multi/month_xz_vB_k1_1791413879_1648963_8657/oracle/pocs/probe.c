#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void *allocate(void *opaque, size_t count, size_t size)
{
    (void)opaque;
    if (size != 0 && count > SIZE_MAX / size)
        return NULL;
    void *ptr = malloc(count * size);
    if (ptr != NULL)
        memset(ptr, 0xa5, count * size);
    return ptr;
}

static void deallocate(void *opaque, void *ptr)
{
    (void)opaque;
    free(ptr);
}

int main(void)
{
    const lzma_allocator allocator = {
        .alloc = allocate,
        .free = deallocate,
        .opaque = NULL,
    };
    lzma_index *index = lzma_index_init(&allocator);
    if (index == NULL)
        return 1;
    if (lzma_index_append(index, &allocator, 5, 1) != LZMA_OK) {
        lzma_index_end(index, &allocator);
        return 2;
    }
    lzma_index_end(index, &allocator);
    return 0;
}
