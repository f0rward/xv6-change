# Initial process execs /idle.

#include "syscall.h"
#include "traps.h"

# exec(idle, argv)
.globl start
start:
  nop
  nop
  push %eax
  pop %eax
  nop
  nop
  jmp start
  pushl $argv
  pushl $idle
  pushl $0
  movl $SYS_exec, %eax
  int $T_SYSCALL

# for(;;) exit();
exit:
  movl $SYS_exit, %eax
  int $T_SYSCALL
  jmp exit

# char idle[] = "/idle\0";
idle:
  .string "/idle\0"

# char *argv[] = { idle, 0 };
.p2align 2
argv:
  .long idle
  .long 0

