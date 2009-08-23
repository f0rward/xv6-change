#include "firstfit.h"
#include "defs.h"

free_area_t free_area;
struct Page *mem_map;
void test_firstfit();

void init_memmap_FF(struct Page * base, unsigned long nr){
  struct Page* page=base;
  mem_map = base;
  if(!LIST_EMPTY(&free_area.free_list)){
    panic("error");
  }
  LIST_INSERT_HEAD(&free_area.free_list, page, lru);
  free_area.nr_free++;
  page->property = nr;
  //test_firstfit();
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
    struct Page* remain=page+nr;
    remain->property=page->property-nr;
    if(remain->property)
      LIST_INSERT_HEAD(&free_area.free_list, remain, lru);
    else
      free_area.nr_free--;
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
  }else if(head==page-head->property){
    struct Page* head_next = LIST_NEXT(head, lru);
    if(page+nr==head_next){
      head->property+=head_next->property;
      head_next->property=0;
      LIST_REMOVE(head_next, lru);
      free_area.nr_free--;
    }
    head->property+=nr;
    page->property=0;
  }else{
    LIST_INSERT_HEAD(&free_area.free_list, page, lru);
    page->property=nr;
    free_area.nr_free++;
  }
}

void print_cont_mem(){
  int count=0;
  struct Page* page=mem_map;
  while(count<free_area.nr_free){
    cprintf("free contiguous mem %x, size: %d pages\n", page_addr(page), page->property);
    count++;
    page=LIST_NEXT(page, lru);
  }
}

void test_firstfit(){
  print_cont_mem();
  struct Page* p1=__alloc_pages_FF(1);
  struct Page* head=LIST_FIRST(&free_area.free_list);
  cprintf("after allocating a page, starting address of the contiguous mem becomes %x\n", page_addr(head));
  struct Page* p2=__alloc_pages_FF(1);
  __free_pages_FF(p1, 1);
  cprintf("after freeing p1\n");
  print_cont_mem();
  __free_pages_FF(p2, 1);
  cprintf("after freeing p2\n");
  print_cont_mem();
}