#include "black_scholes.h"
#include "gaussian.h"
#include "random.h" 
#include "util.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


/**
 * This function is what you compute for each iteration of
 * Black-Scholes.  You don't have to understand it; just call it.
 * "gaussian_random_number" is the current random number (from a
 * Gaussian distribution, which in our case comes from gaussrand1()).
 */

__global__ void stdDev(double *trials, double mean, double *result){
	int i = threadIdx.x + blockIdx.x*blockDim.x;
	int size = blockDim.x*gridDim.x;
	double sum = 0.0;
	for (k = i*256; k < (i+1)*256; k++){
		double diff = trials[k] - mean;
		sum += diff * diff / (double) size;
	}
	result[i] = sum;
}

static inline double 
black_scholes_value (const double S,
		     const double E,
		     const double r,
		     const double sigma,
		     const double T,
		     const double gaussian_random_number)
{
  const double current_value = S * exp ( (r - (sigma*sigma) / 2.0) * T + 
					 sigma * sqrt (T) * gaussian_random_number );
  return exp (-r * T) * 
    ((current_value - E < 0.0) ? 0.0 : current_value - E);
  /* return exp (-r * T) * max_double (current_value - E, 0.0); */
}


/**
 * Compute the standard deviation of trials[0 .. M-1].
 */
static double
black_scholes_stddev (void* the_args)
{
  black_scholes_args_t* args = (black_scholes_args_t*) the_args;
  const double mean = args->mean;
  double *trials = args->trials;
  const int M = args->M;
  double variance = 0.0;
  int k;
  int len = M/256;
  int numthreads = 256;
  int numblocks = len/256;
  double *result;
  int size = sizeof(double)*len;

  double *d_trials;
  double *d_result;
  double d_mean;

  result = (double *)malloc(size);
  CudaMalloc((void **)&d_trials, sizeof(trials));
  CudaMalloc((void **)&d_result, size);
  CudaMalloc((void **)&d_mean, sizeof(double));
  
  CudaMemCpy(d_trials, trials, sizeof(trials), CudaMemCpyHostToDevice);
  CudaMemCpy(d_mean, mean, sizeof(double), CudaMemCpyHostToDevice);

  stdDev<<numthreads, numblocks>>(d_trials, d_mean, d_result);

  CudaMemCpy(d_result, result, size, CudaMemCpyDeviceToHost);
  CudaFree(d_trials);
  CudaFree(d_mean);
  CudaFree(d_result);

  for (k = 0; k < len; k++)
    {
      variance += result[k];
    }

  args->variance = variance;
  return sqrt (variance);
}


/**
 * Take a pointer to a black_scholes_args_t struct, and return NULL.
 * (The return value is irrelevant, because all the interesting
 * information is written to the input struct.)  This function runs
 * Black-Scholes iterations, and computes the local part of the mean.
 */

__global__ void randArray(double *trials){
	int index = threadIdx.x + blockIdx.x*blockDim.x;
	int state = time(NULL) + index;
	gaussrand_state_t gaussrand_states;
	init_gaussrand_state(&(gaussrand_states));
	for (k = i*256; k < (i+1)*256; k++){
		double randNums = gaussrand1 (&uniform_random_double,
							&states,
							&(gaussrand_states));

      		trials[k] = black_scholes_value (S, E, r, sigma, T, randNums);
	}
}

__global__ void mean(double *trials, double *result){
	int i = threadIdx.x + blockIdx.x*blockDim.x;
	int size = blockDim.x*gridDim.x;
	doubel sum = 0.0;
	for (k = i*256; k < (i+1)*256; k++){
		sum += trials[k] / (double) M;
	}
	result[i] = sum;
}

static void*
black_scholes_iterate (void* the_args)
{
  black_scholes_args_t* args = (black_scholes_args_t*) the_args;

  /* Unpack the IN/OUT struct */

  /* IN (read-only) parameters */
  const int S = args->S;
  const int E = args->E;
  const int M = args->M;
  const double r = args->r;
  const double sigma = args->sigma;
  const double T = args->T;

  /* OUT (write-only) parameters */
  double* trials = args->trials;
  double mean = 0.0;

  /* Temporary variables */
  int k;

  double *d_trials;
  CudaMalloc((void **)&d_trials, M * sizeof(double));

  int numthreads = 256;
  int numblocks = M/256;

  randArray<<numthreads, numblocks>>(d_trials);

  CudaMemCpy(d_trials, trials, M * sizeof(double), CudaMemCpyDeviceToHost);
  CudaFree(d_trials);

  int len = M/256;
  int numthreads = 256;
  int numblocks = len/256;
  double *result;
  int size = sizeof(double)*len;

  double *d_trials;
  double *d_result;

  result = (double *)malloc(size);
  CudaMalloc((void **)&d_trials, sizeof(trials));
  CudaMalloc((void **)&d_result, size);
  
  CudaMemCpy(d_trials, trials, sizeof(trials), CudaMemCpyHostToDevice);
  mean<<numthreads, numblocks>>(d_trials, d_result);

  CudaMemCpy(d_result, result, size, CudaMemCpyDeviceToHost);
  CudaFree(d_trials);
  CudaFree(d_mean);
  CudaFree(d_result);

  for (k = 0; k < len; k++)
    {
      mean += result[k];
    }


  /* Pack the OUT values into the args struct */
  args->mean = mean;

  /* 
   * We do the standard deviation computation as a second operation.
   */

  // free_prng_stream (prng_stream);
  return NULL;
}



void
black_scholes (confidence_interval_t* interval,
	       const double S,
	       const double E,
	       const double r,
	       const double sigma,
	       const double T,
	       const int M)
{
  black_scholes_args_t args;
  double mean = 0.0;
  double stddev = 0.0;
  double conf_width = 0.0;
  double* trials = NULL;

  assert (M > 0);
  trials = (double*) malloc (M * sizeof (double));
  assert (trials != NULL);

  args.S = S;
  args.E = E;
  args.r = r;
  args.sigma = sigma;
  args.T = T;
  args.M = M;
  args.trials = trials;
  args.mean = 0.0;
  args.variance = 0.0;

  (void) black_scholes_iterate (&args);
  mean = args.mean;
  stddev = black_scholes_stddev (&args);

  conf_width = 1.96 * stddev / sqrt ((double) M);
  interval->min = mean - conf_width;
  interval->max = mean + conf_width;

  /* Clean up and exit */
    
  deinit_black_scholes_args (&args);
}


void
deinit_black_scholes_args (black_scholes_args_t* args)
{
  if (args != NULL)
    if (args->trials != NULL)
      {
	free (args->trials);
	args->trials = NULL;
      }
}

