#include "firstfit.h"
#include "defs.h"

free_area_t free_area;
struct Page *mem_map;

void init_memmap_FF(struct Page * base, unsigned long nr){
  struct Page* page=base;
  mem_map = base;
  if(!LIST_EMPTY(&free_area.free_list)){
    panic("error");
  }
  LIST_INSERT_HEAD(&free_area.free_list, page, lru);
  free_area.nr_free++;
  page->property = nr;
}

struct Page * __alloc_pages_FF(int nr){
  struct Page* page = LIST_FIRST(&free_area.free_list);
  int count=0;
 // cprintf("%d %d\n", count, free_area.nr_free);
  while(count<free_area.nr_free){
  //  cprintf("%d %d\n", page->property, nr);
    if(page->property>=nr){
      break;
    }
    count++;
    page=LIST_NEXT(page, lru);
  }
  if(count<=free_area.nr_free){
    LIST_REMOVE(page, lru);
    free_area.nr_free--;
    struct Page* remain=page+nr;
    remain->property=page->property-nr;
    if(remain->property)
      LIST_INSERT_HEAD(&free_area.free_list, remain, lru);
    return page;
  }else{
    return NULL;
  }
}
void __free_pages_FF(struct Page * page, int nr){
  struct Page* head=LIST_FIRST(&free_area.free_list);
  if(head==page+nr){
    page->property=nr+head->property;
    head->property=0;
    LIST_REMOVE(head, lru);
    LIST_INSERT_HEAD(&free_area.free_list, page, lru);
  }else if(head==page-nr){
    head->property+=nr;
    page->property=0;
  }else{
    LIST_INSERT_HEAD(&free_area.free_list, page, lru);
    page->property=nr;
    free_area.nr_free++;
  }
}