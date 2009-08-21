#include "buddy.h"
#include "defs.h"

free_area_t free_area[MAX_ORDER];
struct Page * mem_map;
const int FreeAreaSize[MAX_ORDER] = {1,2,4,8,16,32,64,128,256,512,1024};
void test_buddy();

// check if pointer page indicates the first page frame of a block of order_size pages
// if so , return 1, else return 0
int
page_is_buddy(struct Page * page, int order)
{
  if (PageProperty(page) && page->property == order && !PageReserved(page))
    return 1;
  return 0;
}

void
init_memmap(struct Page * base, unsigned long nr)
{
  int i,order;
  struct Page * page = base;
  mem_map = base;
  for (i = 0; i < MAX_ORDER; i++) {
    if (!LIST_EMPTY(&(free_area[i].free_list)))
      panic("error");
  }
  while (nr) {
    for (order = 10; order >=0; order --) {
      if (nr >= FreeAreaSize[order]) {
        LIST_INSERT_HEAD(&(free_area[order].free_list), page, lru);
        free_area[order].nr_free ++;
        page->property = order;
        nr -= FreeAreaSize[order];
        page += FreeAreaSize[order];
        break;
      }
    }
  }
//  test_buddy();
}

struct Page *
__alloc_pages(int nr)
{
  int i;
  struct Page * p;
  for (i = 0; i < MAX_ORDER; i++) {
    if (nr <= FreeAreaSize[i]) {
      p = alloc_pages_bulk(i);
      dbmsg("cpu %x kalloc %x : order %d;\n",cpu(), p - pages, i);
      return p;
    }
  }
  return NULL;
}

// implement the buddy system strategy for freeing page frames
struct Page *
alloc_pages_bulk(int order)
{
  int isalloc = 0;
  int current_order, size = 0;

  struct Page * page = NULL, * buddy = NULL;

  // try to find a suitable block in the buddy system
  for (current_order = order; current_order < MAX_ORDER; current_order ++) {
    if (!LIST_EMPTY(&(free_area[current_order].free_list))) {
      isalloc = 1;
      break;
    }
  }

  if (!isalloc)
    return NULL;
  else {
    page = LIST_FIRST(&(free_area[current_order].free_list));
//    cprintf("current order : %x,remove %x\n",current_order,page - pages);
    LIST_REMOVE(page, lru);
    page->property = 0;
    ClearPageProperty(page);
    free_area[current_order].nr_free --;
  }

  size = 1 << current_order;
  while (current_order > order) {
    current_order --;
    size >>= 1;
    buddy = page + size;
//    cprintf("insert order : %x, insert %x\n",current_order, buddy - pages);
    LIST_INSERT_HEAD(&(free_area[current_order].free_list), buddy, lru);
    buddy->property = current_order;
    SetPageProperty(buddy);
    free_area[current_order].nr_free ++;
  }

  return page;
}

void
__free_pages(struct Page * page, int nr)
{
  int i;
  for (i = 0; i < MAX_ORDER; i++) {
    if (nr <= FreeAreaSize[i]) {
      dbmsg("kfree %x : order %d\n",page - pages, i);
      free_pages_bulk(page, i);
      return;
    }
  }
}

// implement the buddy system strategy for freeing page frames
void
free_pages_bulk(struct Page * page, int order)
{
  int size = 1 << order;
  unsigned long page_idx = page - mem_map, buddy_idx;
  struct Page * buddy, * coalesced;
  while (order < 10) {
    // calculate the buddy index, by xor operation
    // if 1 << order bit was previously zero, buddy 
    // index is equal to page_idx + size, conversely,
    // if the bit was previously one, buddy index is
    // equal to page_idx - size
    buddy_idx = page_idx ^ size;
    buddy = &mem_map[buddy_idx];
    if (!page_is_buddy(buddy, order))
      break;
    LIST_REMOVE(buddy, lru);
    free_area[order].nr_free --;
    buddy->property = 0;
    ClearPageProperty(buddy);
    page_idx &= buddy_idx;
    order ++;
  }
  coalesced = &mem_map[page_idx];
  dbmsg("free order %x, page %x\n", order, coalesced - pages);
  coalesced->property = order;
  SetPageProperty(coalesced);
  LIST_INSERT_HEAD(&(free_area[order].free_list), coalesced, lru);
  free_area[order].nr_free ++;
}

void
print_buddy()
{
  int i;
  for (i = 0; i < MAX_ORDER; i++)
    cprintf("buddy %d : %x blocks; ",i,free_area[i].nr_free);
  cprintf("\n");
}

void
test_buddy()
{
  struct Page * a1, * a2;
  print_buddy();
  a1 = alloc_pages_bulk(8);
  a2 = alloc_pages_bulk(8);
  print_buddy();
  free_pages_bulk(a1, 8);
  print_buddy();
  free_pages_bulk(a2, 8);
  print_buddy();
}
