#include <lzma.h>

int main(void)
{
    lzma_index *index = lzma_index_init(NULL);
    if (index == NULL)
        return 1;

    for (int i = 0; i < 513; ++i) {
        if (lzma_index_append(index, NULL, 5, 0) != LZMA_OK) {
            lzma_index_end(index, NULL);
            return 1;
        }
    }

    lzma_index *copy = lzma_index_dup(index, NULL);
    if (copy != NULL)
        lzma_index_end(copy, NULL);

    lzma_index_end(index, NULL);
    return 0;
}
