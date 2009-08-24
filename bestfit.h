#ifndef _BESTFIT_H
#define _BESTFIT_H

#include "pmap.h"


/* The meaning and explanation for this file is analagous to that of firstfit.h*/
typedef struct free_area{
  page_list_head_t free_list;
  unsigned long nr_free;
} free_area_t;

void init_memmap_BF(struct Page * base, unsigned long nr_free);

extern struct Page * mem_map;

struct Page* __alloc_pages_BF(int nr);

void __free_pages_BF(struct Page* page, int nr);

#endif