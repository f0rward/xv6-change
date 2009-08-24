#ifndef _FIRSTFIT_H
#define _FIRSTFIT_H

#include "pmap.h"

typedef struct free_area{
  page_list_head_t free_list; // keeps track of the head of the list of free memory.
  unsigned long nr_free; // the number of contiguous free memory block in free_list.
} free_area_t;

/* function used to initialize memory mapping*/
void init_memmap_FF(struct Page * base, unsigned long nr_free);

extern struct Page * mem_map;

/* function for allocating nr pages */
struct Page* __alloc_pages_FF(int nr);


/* function for freeing nr pages whose descriptors start from page to page+nr */
void __free_pages_FF(struct Page* page, int nr);

#endif