#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched_MLFQ.h"

void init_rq_MLFQ(struct rq* rq){
  return;
}

void enqueue_proc_MLFQ(struct rq *rq, struct proc *p){
  rq->sub_sched_class->enqueue_proc(rq, p);
}

void dequeue_proc_MLFQ(struct rq *rq, struct proc *p){
  if (p->rq != NULL)
    p->rq->sub_sched_class->dequeue_proc(p->rq, p);
}

void yield_proc_MLFQ(struct rq *rq){
  if(rq->next_to_run)
    rq->sub_sched_class->yield_proc(rq);
  else if (rq->next->next_to_run)
    rq->next->sub_sched_class->yield_proc(rq->next);
  else if (rq->next->next->next_to_run)
    rq->next->next->sub_sched_class->yield_proc(rq->next->next);
  else;
}

struct proc* pick_next_proc_MLFQ(struct rq *rq){
  struct proc* p = rq->sub_sched_class->pick_next_proc(rq);
  if (p == NULL)
    p = rq->next->sub_sched_class->pick_next_proc(rq->next);
  if (p == NULL)
    p = rq->next->next->sub_sched_class->pick_next_proc(rq->next->next);
  return p;
}

void proc_tick_MLFQ(struct rq* rq, struct proc* p){
  if(p == idleproc[cpu()])
    yield();
  else
  {
    // In RR queue
    if (p->rq == rq || p->rq == rq->next)
    {
      p->timeslice--;
      if (p->timeslice == 0)
      {
        if (p->rq == rq)
          cprintf("Switch from 1 to 2\n");
        else
          cprintf("Switch from 2 to 3\n");
	acquire(&(rqs[cpu()].rq_lock));
        p->rq->sub_sched_class->dequeue_proc(p->rq, p);
        p->rq->next->sub_sched_class->enqueue_proc(p->rq->next, p);
	cp->state = RUNNABLE;
	schedule();
	release(&(rqs[cpu()].rq_lock));
      }
    }
    // In fifo queue
    else if (p->rq == rq->next->next)
    {
      p->rq->sub_sched_class->proc_tick(p->rq, p);
    }
  }
}

void load_balance_MLFQ(struct rq* rq){
  int max = 0;
  struct rq* src_rq = NULL;
  int i;
  struct proc* procs_moved[3][NPROC/2];
  int num_procs_moved[3];
  
  // find out the busiest rq
  for (i = 0; i < ncpu; i++){
    if (rqs[i].proc_num + rqs[i+NCPU].proc_num + rqs[i+2*NCPU].proc_num > max){
      src_rq = &(rqs[i]);
      max = rqs[i].proc_num + rqs[i+NCPU].proc_num + rqs[i+2*NCPU].proc_num;
    }
  }
  if (src_rq == NULL)
    return;

  // Get the proc from src_rq
  acquire(&(src_rq->rq_lock));
  num_procs_moved[0] = src_rq->sub_sched_class->get_proc(src_rq, procs_moved[0]);
  num_procs_moved[1] = src_rq->next->sub_sched_class->get_proc(src_rq->next, procs_moved[1]);
  num_procs_moved[2] = src_rq->next->next->sub_sched_class->get_proc(src_rq->next->next, procs_moved[2]);
  release(&(src_rq->rq_lock));

  if (num_procs_moved[0]+num_procs_moved[1]+num_procs_moved[2] != 0)
    cprintf("load_balance\n");

  acquire(&(rq->rq_lock));
  for (i = 0; i < num_procs_moved[0]; i++) {
    rq->sub_sched_class->enqueue_proc(rq, procs_moved[0][i]);
  }
  for (i = 0; i < num_procs_moved[1]; i++) {
    rq->next->sub_sched_class->enqueue_proc(rq->next, procs_moved[1][i]);
  }
  for (i = 0; i < num_procs_moved[2]; i++) {
    rq->next->next->sub_sched_class->enqueue_proc(rq->next->next, procs_moved[2][i]);
  }
  release(&(rq->rq_lock));

  return;
}

int get_proc_MLFQ(struct rq* rq, struct proc* procs_moved[]){
  return 0;
}

const struct sched_class sched_class_MLFQ = {
  .init_rq 		= init_rq_MLFQ,
  .enqueue_proc		= enqueue_proc_MLFQ,
  .dequeue_proc		= dequeue_proc_MLFQ,
  .yield_proc		= yield_proc_MLFQ,
  .pick_next_proc	= pick_next_proc_MLFQ,
  .proc_tick		= proc_tick_MLFQ,
  .load_balance		= load_balance_MLFQ,
  .get_proc		= get_proc_MLFQ,
};

