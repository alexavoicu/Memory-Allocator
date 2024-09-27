// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include <unistd.h>
#include "block_meta.h"
#include "block_meta_functions.h"
#include <sys/mman.h>
#include <string.h>

#define MMAP_THRESHOLD (128 * 1024)
#define ALIGN(size) (((size) + 7) & ~7)
#define SIZE_META (ALIGN(sizeof(struct block_meta)))

int is_preallocated = 0;

void *os_malloc(size_t size)
{
	if (size <= 0)
	{
		return NULL;
	}

	int block_size = ALIGN(size) + SIZE_META;

	if (block_size >= MMAP_THRESHOLD)	//alocam cu mmap
	{
		void *ret = mmap(NULL, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		DIE(ret == (void *)-1, "Mmap failed");

		//inseram mereu la finalul listei memoria mapped
		insert_last_mapped((struct block_meta *)ret, block_size);
		return (void *)(((char *)ret) + SIZE_META);
	}

	if (is_preallocated == 0)	//daca nu a fost prealocata memoria pe heap
	{
		void *allocate = sbrk(MMAP_THRESHOLD);
		DIE(allocate == (void *)-1, "Sbrk failed");	//nu a putut fi alocat

		insert_heap_memory((struct block_meta *)allocate, MMAP_THRESHOLD - SIZE_META);
		is_preallocated = 1;	//setam indicatorul de prealocare
		return (void *)(((char *)allocate) + SIZE_META);
	}

	coleasce_blocks();	//lipim toate blocurile libere adiacente

	struct block_meta *block_reuse = find_best(block_size - SIZE_META);

	if (block_reuse != NULL)	//daca gasim un block liber il refolosim
	{
		if (block_reuse->size < block_size - SIZE_META)
		{ // expand last block
			void *allocate = sbrk(block_size - block_reuse->size - SIZE_META);
			DIE(allocate == (void *)-1, "Sbrk failed");	//nu a putut fi alocat

			block_reuse->size = block_size - SIZE_META;
		}
		
		block_reuse->status = STATUS_ALLOC;
		return (void *)(((char *)block_reuse) + SIZE_META);
	}

	//daca nu am gasit un block ce poate fi refolosit, alocam memorie
	void *allocate = sbrk(block_size);
	DIE(allocate == (void *)-1, "Sbrk failed");	//nu a putut fi alocat

	insert_heap_memory((struct block_meta *)allocate, block_size - SIZE_META);

	return (void *)(((char *)allocate) + SIZE_META);
}

void os_free(void *ptr)
{
	if (ptr == NULL)
	{
		return;
	}

	//obtinem adresa nodului 
	void *start_address = (void *)(((char *)ptr) - SIZE_META);

	int size = ((struct block_meta *)start_address)->size;
	int status = ((struct block_meta *)start_address)->status;

	//daca este alocat pe heap, doar setam statusul pe free
	if (status == STATUS_ALLOC)
	{
		((struct block_meta *)start_address)->status = STATUS_FREE;
		return;
	}

	//daca este alocata cu mmap, stergem blockul din lista si dezalocam cu munmap
	if (status == STATUS_MAPPED)
	{
		delete_block(start_address);
		int return_on_munmap = munmap(start_address, size);
		DIE(return_on_munmap == -1, "Munmap failed");	//nu a putut fi alocat
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (nmemb ==  0 || size == 0)
	{
		return NULL;
	}

	int block_size = ALIGN(nmemb * size) + SIZE_META;

	if (block_size >= getpagesize())
	{
		void *ret = mmap(NULL, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		DIE(ret == (void *)-1, "Mmap failed");	//nu a putut fi alocat

		insert_last_mapped((struct block_meta *)ret, block_size);
		memset((void *)(((char *)ret) + SIZE_META), 0, ((struct block_meta *)ret)->size);
		return (void *)(((char *)ret) + SIZE_META);
	}

	if (is_preallocated == 0)
	{
		void *allocate = sbrk(MMAP_THRESHOLD);
		DIE(allocate == (void *)-1, "Sbrk failed");	//nu a putut fi alocat

		insert_heap_memory((struct block_meta *)allocate, MMAP_THRESHOLD - SIZE_META);
		is_preallocated = 1;

		//setam cu 0 memoria alocata
		memset((void *)(((char *)allocate) + SIZE_META), 0, ((struct block_meta *)allocate)->size);
		return (void *)(((char *)allocate) + SIZE_META);
	}

	coleasce_blocks();

	struct block_meta *block_reuse = find_best(block_size - SIZE_META);

	if (block_reuse != NULL)
	{
		if (block_reuse->size < block_size - SIZE_META)
		{ // expand last block
			void *allocate = sbrk(block_size - block_reuse->size - SIZE_META);
			DIE(allocate == (void *)-1, "Sbrk failed");	//nu a putut fi alocat
			
			block_reuse->size = block_size - SIZE_META;
		}
		
		block_reuse->status = STATUS_ALLOC;
		memset((void *)(((char *)block_reuse) + SIZE_META), 0, ((struct block_meta *)block_reuse)->size);
		return (void *)(((char *)block_reuse) + SIZE_META);
	}

	void *allocate = sbrk(block_size);
	DIE(allocate == (void *)-1, "Sbrk failed");	//nu a putut fi alocat

	//inseram in lista block-ul
	insert_heap_memory((struct block_meta *)allocate, block_size - SIZE_META);

	memset((void *)(((char *)allocate) + SIZE_META), 0, ((struct block_meta *)allocate)->size);
	return (void *)(((char *)allocate) + SIZE_META);
	


	return NULL;
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */
	return NULL;
}
