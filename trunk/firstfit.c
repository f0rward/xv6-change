#include "firstfit.h"
#include "defs.h"

free_area_t free_area;
struct Page *mem_map;
void test_firstfit(); // function for testing the effect of first fit algorithm

/* Itinial mapping of pages is virtually the same for first fit, best fit and worst     fit algorithms*/
void init_memmap_FF(struct Page * base, unsigned long nr){
  /* Once a contiguous block of memory is encountered, the first fit algorithm 
     adds this contiguous block to free_area.free_list, and increments
     free_area.nr_free by 1, indicating a new contiguous block of memory is added
     to free_list. Then in order to help manage the pages, we have to update
     the property field of the first page descriptor representing the first physical
     page of the block of memory by assigning it to nr which is the number of
     pages in that block of memory. This is the same in best fit and worst fit
     algorithm. */
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

/* allocating pages is virtually the same for first fit, best fit and worst fit
   algorithms with only one difference: the way they seek free available memory
   to allocate. */
struct Page * __alloc_pages_FF(int nr){
  /* Iterating among the page descriptors kept in free_area.free_list, looking
     for the first block of contiguous memory bigger than or equal to nr pages.
     Remember the property field of the first page descriptor in a block of
     contiguous memory is used to store the number of pages available in
     that block of memory. */
  struct Page* page = LIST_FIRST(&free_area.free_list);
  int count=0;
 // cprintf("%d %d\n", count, free_area.nr_free);
  while(count<free_area.nr_free){   // looking for first proper block
  //  cprintf("%d %d\n", page->property, nr);
    if(page->property>=nr){
      break;
    }
    count++;
    page=LIST_NEXT(page, lru);
  }
  if(count<=free_area.nr_free){  // The block is found!
    LIST_REMOVE(page, lru);  //take that block as a whole
    struct Page* remain=page+nr; //find the start of remaining mem in that block.
                                // (We only need nr pages from it).
    remain->property=page->property-nr;  //update the remaining part's property.
    if(remain->property)        // If there is some memory left, put it back.
      LIST_INSERT_HEAD(&free_area.free_list, remain, lru);
    else
      free_area.nr_free--;       // we use up the whole block, decrement the number
                           //of free blocks of memory.
    return page;
  }else{
    return NULL;
  }
}

/* freeing of pages is virtually the same for first fit, best fit and worst fit
   algorithms*/
void __free_pages_FF(struct Page * page, int nr){
  struct Page* head=LIST_FIRST(&free_area.free_list);
  if(head==page+nr){  //if the block being freed can be inserted at the head
                     // of free_area.free_list and form a new larger block of
                       //contiguous memory, then put it at the head and form a
                     //new block. Remember to pick off the original head since
                     //they are now a new contiguous block. Update property.
    page->property=nr+head->property;
    head->property=0;
    LIST_REMOVE(head, lru);
    LIST_INSERT_HEAD(&free_area.free_list, page, lru);
  }else if(head==page-head->property){
                      //if the block being freed can be inserted after the head
                     // of free_area.free_list and form a new larger block of
                       //contiguous memory, then put it after the head and form a
                     //new block. Update property.
    struct Page* head_next = LIST_NEXT(head, lru);
    if(page+nr==head_next){
      head->property+=head_next->property;
      head_next->property=0;
      LIST_REMOVE(head_next, lru);
      free_area.nr_free--;
    }
    head->property+=nr;
    page->property=0;
  }else{  // cannot form a contiguous block. Just put it back and update property.
    LIST_INSERT_HEAD(&free_area.free_list, page, lru);
    page->property=nr;
    free_area.nr_free++;
  }
}

/* function for printing free memory, one contiguous block after another. */
void print_cont_mem_FF(){
  int count=0;
  struct Page* page=LIST_FIRST(&free_area.free_list);
  while(count<free_area.nr_free){
    cprintf("free contiguous mem %x, size: %d pages\n", page_addr(page), page->property);
    count++;
    page=LIST_NEXT(page, lru);
  }
}


/* testing first fit. */
void test_firstfit(){
  print_cont_mem_FF();
  struct Page* p1=__alloc_pages_FF(1);
  struct Page* head=LIST_FIRST(&free_area.free_list);
  cprintf("after allocating a page, starting address of the contiguous mem becomes %x\n", page_addr(head));
  struct Page* p2=__alloc_pages_FF(1);
  __free_pages_FF(p1, 1);
  cprintf("after freeing p1\n");
  print_cont_mem_FF();
  __free_pages_FF(p2, 1);
  cprintf("after freeing p2\n");
  print_cont_mem_FF();
}