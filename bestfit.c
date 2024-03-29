#include "bestfit.h"
#include "defs.h"

free_area_t free_area;
struct Page* mem_map;
void test_bestfit();

/* refer to firstfit.c */
void init_memmap_BF(struct Page * base, unsigned long nr){
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


/* Function for seeking the smallest block of contiguous memory which is bigger
   than or equal to nr pages.*/
struct Page * __alloc_pages_BF(int nr){
  struct Page* page = LIST_FIRST(&free_area.free_list);
  int count=0;
 // cprintf("%d %d\n", count, free_area.nr_free);
  struct Page* bulk_to_alloc=NULL;  //mark the starting page of a block to allocate.
  uint32_t min_cont_block=0; // track the smallest size of mem bigger than or equal
                             // to nr pages.
  while(count<free_area.nr_free){
  //  cprintf("%d %d\n", page->property, nr);
    if(page->property>=nr){  //Only if the size of a block is enough for nr pages
                             // will we consider the block.
      if(!bulk_to_alloc || page->property<min_cont_block){ // find the block whose
                             //size is closest to nr.
        min_cont_block=page->property;
        bulk_to_alloc=page;
      }
    }
    count++;
    page=LIST_NEXT(page, lru);
  }
  if(bulk_to_alloc && bulk_to_alloc->property>=nr){ //If the block is found.
    LIST_REMOVE(bulk_to_alloc, lru); //first pick off this block.
    struct Page* remain=bulk_to_alloc+nr; // find the remains.
    remain->property=bulk_to_alloc->property-nr; //update the property of the
                                      //remaining part.
    if(remain->property)      // If we do not use up the block, just put it back.
      LIST_INSERT_HEAD(&free_area.free_list, remain, lru);
    else
      free_area.nr_free--;  // or we can decrement the number of free blocks of
                           // contiguous memory in free_area.free_list.
    return bulk_to_alloc;
  }else{
    return NULL;
  }
}

/*refer to firstfit.c*/
void __free_pages_BF(struct Page * page, int nr){
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

/*refer firstfit.c*/
void print_cont_mem_BF(){
  int count=0;
  struct Page* page=LIST_FIRST(&free_area.free_list);
  while(count<free_area.nr_free){
    cprintf("free contiguous mem %x, size: %d pages\n", page_addr(page), page->property);
    count++;
    page=LIST_NEXT(page, lru);
  }
}

void test_bestfit(){
  print_cont_mem_BF();
  struct Page* p1=__alloc_pages_BF(1);
  struct Page* head=LIST_FIRST(&free_area.free_list);
  cprintf("after allocating a page, starting address of the contiguous mem becomes %x\n", page_addr(head));
  struct Page* p2=__alloc_pages_BF(1);
  __free_pages_BF(p1, 1);
  cprintf("after freeing p1\n");
  print_cont_mem_BF();
  __free_pages_BF(p2, 1);
  cprintf("after freeing p2\n");
  print_cont_mem_BF();
  cprintf("allocating 2 pages\n");
  struct Page* p3=__alloc_pages_BF(2);
  cprintf("after allocating 2 pages\n");
  print_cont_mem_BF();
}