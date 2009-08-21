#include "param.h"
#include "types.h"
#include "mmu.h"
#include "spinlock.h"
#include "rq.h"

struct rq rqs[NCPU*3];
