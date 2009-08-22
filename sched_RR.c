#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched_RR.h"

//#define LOAD_BALANCE_OUTPUT

void init_rq_RR(struct rq* rq){
  int i;

  for(i = 0; i < NPROC; i++){
    rq->nodes[i].prev = NULL;
  }
}

void enqueue_proc_RR(struct rq *rq, struct proc *p){
  struct rq_node* pnode = rq->free_list;
  rq->free_list = pnode->next;

  if (p->rq != rq)
    p->timeslice = rq->max_slices;

  pnode->proc = p;

  if (rq->next_to_run == NULL){
    rq->next_to_run = rq->last_to_run = pnode;
    pnode->next = pnode->prev = pnode;
  } else {
    rq->next_to_run->prev->next = pnode;
    pnode->prev = rq->next_to_run->prev;
    pnode->next = rq->next_to_run;
    rq->next_to_run->prev = pnode;
  }

  p->rq = rq;
  rq->proc_num ++;
}

void dequeue_proc_RR(struct rq *rq, struct proc *p){
  struct rq_node* node = rq->next_to_run;
  while (node != rq->next_to_run->prev && node->proc != p)
  {
    node = node->next;
  }
  if (p == rq->next_to_run->proc)
  {
    if (node->next == node)
      rq->next_to_run = NULL;
    else
      rq->next_to_run = rq->next_to_run->next;
  }
  if (node->next != node)
  {
    node->prev->next = node->next;
    node->next->prev = node->prev;
  }
  
  // free
  node->next = rq->free_list;
  rq->free_list = node;

  rq->proc_num --;
}

void yield_proc_RR(struct rq *rq){
  if(rq->next_to_run)
    rq->next_to_run = rq->next_to_run->next;
}

struct proc* pick_next_proc_RR(struct rq *rq){
  struct rq_node* p_node = rq->next_to_run;
  if (p_node == NULL)
    return NULL;
  return p_node->proc;
}

void proc_tick_RR(struct rq* rq, struct proc* p){
  if(p == idleproc[cpu()])
    yield();
  else
  {
    p->timeslice--;
    if (p->timeslice == 0)
    {
      p->timeslice = rq->max_slices;
      yield();
    }
  }
}

void load_balance_RR(struct rq* rq){
  int max = 0;
  struct rq* src_rq = 0;
  int i;
  struct proc* procs_moved[NPROC/2];
  int num_procs_moved;
  
  // find out the busiest rq
  //for(i=0; i<ncpu; i++){
    if(rqs[i].proc_num > max){
      src_rq = &(rqs[i]);
      max = src_rq->proc_num;
    }
  //}
  if(src_rq == 0)
    return;

  // Get the proc from src_rq
  acquire(&(src_rq->rq_lock));
  num_procs_moved = src_rq->sched_class->get_proc(src_rq, procs_moved);
  release(&(src_rq->rq_lock));

  acquire(&(rq->rq_lock));
  for(i=0; i<num_procs_moved; i++) {
    enqueue_proc(rq, procs_moved[i]);
  }
  release(&(rq->rq_lock));

#ifdef LOAD_BALANCE_OUTPUT
  if(num_procs_moved != 0)
  {
    cprintf("after load_balance\n");
    procdump();
  }
#endif

  return;
}

// by jimmy:
int get_proc_RR(struct rq* rq, struct proc* procs_moved[]){
  int num_procs_moved;
  int i;
  struct rq_node* prev, *next, *cnode;

  num_procs_moved = rq->proc_num/2;

#ifdef LOAD_BALANCE_OUTPUT
  if(num_procs_moved != 0)
  {
    cprintf("before load_balance\n");
    release(&(rq->rq_lock));
    procdump();
    acquire(&(rq->rq_lock));
  }
#endif

  cnode = rq->next_to_run->next;
  for(i=0; i<num_procs_moved; i++){
    if(cnode->next == 0){
      release(&(rq->rq_lock));
      cprintf("Wrong here: proc_num = %d\n", rq->proc_num);
      panic("Not enough proc to move in get_proc_simple\n");
    }
    procs_moved[i] = cnode->proc;
    prev = cnode->prev, next = cnode->next;
    prev->next = next;
    next->prev = prev;
    cnode->next = rq->free_list;
    rq->free_list = cnode;
    cnode = next;
  }
  rq->proc_num -= num_procs_moved;

  return num_procs_moved;
}

const struct sched_class sched_class_RR = {
  .init_rq 		= init_rq_RR,
  .enqueue_proc		= enqueue_proc_RR,
  .dequeue_proc		= dequeue_proc_RR,
  .yield_proc		= yield_proc_RR,
  .pick_next_proc	= pick_next_proc_RR,
  .proc_tick		= proc_tick_RR,
  .load_balance		= load_balance_RR,
  .get_proc		= get_proc_RR,
};

