









#ifndef _PROC_H_
#define _PROC_H_

#include "spinlock.h"
#include "types.h"

// Segments in proc->gdt
#define SEG_KCODE 1  // kernel code
#define SEG_KDATA 2  // kernel data+stack
#define SEG_UCODE 3
#define SEG_UDATA 4
#define SEG_TSS   5  // this process's task state
#define NSEGS     6

// Saved registers for kernel context switches.
// Don't need to save all the %fs etc. segment registers,
// because they are constant across kernel contexts.
// Save all the regular registers so we don't need to care
// which are caller save, but not the return register %eax.
// (Not saving %eax just simplifies the switching code.)
// The layout of context must match code in swtch.S.
struct context {
  int eip;
  int esp;
  int ebx;
  int ecx;
  int edx;
  int esi;
  int edi;
  int ebp;
};

enum proc_state { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct proc_vm {
  pde_t * pgdir;            // Pointer to the page directory
  spinlock_t page_table_lock;  // Page table's spin lock
  vaddr_t start_code;       // Initial address of executable code
  vaddr_t end_code;         // End address of executable code
  vaddr_t start_data;       // Initial address of data
  vaddr_t end_data;         // End address of data
  vaddr_t start_stack;      // Initial address of user mode stack
};

// Per-process state
struct proc {
  char *mem;                // Start of process memory (kernel address)
  uint sz;                  // Size of process memory (bytes)
  char *kstack;             // Bottom of kernel stack for this process
  char *k;
  enum proc_state state;    // Process state
  int pid;                  // Process ID
  struct proc *parent;      // Parent process
  void *chan;               // If non-zero, sleeping on chan
  int killed;               // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;        // Current directory
  struct context context;   // Switch here to run process
  struct trapframe *tf;     // Trap frame for current interrupt
  struct proc_vm vm;        // Information about the process address space
  char name[16];            // Process name (debugging)
  int timeslice;
  struct rq* rq;
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap

// Per-CPU state
struct cpu {
  uchar apicid;               // Local APIC ID
  paddr_t cr3;                // cr3 register
  struct proc *curproc;       // Process currently running.
  struct context context;     // Switch here to enter scheduler
  struct taskstate ts;        // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];  // x86 global descriptor table
  volatile uint booted;        // Has the CPU started?
  int ncli;                   // Depth of pushcli nesting.
  int intena;                 // Were interrupts enabled before pushcli? 
  struct rq* rq;
};

extern struct cpu cpus[NCPU];
extern int ncpu;
extern struct proc *initproc;
extern struct proc *idleproc[];
extern struct spinlock proc_table_lock;

void schedule();
void proc_tick(struct rq* rq, struct proc* p);
void enqueue_proc(struct rq *rq, struct proc *p);
void dequeue_proc (struct rq *rq, struct proc *p);
struct proc* pick_next_proc(struct rq *rq);

struct sched_class{
  // Init the run queue
  void (*init_rq) (struct rq *rq);
  // put the proc into runqueue
  // this function must be called with rq_lock
  void (*enqueue_proc) (struct rq *rq, struct proc *p);
  // get the proc out runqueue
  // this function must be called with rq_lock
  void (*dequeue_proc) (struct rq *rq, struct proc *p);
  //yield
  // this function must be called with rq_lock
  void (*yield_proc) (struct rq *rq);
  // choose the next runnable task
  struct proc* (*pick_next_proc) (struct rq *rq);
  // dealer of the time-tick
  // this function must be called WITHOUT rq_lock
  void (*proc_tick)(struct rq* rq, struct proc* p);
  // load_balance
  // this function must be called WITHOUT rq_lock
  void (*load_balance)(struct rq* rq);
  // get some proc from this rq, used in load_balance
  // the return value is the num of proc we get
  // this function must be called with rq_lock
  int (*get_proc)(struct rq* rq, struct proc* procs_moved[]);
};

// "cp" is a short alias for curproc().
// It gets used enough to make this worthwhile.
#define cp curproc()
#endif
