/*
  Quick test code for MKL convolution.
  - richard.m.veras@ou.edu


  See:
  https://gist.github.com/coder1234/4a1f661f1272dd6083e2
  https://www.intel.com/content/www/us/en/docs/onemkl/developer-reference-c/2025-0/convolution-and-correlation.html
  https://stackoverflow.com/questions/5248915/execution-time-of-c-program
  https://www.smcm.iqfr.csic.es/docs/intel/mkl/mkl_manual/sf/sf_usageex.htm


https://www.intel.com/content/www/us/en/docs/onemkl/developer-reference-c/2024-0/vslconvsetstart-vslcorrsetstart.html
  The vslConvSetStart/vslCorrSetStart routine sets the value of the parameter start for the operation of convolution or correlation. In a one-dimensional case, this parameter points to the first element in the mathematical result that should be stored in the output array. In a multidimensional case, start is an array of indices and its length is equal to the number of dimensions specified by the parameter dims. For more information about the definition and effect of this parameter, see Data Allocation.

During the initial task descriptor construction, the default value for start is undefined and this parameter is not used. Therefore the only way to set and use the start parameter is via assigning it some value by one of the vslConvSetStart/vslCorrSetStart routines.
 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "mkl_vsl.h"

int scond1(
    float h[], int inch,
    float x[], int incx,
    float y[], int incy,
    int nh, int nx,
    int iy0, // Sets the value of the parameter start for the operation of convolution
    int ny)

{

    int status;
    VSLConvTaskPtr task;

    vslsConvNewTask1D(&task,VSL_CONV_MODE_DIRECT,nh,nx,ny);
    vslConvSetStart(task, &iy0);
    status = vslsConvExec1D(task, h,inch, x,incx, y,incy);
    vslConvDeleteTask(&task);

    return status;

}


int main(int argc, char *argv[])
{

  // TODO: These should all be command line parameters.
  int num_runs;
  int start;
  int step;
  int stop;
  
  int size_w;
  int stride_x;
  int stride_y;
  int stride_w;

  if( argc == 1 )
    {
      num_runs = 1000;
      start = 16;
      step  = 16;
      stop  = 1024;
  
      size_w = 3;
      stride_x = 1;
      stride_y = 1;
      stride_w = 1;

    }
  else if (argc == 1+8)
    {
      num_runs = atoi(argv[1]);
      start = atoi(argv[2]);
      step  = atoi(argv[3]);
      stop  = atoi(argv[4]);
  
      size_w = atoi(argv[5]);
      stride_x = atoi(argv[6]);
      stride_y = atoi(argv[7]);
      stride_w = atoi(argv[8]);

    }
  else
    {
      printf("USAGE: %s NUM_RUNS START STEP STOP SIZE_W STRIDE_X STRIDE_Y STRIDE_W\n", argv[0]);
      exit(1);
    }


  
  // TODO: fill this with data.
  float *w = (float *)malloc(sizeof(float)*size_w*stride_w);


  printf("size, size_w, stride_x, stride_y, stride_w, time-s, flop-per-second\n");
  for( int sz = start; sz < stop; sz+=step )
    {
      int size_x = sz;
      int size_y = sz+size_w-1;

      // TODO: fill this with data.
      float *x = (float *)malloc(sizeof(float)*size_x*stride_x);
      float *y = (float *)malloc(sizeof(float)*size_y*stride_y);
      

      clock_t begin = clock();
      for( int run = 0; run < num_runs; ++run )
	{
	  int iy0 = 0; // Sets the value of the parameter start for the operation of convolution

	

	  scond1(
		 w, stride_w,
		 x, stride_x,
		 y, stride_y,
		 size_w, size_x,
		 iy0,
		 size_y);
	}
      clock_t end = clock();
      double time_spent = (double)(end - begin) / ((double)((double)CLOCKS_PER_SEC)*(num_runs));
      double gflop_per_second = ((double)2*size_x*size_w) /  (time_spent*1e9);

      printf("%i, %i, %i, %i, %i, %e, %e\n",
	     sz,
	     size_w, stride_x, stride_y, stride_w,
	     time_spent, gflop_per_second);
      
      free(x);
      free(y);

    }

  free(w);

  return 0;
}
