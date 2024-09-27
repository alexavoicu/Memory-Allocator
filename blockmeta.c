#include "block_meta.h"
#include <unistd.h>
#include <sys/mman.h>
#include "block_meta_functions.h"
#include "../utils/printf.h"

#define MMAP_THRESHOLD (128 * 1024)

#define ALIGNMENT 8
#define ALIGN(size) (((size) + 7) & ~7)
#define SIZE_META (ALIGN(sizeof(struct block_meta)))

struct block_meta *base;
int howmany = 0;

void base_init(void)
{
	base = NULL;
}

struct block_meta *find_best(size_t size)
{
	struct block_meta *iter = base;
	while (iter != NULL && iter->status != STATUS_MAPPED) {
		//verificam sa nu ajungem la finalul listei sau la memoria mapped
		if (iter->status == STATUS_FREE) {
			if (iter->size >= size || iter->next == NULL || iter->next->status == STATUS_MAPPED) {
				//este un block ce poate fi refolosit daca dimensiunea este suficient de mare sau daca
				//este ultimul block alocat pe heap si poate fi extins

				if (iter->size >= size + ALIGN(1) + SIZE_META) {
					//se poate face split, initializam noul block
					struct block_meta *right_block = (struct block_meta *)((char *)iter + size + SIZE_META);

					right_block->size = iter->size - size - SIZE_META;
					right_block->status = STATUS_FREE;
					right_block->next = iter->next;
					right_block->prev = iter;

					if (right_block->next != NULL) {
						right_block->next->prev = right_block;
					}

					//actualizam dimensiunea block-ului vechi
					iter->size = size;
					iter->next = right_block;
				}

				return iter;
			}
		}
		
		iter = iter->next;
	}

	return NULL;
}

int allocate_extra_space(size_t size)
{
	void *request = sbrk(size);
	if (request == (void *)-1)
	{
		return -1;	//nu a putut fi alocat
	}
	return 0;
}

void insert_last_mapped(struct block_meta *ptr, size_t size)
{
	struct block_meta *mapped = ptr;
	mapped->next = NULL;
	mapped->status = STATUS_MAPPED;
	mapped->size = size;

	if (base == NULL)
	{
		base = mapped;
		base->prev = NULL;
		return;
	}

	//mereu inseram la final memoria alocata cu mmap
	//pentru a putea avea memoria alocata cu heap retinuta continuu
	struct block_meta *iter = base;
	while (iter->next != NULL)
	{
		iter = iter->next;
	}
	iter->next = mapped;
	mapped->prev = iter;

	return;
}

void coleasce_blocks (void) {
	struct block_meta *iter = base;

	if (iter == NULL)
	{
		return;
	}

	while (iter != NULL)
	{
		if (iter->status == STATUS_FREE)
		{
			struct block_meta *free_block = iter->next;

			//comasam blocurile adiancente free
			while (free_block != NULL && free_block->status == STATUS_FREE)
			{
				iter->size = iter->size + free_block->size + SIZE_META;	//actualizam dimensiunea
				struct block_meta *block_to_delete = free_block;
				free_block = free_block->next;

				//stergem din lista block-ul care a fost comasat
				delete_block(block_to_delete);
			}
			
		}

		iter = iter->next;
	}
	
}

void insert_heap_memory(struct block_meta *block, size_t size)
{
	struct block_meta *last = base;

	block->status = STATUS_ALLOC;
	block->size = size;
	block->next = block->prev = NULL;

	if (base == NULL)	//lista goala
	{
		base = block;
		return;
	}

	//primul element este alocat cu mmap
	//inseram inaintea lui
	if (base->status == STATUS_MAPPED)
	{
		block->next = base;
		base->prev = block;
		base = block;
		return;
	}

	//gasim ultimul block allocat pe heap si inseram dupa el
	while (last->next != NULL && last->next->status != STATUS_MAPPED)
	{
		last = last->next;
	}

	block->next = last->next;
	block->prev = last;
	if (block->next != NULL)
	{
		block->next->prev = block;
	}
	last->next = block;
}

void delete_block(void *ptr)
{
	struct block_meta *first_elem = base;

	if (first_elem == NULL)	//daca lista e goala nu stergem nimic
	{
		return;
	}

	struct block_meta *block_to_delete = (struct block_meta *)ptr;

	struct block_meta *previous = block_to_delete->prev;
	
	//daca este primul element al listei
	if (previous == NULL)
	{
		base = base->next;
		if (base != NULL)
		{
			base->prev = NULL;
		}

		return;
	}

	//altfel scoatem elementul din lista si refacem legaturile
	previous->next = block_to_delete->next;
	if (previous->next != NULL)
	{
		previous->next->prev = previous;
	}
}
