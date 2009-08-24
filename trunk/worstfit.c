#include "bestfit.h"
#include "defs.h"

free_area_t free_area;
struct Page* mem_map;
void test_worstfit();

void init_memmap_WF(struct Page * base, unsigned long nr){
  struct Page* page=base;
  mem_map = base;
  if(!LIST_EMPTY(&free_area.free_list)){
    panic("error");
  }
  LIST_INSERT_HEAD(&free_area.free_list, page, lru);
  free_area.nr_free++;
  page->property = nr;
  test_worstfit();
}

struct Page * __alloc_pages_WF(int nr){
  struct Page* page = LIST_FIRST(&free_area.free_list);
  int count=0;
 // cprintf("%d %d\n", count, free_area.nr_free);
  struct Page* bulk_to_alloc=page;
  uint32_t max_cont_block=page->property;
  while(count<free_area.nr_free){
    //cprintf("%d %d\n", page->property, nr);
    if(page->property>=nr){
      if(page->property>max_cont_block){
        max_cont_block=page->property;
        bulk_to_alloc=page;
      }
    }
    count++;
    page=LIST_NEXT(page, lru);
  }
 // cprintf("%x %d\n", bulk_to_alloc, bulk_to_alloc->property);
  if(bulk_to_alloc && bulk_to_alloc->property>=nr){
    LIST_REMOVE(bulk_to_alloc, lru);
    struct Page* remain=bulk_to_alloc+nr;
    remain->property=bulk_to_alloc->property-nr;
    //cprintf("remaining %d\n", remain->property);
    if(remain->property){
      //cprintf("putting back\n");
      LIST_INSERT_HEAD(&free_area.free_list, remain, lru);
    }else
      free_area.nr_free--;
    return bulk_to_alloc;
  }else{
    return NULL;
  }
}
void __free_pages_WF(struct Page * page, int nr){
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

void print_cont_mem_WF(){
  int count=0;
  struct Page* page=LIST_FIRST(&free_area.free_list);
  while(count<free_area.nr_free){
    cprintf("free contiguous mem %x, size: %d pages\n", page_addr(page), page->property);
    count++;
    page=LIST_NEXT(page, lru);
  }
}

void test_worstfit(){
  print_cont_mem_WF();
  struct Page* p1=__alloc_pages_WF(1);
  struct Page* head=LIST_FIRST(&free_area.free_list);
  cprintf("after allocating a page, starting address of the contiguous mem becomes %x\n", page_addr(head));
  struct Page* p2=__alloc_pages_WF(1);
  cprintf("allocating p2 and freeing p1\n");
  __free_pages_WF(p1, 1);
  cprintf("after freeing p1\n");
  print_cont_mem_WF();
  cprintf("allocating p3\n");
  struct Page* p3=__alloc_pages_WF(1);
  cprintf("after allocating p3\n");
  print_cont_mem_WF();
}