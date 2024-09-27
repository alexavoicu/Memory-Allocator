#include "block_meta.h"

struct block_meta *find_best(size_t size);
void insert_last_mapped (struct block_meta* ptr, size_t size);
void delete_block(void *ptr);
void insert_heap_memory (struct block_meta* block, size_t size);
void coleasce_blocks (void);
int allocate_extra_space (size_t size);
