/*
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

/*
 * Walsh transforms belong to a class of generalized Fourier transformations.
 * They have applications in various fields of electrical engineering
 * and numeric theory. In this sample we demonstrate efficient implementation
 * of naturally-ordered Walsh transform
 * (also known as Walsh-Hadamard or Hadamard transform) in CUDA and its
 * particular application to dyadic convolution computation.
 * Refer to excellent Jorg Arndt's "Algorithms for Programmers" textbook
 * http://www.jjj.de/fxt/fxtbook.pdf (Chapter 22)
 *
 * Victor Podlozhnyuk (vpodlozhnyuk@nvidia.com)
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>


////////////////////////////////////////////////////////////////////////////////
// Reference CPU FWT
////////////////////////////////////////////////////////////////////////////////
extern"C" void fwtCPU(float *h_Output, float *h_Input, int log2N);
extern"C" void slowWTcpu(float *h_Output, float *h_Input, int log2N);
extern "C" void dyadicConvolutionCPU(
    float *h_Result,
    float *h_Data,
    float *h_Kernel,
    int log2dataN,
    int log2kernelN
);

////////////////////////////////////////////////////////////////////////////////
// GPU FWT
////////////////////////////////////////////////////////////////////////////////
#include "kernels.cpp"

////////////////////////////////////////////////////////////////////////////////
// Data configuration
////////////////////////////////////////////////////////////////////////////////
const int log2Data = 23;
const int   dataN = 1 << log2Data;
const int   DATA_SIZE = dataN   * sizeof(float);

const int log2Kernel = 7;
const int kernelN = 1 << log2Kernel;
const int KERNEL_SIZE = kernelN * sizeof(float);

//const double NOPS = 3.0 * (double)dataN * (double)log2Data / 2.0;



////////////////////////////////////////////////////////////////////////////////
// Main program
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    double delta, ref, sum_delta2, sum_ref2, L2norm;

    int i;

    printf("Data length: %i; kernel length: %i\n", dataN, kernelN);

    printf("Initializing data...\n");
    float *h_Kernel    = (float *)malloc(KERNEL_SIZE);
    float *h_Data      = (float *)malloc(DATA_SIZE);
    float *h_ResultCPU = (float *)malloc(DATA_SIZE);

    srand(123);
    for (i = 0; i < kernelN; i++)
    {
        h_Kernel[i] = (float)rand() / (float)RAND_MAX;
    }

    for (i = 0; i < dataN; i++)
    {
        h_Data[i] = (float)rand() / (float)RAND_MAX;
    }

    printf("Running GPU dyadic convolution using Fast Walsh Transform...\n");

    float *d_Kernel = (float *)malloc(DATA_SIZE);
    float *d_Data   = (float *)malloc(DATA_SIZE);

#pragma omp target data map (alloc: d_Kernel[0:dataN], d_Data[0:dataN])
{
    for (i = 0; i < 1; i++)
    {
      memset(d_Kernel, 0, DATA_SIZE);
      memcpy(d_Kernel, h_Kernel, KERNEL_SIZE);
      #pragma omp target update to (d_Kernel[0:dataN])

      memcpy(d_Data, h_Data, DATA_SIZE);
      #pragma omp target update to (d_Data[0:dataN])

      fwtBatchGPU(d_Data, 1, log2Data);
      fwtBatchGPU(d_Kernel, 1, log2Data);
      modulateGPU(d_Data, d_Kernel, dataN);
      fwtBatchGPU(d_Data, 1, log2Data);
    }

    printf("Reading back GPU results...\n");
    #pragma omp target update from (d_Data[0:dataN])
}

    printf("Running straightforward CPU dyadic convolution...\n");
    dyadicConvolutionCPU(h_ResultCPU, h_Data, h_Kernel, log2Data, log2Kernel);

    printf("Comparing the results...\n");
    sum_delta2 = 0;
    sum_ref2   = 0;

    for (i = 0; i < dataN; i++)
    {
        delta       = h_ResultCPU[i] - d_Data[i];
        ref         = h_ResultCPU[i];
        sum_delta2 += delta * delta;
        sum_ref2   += ref * ref;
    }

    L2norm = sqrt(sum_delta2 / sum_ref2);

    printf("Shutting down...\n");
    free(h_ResultCPU);
    free(h_Data);
    free(h_Kernel);
    free(d_Data);
    free(d_Kernel);

    printf("L2 norm: %E\n", L2norm);
    printf(L2norm < 1e-6 ? "Test passed\n" : "Test failed!\n");
}
