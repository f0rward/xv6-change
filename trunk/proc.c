#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched.h"
#include "sched_fifo.h"
#include "sched_RR.h"
#include "sched_MLFQ.h"
#include "rq.h"
#include "pmap.h"
#include "memlayout.h"


//#define dbmsg(...) cprintf(__VA_ARGS__);

static char* rq_lock_name = "rqlock";

#define SCHED_SIMPLE
//#define SCHED_FIFO
//#define SCHED_RR
//#define SCHED_MLFQ

struct spinlock proc_table_lock;

struct proc proc[NPROC];
struct proc *initproc;

// by jimmy:
struct proc* idleproc[NCPU];

int nextpid = 1;
extern void forkret(void);
extern void forkret1(struct trapframe*);

void init_rq(struct rq *rq);
void enqueue_proc (struct rq *rq, struct proc *p);
void dequeue_proc (struct rq *rq, struct proc *p);
void yield_proc (struct rq *rq);
struct proc* pick_next_proc (struct rq *rq);
void proc_tick (struct rq* rq, struct proc* p);

void
pinit(void)
{  
  int i;
  initlock(&proc_table_lock, "proc_table");
//  for(i = 0; i < NCPU; i++)
//  {
    struct rq * rq = &rqs[0];
#ifdef SCHED_SIMPLE
    rq->sched_class = (struct sched_class *)&simple_sched_class;
    rq->sub_sched_class = (struct sched_class *)&simple_sched_class;
#endif
#ifdef SCHED_FIFO
    rq->sched_class = (struct sched_class *)&sched_class_fifo;
    rq->sub_sched_class = (struct sched_class *)&sched_class_fifo;
#endif
#ifdef SCHED_RR
    rq->sched_class = (struct sched_class *)&sched_class_RR;
    rq->sub_sched_class = (struct sched_class *)&sched_class_RR;
    rq->max_slices = 100;
#endif
#ifdef SCHED_MLFQ
    rq->next = &rqs[1];
    rq->next->next = &rqs[2];
    rq->sched_class = (struct sched_class *)&sched_class_MLFQ;
    rq->sub_sched_class = (struct sched_class *)&sched_class_RR;
    rq->max_slices = 20;
    rq->next->sched_class = (struct sched_class *)&sched_class_MLFQ;
    rq->next->sub_sched_class = (struct sched_class *)&sched_class_RR;
    rq->next->max_slices = 500;
    rq->next->next->sched_class = (struct sched_class *)&sched_class_MLFQ;
    rq->next->next->sub_sched_class = (struct sched_class *)&sched_class_fifo;
#endif

    init_rq(rq);
#ifdef SCHED_MLFQ
    init_rq(rq->next);
    init_rq(rq->next->next);
#endif
//  }
 // for(i = 0; i < NCPU; i++)
    theCpu.rq = &(rqs[0]);
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and return it.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  int i;
  struct proc *p;

  acquire(&proc_table_lock);
  for(i = 0; i < NPROC; i++){
    p = &proc[i];
    if(p->state == UNUSED){
      p->state = EMBRYO;
      p->pid = nextpid++;
      p->rq = NULL;
      release(&proc_table_lock);
      return p;
    }
  }
  release(&proc_table_lock);
  return 0;
}

// Grow current process's memory by n bytes.
// Return old size on success, -1 on failure.
int
growproc(int n)
{
  char *newmem;

  /*newmem = kalloc(cp->sz + n);
  if(newmem == 0)
    return -1;
  memmove(newmem, cp->mem, cp->sz);
  memset(newmem + cp->sz, 0, n);
  kfree(cp->mem, cp->sz);
  cp->mem = newmem;
  cp->sz += n;*/
  newmem = kalloc(n);
  if (newmem == 0)
    return -1;
  memset(newmem, 0, n);
  map_segment(cp->vm.pgdir, (paddr_t)newmem, KERNTOP + cp->sz, n, PTE_P | PTE_W | PTE_U);
  cp->sz += n;
  setupsegs(cp);
  return cp->sz - n;
}

