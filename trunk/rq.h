#ifndef _RQ_H_
#define _RQ_H_

#include "proc.h"
#define NULL 0

struct rq_node{
  struct proc* proc;	// real data
  struct rq_node* next; // double link list, if this rq_node is free,
  			// next will point to the next free rq_node
  struct rq_node* prev;	
};

struct rq{
  struct spinlock rq_lock;
  struct rq_node nodes[NPROC];  // rq_node table
  
  struct rq_node* next_to_run;	// next proc to run
  struct rq_node* last_to_run;  // last proc to run
  struct rq_node* free_list;	// all the free proc
  
  // by cxyzs7
  int max_slices;		// Used for RR
  struct sched_class* sched_class;
  struct rq* next;		// next run queue (used in mlfq)
  struct sched_class* sub_sched_class; // used in mlfq

  // by jimmy:
  int proc_num;
};

extern struct rq rqs[];

#endif
