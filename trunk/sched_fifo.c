#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched_fifo.h"

void init_rq_fifo(struct rq* rq){
  return;
}

void enqueue_proc_fifo(struct rq *rq, struct proc *p){
  struct rq_node* pnode = rq->free_list;
  rq->free_list = pnode->next;

  // insert after last_to_run
  pnode->proc = p;
  pnode->next = NULL;
  
  if (rq->next_to_run == NULL){
    rq->next_to_run = rq->last_to_run = pnode;
  } else {
    rq->last_to_run->next = pnode;
    rq->last_to_run = pnode;
  }

  p->rq = rq;
  rq->proc_num ++;
}

void dequeue_proc_fifo(struct rq *rq, struct proc *p){
  struct rq_node* node = rq->next_to_run;
  struct rq_node* pnode = NULL;
  while (node != rq->last_to_run && node->proc != p)
  {
    pnode = node;
    node = node->next;
  }
  if (p == rq->next_to_run->proc)
    rq->next_to_run = node->next;
  else
    pnode->next = node->next;
  
  // free
  node->next = rq->free_list;
  rq->free_list = node;

  rq->proc_num --;
}

void yield_proc_fifo(struct rq *rq){
  if(rq->next_to_run)
  {
    if (rq->next_to_run == rq->last_to_run)
	return;
    rq->last_to_run->next = rq->next_to_run;
    rq->next_to_run = rq->next_to_run->next;
    rq->last_to_run = rq->last_to_run->next;
  }
}

struct proc* pick_next_proc_fifo(struct rq *rq){
  struct rq_node* p_node = rq->next_to_run;
  if (p_node == NULL)
    return NULL;
  return p_node->proc;
}

void proc_tick_fifo(struct rq* rq, struct proc* p){
  extern struct proc* idleproc[];
  if(p == idleproc[cpu()])
    yield();
  else;
}

void load_balance_fifo(struct rq* rq){
  int max = 0;
  struct rq* src_rq = NULL;
  int i;
  struct proc* procs_moved[NPROC/2];
  int num_procs_moved;
  
  // find out the busiest rq
  for (i = 0; i < ncpu; i++){
    if (rqs[i].proc_num > max){
      src_rq = &(rqs[i]);
      max = src_rq->proc_num;
    }
  }
  if(src_rq == NULL)
    return;

  // Get the proc from src_rq
  acquire(&(src_rq->rq_lock));
  num_procs_moved = src_rq->sched_class->get_proc(src_rq, procs_moved);
  release(&(src_rq->rq_lock));
  if (num_procs_moved != 0)
    cprintf("Load balance\n");
  acquire(&(rq->rq_lock));
  for (i = 0; i < num_procs_moved; i++)
    enqueue_proc(rq, procs_moved[i]);
  
  release(&(rq->rq_lock));

  return;
}

int get_proc_fifo(struct rq* rq, struct proc* procs_moved[]){
  int num_procs_moved = rq->proc_num/2;
  int i;
  struct rq_node *next;

  struct rq_node* cnode = rq->next_to_run;
  for (i = 0; i < num_procs_moved; i++){
    if (cnode->next == NULL){
      cprintf("Wrong here: proc_num = %d\n", rq->proc_num);
      panic("Not enough proc to move in get_proc_fifo\n");
    }
    next = cnode->next;
    if(next == rq->last_to_run)
      rq->last_to_run = cnode;
    procs_moved[i] = next->proc;
    cnode->next = next->next;
    next->next = rq->free_list;
    rq->free_list = next;
  }
  rq->proc_num -= num_procs_moved;

  return num_procs_moved;
}

const struct sched_class sched_class_fifo = {
  .init_rq 		= init_rq_fifo,
  .enqueue_proc		= enqueue_proc_fifo,
  .dequeue_proc		= dequeue_proc_fifo,
  .yield_proc		= yield_proc_fifo,
  .pick_next_proc	= pick_next_proc_fifo,
  .proc_tick		= proc_tick_fifo,
  .load_balance		= load_balance_fifo,
  .get_proc		= get_proc_fifo,
};