// Set up CPU's segment descriptors and task state for a given process.
// If p==0, set up for "idle" state for when scheduler() is running.
void
setupsegs(struct proc *p)
{
  struct cpu *c;
  
  pushcli();
  c = &theCpu;
  c->ts.ss0 = SEG_KDATA << 3;
  if(p)
    c->ts.esp0 = (uint)(p->kstack + KSTACKSIZE);
  else
    c->ts.esp0 = 0xffffffff;

  c->gdt[0] = SEG_NULL;
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0x100000 + 64*1024-1, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_TSS] = SEG16(STS_T32A, (uint)&c->ts, sizeof(c->ts)-1, 0);
  c->gdt[SEG_TSS].s = 0;
  if(p){
    c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, (uint)KERNTOP, p->sz-1, DPL_USER);
    c->gdt[SEG_UDATA] = SEG(STA_W, (uint)KERNTOP, p->sz-1, DPL_USER);
    c->cr3 = (paddr_t)(p->vm.pgdir);
    dbmsg("process %s load cr3 %x\n",p->name, c->cr3);
  } 
  else {
    c->gdt[SEG_UCODE] = SEG_NULL;
    c->gdt[SEG_UDATA] = SEG_NULL;
    c->cr3 = (paddr_t)(boot_pgdir);  
    dbmsg("NULL process load cr3 %x for User NULL\n",c->cr3);
  }
  dbmsg("load gdt %x\n",c->gdt);
  lgdt(c->gdt, sizeof(c->gdt));
  dbmsg("load tr \n");
  ltr(SEG_TSS << 3);
  dbmsg("load cr3 %x\n",c->cr3);
  lcr3(c->cr3);
  dbmsg("load cr3 over\n");
  popcli();
  dbmsg("setupsegs over\n");
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
struct proc*
copyproc(struct proc *p)
{
  int i,ret;
  struct proc *np;
  char * kstack, * mem;
  pde_t * pgdir = 0;

  // Allocate process.
  if((np = allocproc()) == 0)
    return 0;

  // Allocate new page directory
  pgdir = (pde_t *)alloc_page();
  memset(pgdir, 0 ,PAGE);
  for (i = 0; i < PDX(KERNTOP); i++) {
    pgdir[i] = boot_pgdir[i];  
  }
  for (i = PDX(0xfec00000); i < PTENTRY; i++) {
    pgdir[i] = boot_pgdir[i];
  }
  np->vm.pgdir = pgdir;

  // Allocate kernel stack.
  if ((kstack = kalloc(KSTACKSIZE)) == 0) {
    np->state = UNUSED;
    kfree((char *)pgdir, PAGE);
    return 0;
  }

  np->kstack = kstack;
  np->k = kstack;
  // kernel read/write
  //ret = map_segment(pgdir, (paddr_t)kstack, (vaddr_t)np->kstack, KSTACKSIZE, PTE_P | PTE_W);
  //if (ret < 0)
  //  panic("kstack map error");
  np->tf = (struct trapframe*)(kstack + KSTACKSIZE) - 1;

  if(p){  // Copy process state from p.
    np->parent = p;
    memmove(np->tf, p->tf, sizeof(*np->tf));
  
    np->sz = p->sz;
    if((mem = kalloc(np->sz)) == 0){
      //kfree(np->kstack, KSTACKSIZE);
      do_unmap(np->vm.pgdir, (vaddr_t)p->kstack, KSTACKSIZE);
      np->kstack = 0;
      np->state = UNUSED;
      np->parent = 0;
      return 0;
    }
    memmove(mem, p->mem, np->sz);
    ret = map_segment(pgdir, (paddr_t)mem, KERNTOP, np->sz, PTE_P | PTE_W | PTE_U);
    np->mem = (char *)KERNTOP;

    for(i = 0; i < NOFILE; i++)
      if(p->ofile[i])
        np->ofile[i] = filedup(p->ofile[i]);
    np->cwd = idup(p->cwd);
  }

  // Set up new context to start executing at forkret (see below).
  memset(&np->context, 0, sizeof(np->context));
  np->context.eip = (uint)forkret;
  np->context.esp = (uint)np->tf;

  // Clear %eax so that fork system call returns 0 in child.
  np->tf->eax = 0;
  return np;
}

