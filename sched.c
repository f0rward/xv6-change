#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched.h"
#include "rq.h"

// by jimmy:
void init_rq_simple(struct rq* rq){
  int i;

  // mark all the nodes is free
  for(i = 0; i < NPROC; i++){
    rq->nodes[i].prev = NULL;
  }
}

void enqueue_proc_simple(struct rq *rq, struct proc *p){
  //cprintf("in enqueue: %x, free_list = %x\n", p->pid, rq->free_list);
  //_check_lock(&(rq->rq_lock), "enqueue_proc_simple no lock");

  struct rq_node* pnode = rq->free_list;
  rq->free_list = pnode->next;

  // insert before next_to_run
  pnode->proc = p;
  if(rq->next_to_run == 0){
    rq->next_to_run = pnode;
    pnode->next = pnode->prev = pnode;
  }else{
    struct rq_node* prev = rq->next_to_run->prev;
    prev->next = pnode;
    pnode->prev = prev;
    rq->next_to_run->prev = pnode;
    pnode->next = rq->next_to_run;
  }

  p->rq = rq;
  rq->proc_num ++;
}

void dequeue_proc_simple(struct rq *rq, struct proc *p){
  // search
  struct rq_node* cnode;
  if(rq->next_to_run->proc == p)
    cnode = rq->next_to_run;
  else{
    cnode = rq->next_to_run->prev;
    while(cnode->proc != p){
      if(cnode == rq->next_to_run){
	cprintf("proc: %s, state = %d\n", p->name, p->state);
        panic("Kernel panic: Cannot find proc in dequeue_proc_simple\n");
      }
      cnode = cnode->prev;
    }
  }

  // delete
  if(cnode->next == cnode){
    rq->next_to_run = 0;
  }else{
    struct rq_node* prev = cnode->prev;
    struct rq_node* next = cnode->next;
    prev->next = next;
    next->prev = prev;
    rq->next_to_run = next;
  }

  // free
  cnode->next = rq->free_list;
  rq->free_list = cnode;

  rq->proc_num --;
}

void yield_proc_simple(struct rq *rq){
  if(rq->next_to_run)
    rq->next_to_run = rq->next_to_run->next;
}

struct proc* pick_next_proc_simple(struct rq *rq){
  struct rq_node* p_node = rq->next_to_run;
  if (p_node == NULL)
    return NULL;
  return p_node->proc;
}

void proc_tick_simple(struct rq* rq, struct proc* p){
  yield();
  return;
}

// by jimmy:
void load_balance_simple(struct rq* rq){
  int max = 0;
  struct rq* src_rq = 0;
  int i;
  struct proc* procs_moved[NPROC/2];
  int num_procs_moved;
  
  // find out the busiest rq
  for(i=0; i<ncpu; i++){
    if(rqs[i].proc_num > max){
      src_rq = &(rqs[i]);
      max = src_rq->proc_num;
    }
  }
  if(src_rq == 0)
    return;

  // Get the proc from src_rq
  acquire(&(src_rq->rq_lock));
  num_procs_moved = src_rq->sched_class->get_proc(src_rq, procs_moved);
  release(&(src_rq->rq_lock));

  if(num_procs_moved != 0)
    cprintf("load_balance\n");

  acquire(&(rq->rq_lock));
  for(i=0; i<num_procs_moved; i++) {
    enqueue_proc(rq, procs_moved[i]);
  }
  release(&(rq->rq_lock));

  return;
}

// by jimmy:
int get_proc_simple(struct rq* rq, struct proc* procs_moved[]){
  int num_procs_moved;
  int i;
  struct rq_node* prev, *next, *cnode;

  num_procs_moved = rq->proc_num/2;

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

const struct sched_class simple_sched_class = {
  .init_rq		= init_rq_simple,
  .enqueue_proc		= enqueue_proc_simple,
  .dequeue_proc		= dequeue_proc_simple,
  .yield_proc		= yield_proc_simple,
  .pick_next_proc	= pick_next_proc_simple,
  .proc_tick		= proc_tick_simple,
  .load_balance		= load_balance_simple,
  .get_proc		= get_proc_simple,
};

