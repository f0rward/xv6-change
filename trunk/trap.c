#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "syscall.h"
#include "rq.h"
#include "pmap.h"
#include "memlayout.h"

//#define LOAD_BALANCE_ON

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
int ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
  
  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Falt",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}


void
print_trapframe(struct trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);

	cprintf("  edi  0x%x\n", tf->edi);
	cprintf("  esi  0x%x\n", tf->esi);
	cprintf("  ebp  0x%x\n", tf->ebp);
	cprintf("  oesp 0x%x\n", tf->oesp);
	cprintf("  ebx  0x%x\n", tf->ebx);
	cprintf("  edx  0x%x\n", tf->edx);
	cprintf("  ecx  0x%x\n", tf->ecx);
	cprintf("  eax  0x%x\n", tf->eax);

	cprintf("  es   0x----%x\n", tf->es);
	cprintf("  ds   0x----%x\n", tf->ds);
	cprintf("  trap 0x%x %s\n", tf->trapno, trapname(tf->trapno));
	cprintf("  err  0x%x\n", tf->err);
	cprintf("  eip  0x%x\n", tf->eip);
	cprintf("  cs   0x----%x\n", tf->cs);
	cprintf("  flag 0x%x\n", tf->eflags);
	cprintf("  esp  0x%x\n", tf->esp);
	cprintf("  ss   0x----%x\n", tf->ss);
}

void
trap(struct trapframe *tf)
{
  //print_trapframe(tf); procdump(); //chy for debug
  if (cp!=NULL)
     dbmsg("trap frame from %x %x, cp name %s\n",tf->eip, tf->trapno,cp->name);
  if(tf->trapno == T_SYSCALL){
    if(cp->killed)
      exit();
    cp->tf = tf;
    syscall();
    if(cp->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  cprintf("interrupt %x, trap frame : %x\n",tf->trapno, (uint)tf);
  case IRQ_OFFSET + IRQ_TIMER:
    if(cpu() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapic_eoi();
    break;
  case IRQ_OFFSET + IRQ_IDE:
    ide_intr();
    lapic_eoi();
    break;
  case IRQ_OFFSET + IRQ_KBD:
    kbd_intr();
    lapic_eoi();
    break;
  case IRQ_OFFSET + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpu(), tf->cs, tf->eip);
    lapic_eoi();
    break;
    
  default:
    if(cp == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x esp %x cr2 %x, pid %d %s \n",
              tf->trapno, cpu(), tf->eip, tf->esp, rcr2(),cp->pid, cp->name);
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d eip %x cr2 %x -- kill proc\n",
            cp->pid, cp->name, tf->trapno, tf->err, cpu(), tf->eip, rcr2());
    cp->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running 
  // until it gets to the regular system call return.)
  if(cp && cp->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(cp && cp->state == RUNNING && tf->trapno == IRQ_OFFSET+IRQ_TIMER){
#ifdef LOAD_BALANCE_ON
    if(cp == idleproc[cpu()]){
      struct rq* rq = theCpu.rq;
      rq->sched_class->load_balance(rq);
    }
#endif
    proc_tick(theCpu.rq, cp);
  }
}