struct proc*
allocIdle(void){
  extern uchar _binary_idlecode_start[], _binary_idlecode_size[];
  char *mem;
  // init idle proc
  struct proc* p = copyproc(0);
  p->sz = PAGE;
  mem = kalloc(p->sz);
  p->cwd = namei("/");
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = p->sz;
  
  // Make return address readable; needed for some gcc.
  p->tf->esp -= 4;
  *(uint*)(mem + p->tf->esp) = 0xefefefef;

  // On entry to user space, start executing at beginning of idlecode.S.
  p->tf->eip = 0;
  memmove(mem, _binary_idlecode_start, (int)_binary_idlecode_size);
  map_segment(p->vm.pgdir, (paddr_t)mem, KERNTOP, p->sz, PTE_P | PTE_W | PTE_U);
  p->mem = (char *)KERNTOP;
  safestrcpy(p->name, "idle", sizeof(p->name));
  p->state = RUNNABLE;

  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern uchar _binary_initcode_start[], _binary_initcode_size[];
  char * mem;
  int i;

  p = copyproc(0);
  p->sz = PAGE;
  mem = kalloc(p->sz);
  p->cwd = namei("/");
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = p->sz;
  
  // Make return address readable; needed for some gcc.
  p->tf->esp -= 4;
  *(uint*)(mem + p->tf->esp) = 0xefefefef;
  dbmsg("init size %x\m", p->sz);

  // On entry to user space, start executing at beginning of initcode.S.
  p->tf->eip = 0;
  memmove(mem, _binary_initcode_start, (int)_binary_initcode_size);
  map_segment(p->vm.pgdir, (paddr_t)mem, KERNTOP, p->sz, PTE_P | PTE_W | PTE_U);
  p->mem = (char *)KERNTOP;
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->state = RUNNABLE;
  acquire(&(theCpu.rq->rq_lock));
  enqueue_proc(theCpu.rq, p);
  release(&(theCpu.rq->rq_lock));

  initproc = p;

//  for(i=0; i<NCPU; i++){
    p = idleproc[0] = allocIdle();
    safestrcpy(p->name, "idle", sizeof(p->name));
    p->name[4] = (char)(i+'0');
    p->name[5] = '\0';\
    cprintf("idleproc[%d]: %x\n", 0,p);
 // }
}

// Return currently running process.
struct proc*
curproc(void)
{
  struct proc *p;

  pushcli();
  p = theCpu.curproc;
  popcli();
  return p;
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
runIdle(void)
{
  struct proc *p;
  struct cpu *c;
  int i;

  c = &theCpu;
  p = idleproc[cpu()];

  sti();
  acquire(&(theCpu.rq->rq_lock));
  c->curproc = p;
  setupsegs(p);
  p->state = RUNNING;
  swtch(&c->context, &p->context);
  
}


// schedule is the only interface that can swictch CPU from one
// process another
// must acquire the spinlock of rq before call this and release it 
// after calling
void
schedule(void)
{
  struct cpu *c;
  struct proc* next, *prev;
  c = &theCpu;
  prev = cp;

  //if(cp is not running)
  //   remove it from the queue
  if((prev->state != RUNNABLE) && (prev->state != RUNNING))
  {
    dequeue_proc(theCpu.rq, prev);
  }
  // get next proc to run 
  next = pick_next_proc(theCpu.rq);
    

  // swtch
  c->curproc = next;
  
  setupsegs(next);
  //cprintf("s C ss \n"); //for test
  next->state = RUNNING;
  //if(next==initproc){ //chy for debug
   // cprintf("initproc cxt sp %x, ip %x\n",next->context.esp,next->context.eip);
  //}
  if(next != prev)
  {
    swtch(&prev->context, &next->context);
  }
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  //lock the rq
  acquire(&(theCpu.rq->rq_lock));
  cp->state = RUNNABLE;

  yield_proc(theCpu.rq);
  schedule();

  // unlock rq
  release(&(theCpu.rq->rq_lock));
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  // Still holding proc_table_lock from scheduler.
  release(&(theCpu.rq->rq_lock));
  //cprintf(" cp is %s\n",cp->name);//chy debug
  // Jump into assembly, never to return.
  forkret1(cp->tf);
}

// Atomically release lock and sleep on chan.
// Reacquires lock when reawakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(cp == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire proc_table_lock in order to
  // change p->state and then call sched.
  // Once we hold proc_table_lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with proc_table_lock locked),
  // so it's okay to release lk.
  if(lk == &proc_table_lock){
    acquire(&(theCpu.rq->rq_lock));
  }else if(lk == &(theCpu.rq->rq_lock)){
    acquire(&proc_table_lock);
  }else{
    acquire(&proc_table_lock);
    acquire(&(theCpu.rq->rq_lock));
    release(lk);
  }

  // Go to sleep.
  cp->chan = chan;
  cp->state = SLEEPING;
  release(&proc_table_lock);
  schedule();	

  // Tidy up.
  acquire(&proc_table_lock);
  cp->chan = 0;

  // Reacquire original lock.
  if(lk == &proc_table_lock){
    release(&(theCpu.rq->rq_lock));
  }else if(lk == &(theCpu.rq->rq_lock)){
    release(&proc_table_lock);
  }else{
    release(&proc_table_lock);
    release(&(theCpu.rq->rq_lock));
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// Proc_table_lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      enqueue_proc(theCpu.rq, p);
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&proc_table_lock);
  acquire(&(theCpu.rq->rq_lock));

  wakeup1(chan);

  release(&(theCpu.rq->rq_lock));
  release(&proc_table_lock);
}

// Kill the process with the given pid.
// Process won't actually exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&proc_table_lock);
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
        enqueue_proc(theCpu.rq, p);
      }
      release(&proc_table_lock);
      return 0;
    }
  }
  release(&proc_table_lock);
  return -1;
}

