#ifndef _PARAM_H_
#define _PARAM_H_
#define NPROC        64  // maximum number of processes
#define PAGE       4096  // granularity of user-space memory allocation
#define KSTACKSIZE 8*PAGE  // size of per-process kernel stack
#define NCPU          1  // maximum number of CPUs, this parameter is not used.
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NBUF         10  // size of disk block cache
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#endif
