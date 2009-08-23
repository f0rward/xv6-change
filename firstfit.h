#ifndef _FIRSTFIT_H
#define _FIRSTFIT_H

#include "pmap.h"

typedef struct free_area{
  page_list_head_t free_list;
  unsigned long nr_free;
} free_area_t;

void init_memmap_FF(struct Page * base, unsigned long nr_free);

extern struct Page * mem_map;

struct Page* __alloc_pages_FF(int nr);

void __free_pages_FF(struct Page* page, int nr);

#endif