// Exit the current process.  Does not return.
// Exited processes remain in the zombie state
// until their parent calls wait() to find out they exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(cp == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(cp->ofile[fd]){
      fileclose(cp->ofile[fd]);
      cp->ofile[fd] = 0;
    }
  }

  iput(cp->cwd);
  cp->cwd = 0;

  acquire(&proc_table_lock);
  acquire(&(theCpu.rq->rq_lock));
  // Parent might be sleeping in wait().
  wakeup1(cp->parent);

  // Pass abandoned children to init.
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->parent == cp){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  cp->killed = 0;
  cp->state = ZOMBIE;
  //cprintf("proc %x exit\n",cp - proc);
  release(&proc_table_lock);
  schedule();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int i, havekids, pid;

  acquire(&proc_table_lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(i = 0; i < NPROC; i++){
      p = &proc[i];
      if(p->state == UNUSED)
        continue;
      if(p->parent == cp){
        if(p->state == ZOMBIE){
          // Found one.
          //kfree(p->mem, p->sz);
          kfree(p->kstack, KSTACKSIZE);
          //do_unmap(p->vm.pgdir, (vaddr_t)p->kstack, KSTACKSIZE);
          pid = p->pid;
          p->state = UNUSED;
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          release(&proc_table_lock);
          return pid;
        }
        havekids = 1;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || cp->killed){
      release(&proc_table_lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(cp, &proc_table_lock);
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i, j;
  struct proc *p;
  char *state;
  uint pc[10];

  acquire(&proc_table_lock);
 /* for(i=0; i<ncpu; i++)
  {
    if(i != cpu())
      acquire(&(theCpu.rq->rq_lock));
  }*/
  cprintf("************************\n"); 
  //for(i=0; i<ncpu; i++){
    p = theCpu.curproc;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
#ifndef SCHED_MLFQ
    cprintf("On cpu %d, nproc=%d : curproc_pid %d\n", 
	0, theCpu.rq->proc_num, p->pid);
#else
    cprintf("On cpu %d, rq1=%d, rq2=%d, rq3=%d : curproc_pid %d\n",
	0, theCpu.rq->proc_num, theCpu.rq->next->proc_num, theCpu.rq->next->next->proc_num, 
	p->pid);
#endif
 // }
   
  for(i = 0; i < NPROC; i++){
    p = &proc[i];
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context.ebp+2, pc);
      for(j=0; j<10 && pc[j] != 0; j++)
        cprintf(" %p", pc[j]);
    }
    cprintf("\n");
  }
  cprintf("************************\n");
 /* for(i = 0; i < ncpu; i++)
  {
    if(i != cpu())
    release(&(theCpu.rq->rq_lock));
  }*/
  release(&proc_table_lock); 
}

void init_rq(struct rq *rq)
{
  int i;
 
  // mark all the nodes is free
  for(i = 0; i < NPROC; i++){
    rq->nodes[i].proc = NULL;
    rq->nodes[i].next = &(rq->nodes[i]) + 1;
  }
  rq->nodes[NPROC-1].next = 0;
  rq->free_list = &(rq->nodes[0]);
  rq->next_to_run = NULL;
  rq->sub_sched_class->init_rq(rq);
  rq->proc_num = 0;

  // init lock
  initlock(&(rq->rq_lock), rq_lock_name);
}

void enqueue_proc(struct rq *rq, struct proc *p)
{
  if(!holding(&(rq->rq_lock)))
    panic("enqueue_proc no lock");

  // alloc
  if(rq->free_list == 0)
    panic("kernel panic: Do not support procs more than NPROC!(in enqueue_proc_simple)\n");

  rq->sched_class->enqueue_proc(rq, p);
}

void dequeue_proc (struct rq *rq, struct proc *p)
{
  if(!holding(&(rq->rq_lock)))
    panic("dequeue_proc no lock");

  if(p == idleproc[cpu()])
    return;
  rq->sched_class->dequeue_proc(rq, p);
}

void yield_proc (struct rq *rq)
{
  if(!holding(&(rq->rq_lock)))
    panic("yield no lock");
  rq->sched_class->yield_proc(rq);
}

struct proc* pick_next_proc(struct rq *rq)
{
  if(!holding(&(rq->rq_lock)))
    panic("pick_next_proc no lock");
  struct proc* p = rq->sched_class->pick_next_proc(rq);
  if (p)
    return p;
  else
    return idleproc[cpu()];
}

void proc_tick(struct rq* rq, struct proc* p)
{
  rq->sched_class->proc_tick(theCpu.rq, cp);
}

