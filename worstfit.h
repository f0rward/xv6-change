#ifndef _WORSTFIT_H
#define _WORSTFIT_H

#include "pmap.h"

typedef struct free_area{
  page_list_head_t free_list;
  unsigned long nr_free;
} free_area_t;

void init_memmap_WF(struct Page * base, unsigned long nr_free);

extern struct Page * mem_map;

struct Page* __alloc_pages_WF(int nr);

void __free_pages_WF(struct Page* page, int nr);

#endif