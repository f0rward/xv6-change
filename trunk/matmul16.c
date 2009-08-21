#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define MATSIZE 3

static int matA[MATSIZE][MATSIZE];
static int matB[MATSIZE][MATSIZE];
static int matC[MATSIZE][MATSIZE];

void
testMatMul()
{
  int i,j,k,iter;

  for(i=0; i<MATSIZE; i++)
    for(j=0; j<MATSIZE; j++)
    {
      matA[i][j] = 1;
      matB[i][j] = 1;
    }

  for(iter=0; iter<2000000; iter++)
  {
    // matmul
    for(i=0; i<MATSIZE; i++)
    {
      for(j=0; j<MATSIZE; j++)
      {
        int sum = 0;
        for(k=0; k<MATSIZE; k++)
      	{
          sum += matA[i][k] * matB[k][j];
        }
        matC[i][j] = sum;
      }
    }

    // store
    for(i=0; i<MATSIZE; i++)
      for(j=0; j<MATSIZE; j++)
        matB[i][j] = matC[i][j];
  }

  // output
/*  for(i=0; i<MATSIZE; i++)
      for(j=0; j<MATSIZE; j++)
        printf(1, "%d ", matB[i][j]);*/
  printf(1, "pid %d done!\n", getpid());

  return;
}

int
main(void){
  int i;
  int pid;

  for(i=0; i<16; i++){
    
    pid = fork();
    
    if(pid < 0){
      printf(1, "matmul16 fork failed!\n");
      exit();
    }
    if(pid == 0){
      //printf(1, "matm	ul16: test%d begin\n", i);
      testMatMul();
      exit();
    }
  }

  printf(1, "All children have been create!\n");
  i = 0;
  exit();
}
