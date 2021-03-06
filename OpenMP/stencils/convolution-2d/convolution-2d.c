/* POLYBENCH/GPU-OPENMP
 *
 * This file is a part of the Polybench/GPU-OpenMP suite
 *
 * Contact:
 * William Killian <killian@udel.edu>
 *
 * Copyright 2013, The University of Delaware
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

/* Include polybench common header. */
#include <polybench.h>

/* Include benchmark-specific header. */
/* Default data type is double, default size is 4096x4096. */
#include "convolution-2d.h"


/* Array initialization. */
static
void init_array (int ni, int nj,
		 DATA_TYPE POLYBENCH_2D(A,NI,NJ,ni,nj))
{
  int i, j;

  for (i = 0; i < ni; i++)
    for (j = 0; j < nj; j++)
      {
	A[i][j] = ((DATA_TYPE) (i + j) / nj);
      }
}


/* DCE code. Must scan the entire live-out data.
   Can be used also to check the correctness of the output. */
static
void print_array(int ni, int nj,
		 DATA_TYPE POLYBENCH_2D(B,NI,NJ,ni,nj))

{
  int i, j;

  for (i = 0; i < ni; i++)
    for (j = 0; j < nj; j++) {
      fprintf(stderr, DATA_PRINTF_MODIFIER, B[i][j]);
      if ((i * NJ + j) % 20 == 0) fprintf(stderr, "\n");
    }
  fprintf(stderr, "\n");
}


/* Main computational kernel. The whole function will be timed,
   including the call and return. */
#ifndef OMP_OFFLOAD
//static
void kernel_conv2d(int ni,
		   int nj,
		   DATA_TYPE POLYBENCH_2D(A,NI,NJ,ni,nj),
		   DATA_TYPE POLYBENCH_2D(B,NI,NJ,ni,nj))
{
  int i, j;
  #pragma scop
  #pragma omp parallel for private(j) collapse(2) schedule(static)
  for (i = 1; i < _PB_NI - 1; ++i)
  {
    for (j = 1; j < _PB_NJ - 1; ++j)
    {
	    B[i][j] =  0.2 * A[i-1][j-1] + 0.5 * A[i-1][j] + -0.8 * A[i-1][j+1]
              + -0.3 * A[ i ][j-1] + 0.6 * A[ i ][j] + -0.9 * A[ i ][j+1]
              +  0.4 * A[i+1][j-1] + 0.7 * A[i+1][j] +  0.1 * A[i+1][j+1];
    }
  }
  #pragma endscop
}
#elif defined POLYBENCH_OFFLOAD1D
//static
void kernel_conv2d(int ni,
		   int nj,
		   DATA_TYPE POLYBENCH_2D_1D(A,NI,NJ,ni,nj),
		   DATA_TYPE POLYBENCH_2D_1D(B,NI,NJ,ni,nj))
{
  int i, j;
#define A_IDX(i,j) IDX2(A,i,j,ni,nj)
#define B_IDX(i,j) IDX2(B,i,j,ni,nj)
  #pragma scop
  #pragma omp target data map(to: A[:ni*nj]) map(tofrom: B[:ni*nj])
  #pragma omp target teams distribute parallel for private(j) schedule(static)
  for (i = 1; i < _PB_NI - 1; ++i)
  {
    for (j = 1; j < _PB_NJ - 1; ++j)
    {
	    GET_IDX2(B,i,j) =  0.2 * GET_IDX2(A,i-1,j-1) + 0.5 * GET_IDX2(A,i-1,j) + -0.8 * GET_IDX2(A,i-1,j+1)
              + -0.3 * GET_IDX2(A, i ,j-1) + 0.6 * GET_IDX2(A, i ,j) + -0.9 * GET_IDX2(A, i ,j+1)
              +  0.4 * GET_IDX2(A,i+1,j-1) + 0.7 * GET_IDX2(A,i+1,j) +  0.1 * GET_IDX2(A,i+1,j+1);
    }
  }
  #pragma endscop
}
#else
//static
void kernel_conv2d(int ni,
		   int nj,
		   DATA_TYPE POLYBENCH_2D(A,NI,NJ,ni,nj),
		   DATA_TYPE POLYBENCH_2D(B,NI,NJ,ni,nj))
{
  int i, j;
  #pragma scop
#ifdef OMP_DCAT
  #pragma omp target data map(to: A) map(tofrom: B)
#else
  #pragma omp target data map(to: A[:ni][:nj]) map(tofrom: B[:ni][:nj])
#endif
  #pragma omp target teams distribute parallel for private(j) schedule(static)
  for (i = 1; i < _PB_NI - 1; ++i)
  {
    for (j = 1; j < _PB_NJ - 1; ++j)
    {
	    B[i][j] =  0.2 * A[i-1][j-1] + 0.5 * A[i-1][j] + -0.8 * A[i-1][j+1]
              + -0.3 * A[ i ][j-1] + 0.6 * A[ i ][j] + -0.9 * A[ i ][j+1]
              +  0.4 * A[i+1][j-1] + 0.7 * A[i+1][j] +  0.1 * A[i+1][j+1];
    }
  }
  #pragma endscop
}
#endif


int main(int argc, char** argv)
{
  /* Retrieve problem size. */
  int ni = NI;
  int nj = NJ;

  /* Variable declaration/allocation. */
  DC_BEGIN();
  POLYBENCH_2D_ARRAY_DECL(A, DATA_TYPE, NI, NJ, ni, nj);
  DC_END();
  DC_BEGIN();
  POLYBENCH_2D_ARRAY_DECL(B, DATA_TYPE, NI, NJ, ni, nj);
  DC_END();


  /* Initialize array(s). */
  init_array (ni, nj, POLYBENCH_ARRAY(A));

  /* Start timer. */
  polybench_start_instruments;

  /* Run kernel. */
  kernel_conv2d (ni, nj, POLYBENCH_ARRAY(A), POLYBENCH_ARRAY(B));

  /* Stop and print timer. */
  polybench_stop_instruments;
  polybench_print_instruments;

  /* Prevent dead-code elimination. All live-out data must be printed
     by the function call in argument. */
  polybench_prevent_dce(print_array(ni, nj, POLYBENCH_ARRAY(B)));

  /* Be clean. */
  POLYBENCH_FREE_ARRAY(A);
  POLYBENCH_FREE_ARRAY(B);

  return 0;
}
