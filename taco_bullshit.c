#ifndef TACO_C_HEADERS
#define TACO_C_HEADERS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <complex.h>
#include <string.h>
#if _OPENMP
#include <omp.h>
#endif
#define TACO_MIN(_a,_b) ((_a) < (_b) ? (_a) : (_b))
#define TACO_MAX(_a,_b) ((_a) > (_b) ? (_a) : (_b))
#define TACO_DEREF(_a) (((___context___*)(*__ctx__))->_a)
#ifndef TACO_TENSOR_T_DEFINED
#define TACO_TENSOR_T_DEFINED
typedef enum { taco_mode_dense, taco_mode_sparse } taco_mode_t;
typedef struct {
  int32_t      order;         // tensor order (number of modes)
  int32_t*     dimensions;    // tensor dimensions
  int32_t      csize;         // component size
  int32_t*     mode_ordering; // mode storage ordering
  taco_mode_t* mode_types;    // mode storage types
  uint8_t***   indices;       // tensor index data (per mode)
  uint8_t*     vals;          // tensor values
  uint8_t*     fill_value;    // tensor fill value
  int32_t      vals_size;     // values array size
} taco_tensor_t;
#endif
#if !_OPENMP
int omp_get_thread_num() { return 0; }
int omp_get_max_threads() { return 1; }
#endif
int cmp(const void *a, const void *b) {
  return *((const int*)a) - *((const int*)b);
}
int taco_gallop(int *array, int arrayStart, int arrayEnd, int target) {
  if (array[arrayStart] >= target || arrayStart >= arrayEnd) {
    return arrayStart;
  }
  int step = 1;
  int curr = arrayStart;
  while (curr + step < arrayEnd && array[curr + step] < target) {
    curr += step;
    step = step * 2;
  }

  step = step / 2;
  while (step > 0) {
    if (curr + step < arrayEnd && array[curr + step] < target) {
      curr += step;
    }
    step = step / 2;
  }
  return curr+1;
}
int taco_binarySearchAfter(int *array, int arrayStart, int arrayEnd, int target) {
  if (array[arrayStart] >= target) {
    return arrayStart;
  }
  int lowerBound = arrayStart; // always < target
  int upperBound = arrayEnd; // always >= target
  while (upperBound - lowerBound > 1) {
    int mid = (upperBound + lowerBound) / 2;
    int midValue = array[mid];
    if (midValue < target) {
      lowerBound = mid;
    }
    else if (midValue > target) {
      upperBound = mid;
    }
    else {
      return mid;
    }
  }
  return upperBound;
}
int taco_binarySearchBefore(int *array, int arrayStart, int arrayEnd, int target) {
  if (array[arrayEnd] <= target) {
    return arrayEnd;
  }
  int lowerBound = arrayStart; // always <= target
  int upperBound = arrayEnd; // always > target
  while (upperBound - lowerBound > 1) {
    int mid = (upperBound + lowerBound) / 2;
    int midValue = array[mid];
    if (midValue < target) {
      lowerBound = mid;
    }
    else if (midValue > target) {
      upperBound = mid;
    }
    else {
      return mid;
    }
  }
  return lowerBound;
}
taco_tensor_t* init_taco_tensor_t(int32_t order, int32_t csize,
                                  int32_t* dimensions, int32_t* mode_ordering,
                                  taco_mode_t* mode_types) {
  taco_tensor_t* t = (taco_tensor_t *) malloc(sizeof(taco_tensor_t));
  t->order         = order;
  t->dimensions    = (int32_t *) malloc(order * sizeof(int32_t));
  t->mode_ordering = (int32_t *) malloc(order * sizeof(int32_t));
  t->mode_types    = (taco_mode_t *) malloc(order * sizeof(taco_mode_t));
  t->indices       = (uint8_t ***) malloc(order * sizeof(uint8_t***));
  t->csize         = csize;
  for (int32_t i = 0; i < order; i++) {
    t->dimensions[i]    = dimensions[i];
    t->mode_ordering[i] = mode_ordering[i];
    t->mode_types[i]    = mode_types[i];
    switch (t->mode_types[i]) {
      case taco_mode_dense:
        t->indices[i] = (uint8_t **) calloc(1, sizeof(uint8_t **));
        break;
      case taco_mode_sparse:
        t->indices[i] = (uint8_t **) calloc(2, sizeof(uint8_t **));
        break;
    }
  }
  return t;
}
void deinit_taco_tensor_t(taco_tensor_t* t) {
  for (int i = 0; i < t->order; i++) {
    free(t->indices[i]);
  }
  free(t->indices);
  free(t->dimensions);
  free(t->mode_ordering);
  free(t->mode_types);
  free(t);
}
#endif

int CSR_x_CSR_compute(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  double* restrict C_vals = (double*)(C->vals);
  int A1_dimension = (int)(A->dimensions[0]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);
  int B1_dimension = (int)(B->dimensions[0]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  int32_t jC = 0;

  for (int32_t i = 0; i < A1_dimension; i++) {
    for (int32_t kA = A2_pos[i]; kA < A2_pos[(i + 1)]; kA++) {
      int32_t k = A2_crd[kA];
      for (int32_t jB = B2_pos[k]; jB < B2_pos[(k + 1)]; jB++) {
        C_vals[jC] = 0.0;
        C_vals[jC] = C_vals[jC] + A_vals[kA] * B_vals[jB];
        jC++;
      }
    }
  }
  return 0;
}

int CSR_x_CSR_assemble(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  int* restrict C1_pos = (int*)(C->indices[0][0]);
  int* restrict C1_crd = (int*)(C->indices[0][1]);
  int* restrict C2_crd = (int*)(C->indices[1][1]);
  double* restrict C_vals = (double*)(C->vals);
  int A1_dimension = (int)(A->dimensions[0]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  int B1_dimension = (int)(B->dimensions[0]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);

  C1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  C1_pos[0] = 0;
  int32_t C1_crd_size = 1048576;
  C1_crd = (int32_t*)malloc(sizeof(int32_t) * C1_crd_size);
  int32_t C2_crd_size = 1048576;
  C2_crd = (int32_t*)malloc(sizeof(int32_t) * C2_crd_size);
  int32_t jC = 0;


  for (int32_t i = 0; i < A1_dimension; i++) {
    for (int32_t kA = A2_pos[i]; kA < A2_pos[(i + 1)]; kA++) {
      int32_t k = A2_crd[kA];
      for (int32_t jB = B2_pos[k]; jB < B2_pos[(k + 1)]; jB++) {
        int32_t j = B2_crd[jB];
        if (C2_crd_size <= jC) {
          int32_t C2_crd_new_size = TACO_MAX(C2_crd_size * 2,(jC + 1));
          C2_crd = (int32_t*)realloc(C2_crd, sizeof(int32_t) * C2_crd_new_size);
          C2_crd_size = C2_crd_new_size;
        }
        C2_crd[jC] = j;
        if (C1_crd_size <= jC) {
          C1_crd = (int32_t*)realloc(C1_crd, sizeof(int32_t) * (C1_crd_size * 2));
          C1_crd_size *= 2;
        }
        C1_crd[jC] = i;
        jC++;
      }
    }
  }

  C1_pos[1] = jC;

  C_vals = (double*)malloc(sizeof(double) * jC);

  C->indices[0][0] = (uint8_t*)(C1_pos);
  C->indices[0][1] = (uint8_t*)(C1_crd);
  C->indices[1][1] = (uint8_t*)(C2_crd);
  C->vals = (uint8_t*)C_vals;
  return 0;
}

int CSR_x_CSR_pack_A(taco_tensor_t *A, int* A_COO1_pos, int* A_COO1_crd, int* A_COO2_crd, double* A_COO_vals) {
  int A1_dimension = (int)(A->dimensions[0]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);

  A2_pos = (int32_t*)malloc(sizeof(int32_t) * (A1_dimension + 1));
  A2_pos[0] = 0;
  for (int32_t pA2 = 1; pA2 < (A1_dimension + 1); pA2++) {
    A2_pos[pA2] = 0;
  }
  int32_t A2_crd_size = 1048576;
  A2_crd = (int32_t*)malloc(sizeof(int32_t) * A2_crd_size);
  int32_t kA = 0;
  int32_t A_capacity = 1048576;
  A_vals = (double*)malloc(sizeof(double) * A_capacity);

  int32_t iA_COO = A_COO1_pos[0];
  int32_t pA_COO1_end = A_COO1_pos[1];

  while (iA_COO < pA_COO1_end) {
    int32_t i = A_COO1_crd[iA_COO];
    int32_t A_COO1_segend = iA_COO + 1;
    while (A_COO1_segend < pA_COO1_end && A_COO1_crd[A_COO1_segend] == i) {
      A_COO1_segend++;
    }
    int32_t pA2_begin = kA;

    int32_t kA_COO = iA_COO;

    while (kA_COO < A_COO1_segend) {
      int32_t k = A_COO2_crd[kA_COO];
      double A_COO_val = A_COO_vals[kA_COO];
      kA_COO++;
      while (kA_COO < A_COO1_segend && A_COO2_crd[kA_COO] == k) {
        A_COO_val += A_COO_vals[kA_COO];
        kA_COO++;
      }
      if (A_capacity <= kA) {
        A_vals = (double*)realloc(A_vals, sizeof(double) * (A_capacity * 2));
        A_capacity *= 2;
      }
      A_vals[kA] = A_COO_val;
      if (A2_crd_size <= kA) {
        A2_crd = (int32_t*)realloc(A2_crd, sizeof(int32_t) * (A2_crd_size * 2));
        A2_crd_size *= 2;
      }
      A2_crd[kA] = k;
      kA++;
    }

    A2_pos[i + 1] = kA - pA2_begin;
    iA_COO = A_COO1_segend;
  }

  int32_t csA2 = 0;
  for (int32_t pA20 = 1; pA20 < (A1_dimension + 1); pA20++) {
    csA2 += A2_pos[pA20];
    A2_pos[pA20] = csA2;
  }

  A->indices[1][0] = (uint8_t*)(A2_pos);
  A->indices[1][1] = (uint8_t*)(A2_crd);
  A->vals = (uint8_t*)A_vals;
  return 0;
}

int CSR_x_CSR_pack_B(taco_tensor_t *B, int* B_COO1_pos, int* B_COO1_crd, int* B_COO2_crd, double* B_COO_vals) {
  int B1_dimension = (int)(B->dimensions[0]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  B2_pos = (int32_t*)malloc(sizeof(int32_t) * (B1_dimension + 1));
  B2_pos[0] = 0;
  for (int32_t pB2 = 1; pB2 < (B1_dimension + 1); pB2++) {
    B2_pos[pB2] = 0;
  }
  int32_t B2_crd_size = 1048576;
  B2_crd = (int32_t*)malloc(sizeof(int32_t) * B2_crd_size);
  int32_t jB = 0;
  int32_t B_capacity = 1048576;
  B_vals = (double*)malloc(sizeof(double) * B_capacity);

  int32_t kB_COO = B_COO1_pos[0];
  int32_t pB_COO1_end = B_COO1_pos[1];

  while (kB_COO < pB_COO1_end) {
    int32_t k = B_COO1_crd[kB_COO];
    int32_t B_COO1_segend = kB_COO + 1;
    while (B_COO1_segend < pB_COO1_end && B_COO1_crd[B_COO1_segend] == k) {
      B_COO1_segend++;
    }
    int32_t pB2_begin = jB;

    int32_t jB_COO = kB_COO;

    while (jB_COO < B_COO1_segend) {
      int32_t j = B_COO2_crd[jB_COO];
      double B_COO_val = B_COO_vals[jB_COO];
      jB_COO++;
      while (jB_COO < B_COO1_segend && B_COO2_crd[jB_COO] == j) {
        B_COO_val += B_COO_vals[jB_COO];
        jB_COO++;
      }
      if (B_capacity <= jB) {
        B_vals = (double*)realloc(B_vals, sizeof(double) * (B_capacity * 2));
        B_capacity *= 2;
      }
      B_vals[jB] = B_COO_val;
      if (B2_crd_size <= jB) {
        B2_crd = (int32_t*)realloc(B2_crd, sizeof(int32_t) * (B2_crd_size * 2));
        B2_crd_size *= 2;
      }
      B2_crd[jB] = j;
      jB++;
    }

    B2_pos[k + 1] = jB - pB2_begin;
    kB_COO = B_COO1_segend;
  }

  int32_t csB2 = 0;
  for (int32_t pB20 = 1; pB20 < (B1_dimension + 1); pB20++) {
    csB2 += B2_pos[pB20];
    B2_pos[pB20] = csB2;
  }

  B->indices[1][0] = (uint8_t*)(B2_pos);
  B->indices[1][1] = (uint8_t*)(B2_crd);
  B->vals = (uint8_t*)B_vals;
  return 0;
}

int CSR_x_CSC_compute(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  double* restrict C_vals = (double*)(C->vals);
  int A1_dimension = (int)(A->dimensions[0]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);
  int B2_dimension = (int)(B->dimensions[1]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  int32_t jC = 0;

  for (int32_t i = 0; i < A1_dimension; i++) {
    for (int32_t j = 0; j < B2_dimension; j++) {
      double tkC_val = 0.0;
      int32_t kA = A2_pos[i];
      int32_t pA2_end = A2_pos[(i + 1)];
      int32_t kB = B2_pos[j];
      int32_t pB2_end = B2_pos[(j + 1)];

      while (kA < pA2_end && kB < pB2_end) {
        int32_t kA0 = A2_crd[kA];
        int32_t kB0 = B2_crd[kB];
        int32_t k = TACO_MIN(kA0,kB0);
        if (kA0 == k && kB0 == k) {
          tkC_val += A_vals[kA] * B_vals[kB];
        }
        kA += (int32_t)(kA0 == k);
        kB += (int32_t)(kB0 == k);
      }
      C_vals[jC] = tkC_val;
      jC++;
    }
  }
  return 0;
}

int CSR_x_CSC_assemble(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  int* restrict C1_pos = (int*)(C->indices[0][0]);
  int* restrict C1_crd = (int*)(C->indices[0][1]);
  int* restrict C2_crd = (int*)(C->indices[1][1]);
  double* restrict C_vals = (double*)(C->vals);
  int A1_dimension = (int)(A->dimensions[0]);
  int B2_dimension = (int)(B->dimensions[1]);

  C1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  C1_pos[0] = 0;
  int32_t C1_crd_size = 1048576;
  C1_crd = (int32_t*)malloc(sizeof(int32_t) * C1_crd_size);
  int32_t C2_crd_size = 1048576;
  C2_crd = (int32_t*)malloc(sizeof(int32_t) * C2_crd_size);
  int32_t jC = 0;


  for (int32_t i = 0; i < A1_dimension; i++) {
    for (int32_t j = 0; j < B2_dimension; j++) {
      if (C2_crd_size <= jC) {
        int32_t C2_crd_new_size = TACO_MAX(C2_crd_size * 2,(jC + 1));
        C2_crd = (int32_t*)realloc(C2_crd, sizeof(int32_t) * C2_crd_new_size);
        C2_crd_size = C2_crd_new_size;
      }
      C2_crd[jC] = j;
      if (C1_crd_size <= jC) {
        C1_crd = (int32_t*)realloc(C1_crd, sizeof(int32_t) * (C1_crd_size * 2));
        C1_crd_size *= 2;
      }
      C1_crd[jC] = i;
      jC++;
    }
  }

  C1_pos[1] = jC;

  C_vals = (double*)malloc(sizeof(double) * jC);

  C->indices[0][0] = (uint8_t*)(C1_pos);
  C->indices[0][1] = (uint8_t*)(C1_crd);
  C->indices[1][1] = (uint8_t*)(C2_crd);
  C->vals = (uint8_t*)C_vals;
  return 0;
}

int CSR_x_CSC_pack_A(taco_tensor_t *A, int* A_COO1_pos, int* A_COO1_crd, int* A_COO2_crd, double* A_COO_vals) {
  int A1_dimension = (int)(A->dimensions[0]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);

  A2_pos = (int32_t*)malloc(sizeof(int32_t) * (A1_dimension + 1));
  A2_pos[0] = 0;
  for (int32_t pA2 = 1; pA2 < (A1_dimension + 1); pA2++) {
    A2_pos[pA2] = 0;
  }
  int32_t A2_crd_size = 1048576;
  A2_crd = (int32_t*)malloc(sizeof(int32_t) * A2_crd_size);
  int32_t kA = 0;
  int32_t A_capacity = 1048576;
  A_vals = (double*)malloc(sizeof(double) * A_capacity);

  int32_t iA_COO = A_COO1_pos[0];
  int32_t pA_COO1_end = A_COO1_pos[1];

  while (iA_COO < pA_COO1_end) {
    int32_t i = A_COO1_crd[iA_COO];
    int32_t A_COO1_segend = iA_COO + 1;
    while (A_COO1_segend < pA_COO1_end && A_COO1_crd[A_COO1_segend] == i) {
      A_COO1_segend++;
    }
    int32_t pA2_begin = kA;

    int32_t kA_COO = iA_COO;

    while (kA_COO < A_COO1_segend) {
      int32_t k = A_COO2_crd[kA_COO];
      double A_COO_val = A_COO_vals[kA_COO];
      kA_COO++;
      while (kA_COO < A_COO1_segend && A_COO2_crd[kA_COO] == k) {
        A_COO_val += A_COO_vals[kA_COO];
        kA_COO++;
      }
      if (A_capacity <= kA) {
        A_vals = (double*)realloc(A_vals, sizeof(double) * (A_capacity * 2));
        A_capacity *= 2;
      }
      A_vals[kA] = A_COO_val;
      if (A2_crd_size <= kA) {
        A2_crd = (int32_t*)realloc(A2_crd, sizeof(int32_t) * (A2_crd_size * 2));
        A2_crd_size *= 2;
      }
      A2_crd[kA] = k;
      kA++;
    }

    A2_pos[i + 1] = kA - pA2_begin;
    iA_COO = A_COO1_segend;
  }

  int32_t csA2 = 0;
  for (int32_t pA20 = 1; pA20 < (A1_dimension + 1); pA20++) {
    csA2 += A2_pos[pA20];
    A2_pos[pA20] = csA2;
  }

  A->indices[1][0] = (uint8_t*)(A2_pos);
  A->indices[1][1] = (uint8_t*)(A2_crd);
  A->vals = (uint8_t*)A_vals;
  return 0;
}

int CSR_x_CSC_pack_B(taco_tensor_t *B, int* B_COO1_pos, int* B_COO1_crd, int* B_COO2_crd, double* B_COO_vals) {
  int B2_dimension = (int)(B->dimensions[1]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  B2_pos = (int32_t*)malloc(sizeof(int32_t) * (B2_dimension + 1));
  B2_pos[0] = 0;
  for (int32_t pB2 = 1; pB2 < (B2_dimension + 1); pB2++) {
    B2_pos[pB2] = 0;
  }
  int32_t B2_crd_size = 1048576;
  B2_crd = (int32_t*)malloc(sizeof(int32_t) * B2_crd_size);
  int32_t kB = 0;
  int32_t B_capacity = 1048576;
  B_vals = (double*)malloc(sizeof(double) * B_capacity);

  int32_t jB_COO = B_COO1_pos[0];
  int32_t pB_COO1_end = B_COO1_pos[1];

  while (jB_COO < pB_COO1_end) {
    int32_t j = B_COO1_crd[jB_COO];
    int32_t B_COO1_segend = jB_COO + 1;
    while (B_COO1_segend < pB_COO1_end && B_COO1_crd[B_COO1_segend] == j) {
      B_COO1_segend++;
    }
    int32_t pB2_begin = kB;

    int32_t kB_COO = jB_COO;

    while (kB_COO < B_COO1_segend) {
      int32_t k = B_COO2_crd[kB_COO];
      double B_COO_val = B_COO_vals[kB_COO];
      kB_COO++;
      while (kB_COO < B_COO1_segend && B_COO2_crd[kB_COO] == k) {
        B_COO_val += B_COO_vals[kB_COO];
        kB_COO++;
      }
      if (B_capacity <= kB) {
        B_vals = (double*)realloc(B_vals, sizeof(double) * (B_capacity * 2));
        B_capacity *= 2;
      }
      B_vals[kB] = B_COO_val;
      if (B2_crd_size <= kB) {
        B2_crd = (int32_t*)realloc(B2_crd, sizeof(int32_t) * (B2_crd_size * 2));
        B2_crd_size *= 2;
      }
      B2_crd[kB] = k;
      kB++;
    }

    B2_pos[j + 1] = kB - pB2_begin;
    jB_COO = B_COO1_segend;
  }

  int32_t csB2 = 0;
  for (int32_t pB20 = 1; pB20 < (B2_dimension + 1); pB20++) {
    csB2 += B2_pos[pB20];
    B2_pos[pB20] = csB2;
  }

  B->indices[1][0] = (uint8_t*)(B2_pos);
  B->indices[1][1] = (uint8_t*)(B2_crd);
  B->vals = (uint8_t*)B_vals;
  return 0;
}

int CSR_x_COO_compute(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  double* restrict C_vals = (double*)(C->vals);
  int A1_dimension = (int)(A->dimensions[0]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);
  int* restrict B1_pos = (int*)(B->indices[0][0]);
  int* restrict B1_crd = (int*)(B->indices[0][1]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  int32_t jC = 0;

  for (int32_t i = 0; i < A1_dimension; i++) {
    int32_t kA = A2_pos[i];
    int32_t pA2_end = A2_pos[(i + 1)];
    int32_t kB = B1_pos[0];
    int32_t pB1_end = B1_pos[1];

    while (kA < pA2_end && kB < pB1_end) {
      int32_t kA0 = A2_crd[kA];
      int32_t kB0 = B1_crd[kB];
      int32_t k = TACO_MIN(kA0,kB0);
      int32_t B1_segend = kB;
      while (B1_segend < pB1_end && B1_crd[B1_segend] == k) {
        B1_segend++;
      }
      if (kA0 == k && kB0 == k) {
        for (int32_t jB = kB; jB < B1_segend; jB++) {
          C_vals[jC] = 0.0;
          C_vals[jC] = C_vals[jC] + A_vals[kA] * B_vals[jB];
          jC++;
        }
      }
      kA += (int32_t)(kA0 == k);
      kB = B1_segend;
    }
  }
  return 0;
}

int CSR_x_COO_assemble(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  int* restrict C1_pos = (int*)(C->indices[0][0]);
  int* restrict C1_crd = (int*)(C->indices[0][1]);
  int* restrict C2_crd = (int*)(C->indices[1][1]);
  double* restrict C_vals = (double*)(C->vals);
  int A1_dimension = (int)(A->dimensions[0]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  int* restrict B1_pos = (int*)(B->indices[0][0]);
  int* restrict B1_crd = (int*)(B->indices[0][1]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);

  C1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  C1_pos[0] = 0;
  int32_t C1_crd_size = 1048576;
  C1_crd = (int32_t*)malloc(sizeof(int32_t) * C1_crd_size);
  int32_t C2_crd_size = 1048576;
  C2_crd = (int32_t*)malloc(sizeof(int32_t) * C2_crd_size);
  int32_t jC = 0;


  for (int32_t i = 0; i < A1_dimension; i++) {
    int32_t kA = A2_pos[i];
    int32_t pA2_end = A2_pos[(i + 1)];
    int32_t kB = B1_pos[0];
    int32_t pB1_end = B1_pos[1];

    while (kA < pA2_end && kB < pB1_end) {
      int32_t kA0 = A2_crd[kA];
      int32_t kB0 = B1_crd[kB];
      int32_t k = TACO_MIN(kA0,kB0);
      int32_t B1_segend = kB;
      while (B1_segend < pB1_end && B1_crd[B1_segend] == k) {
        B1_segend++;
      }
      if (kA0 == k && kB0 == k) {
        for (int32_t jB = kB; jB < B1_segend; jB++) {
          int32_t j = B2_crd[jB];
          if (C2_crd_size <= jC) {
            int32_t C2_crd_new_size = TACO_MAX(C2_crd_size * 2,(jC + 1));
            C2_crd = (int32_t*)realloc(C2_crd, sizeof(int32_t) * C2_crd_new_size);
            C2_crd_size = C2_crd_new_size;
          }
          C2_crd[jC] = j;
          if (C1_crd_size <= jC) {
            C1_crd = (int32_t*)realloc(C1_crd, sizeof(int32_t) * (C1_crd_size * 2));
            C1_crd_size *= 2;
          }
          C1_crd[jC] = i;
          jC++;
        }
      }
      kA += (int32_t)(kA0 == k);
      kB = B1_segend;
    }
  }

  C1_pos[1] = jC;

  C_vals = (double*)malloc(sizeof(double) * jC);

  C->indices[0][0] = (uint8_t*)(C1_pos);
  C->indices[0][1] = (uint8_t*)(C1_crd);
  C->indices[1][1] = (uint8_t*)(C2_crd);
  C->vals = (uint8_t*)C_vals;
  return 0;
}

int CSR_x_COO_pack_A(taco_tensor_t *A, int* A_COO1_pos, int* A_COO1_crd, int* A_COO2_crd, double* A_COO_vals) {
  int A1_dimension = (int)(A->dimensions[0]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);

  A2_pos = (int32_t*)malloc(sizeof(int32_t) * (A1_dimension + 1));
  A2_pos[0] = 0;
  for (int32_t pA2 = 1; pA2 < (A1_dimension + 1); pA2++) {
    A2_pos[pA2] = 0;
  }
  int32_t A2_crd_size = 1048576;
  A2_crd = (int32_t*)malloc(sizeof(int32_t) * A2_crd_size);
  int32_t kA = 0;
  int32_t A_capacity = 1048576;
  A_vals = (double*)malloc(sizeof(double) * A_capacity);

  int32_t iA_COO = A_COO1_pos[0];
  int32_t pA_COO1_end = A_COO1_pos[1];

  while (iA_COO < pA_COO1_end) {
    int32_t i = A_COO1_crd[iA_COO];
    int32_t A_COO1_segend = iA_COO + 1;
    while (A_COO1_segend < pA_COO1_end && A_COO1_crd[A_COO1_segend] == i) {
      A_COO1_segend++;
    }
    int32_t pA2_begin = kA;

    int32_t kA_COO = iA_COO;

    while (kA_COO < A_COO1_segend) {
      int32_t k = A_COO2_crd[kA_COO];
      double A_COO_val = A_COO_vals[kA_COO];
      kA_COO++;
      while (kA_COO < A_COO1_segend && A_COO2_crd[kA_COO] == k) {
        A_COO_val += A_COO_vals[kA_COO];
        kA_COO++;
      }
      if (A_capacity <= kA) {
        A_vals = (double*)realloc(A_vals, sizeof(double) * (A_capacity * 2));
        A_capacity *= 2;
      }
      A_vals[kA] = A_COO_val;
      if (A2_crd_size <= kA) {
        A2_crd = (int32_t*)realloc(A2_crd, sizeof(int32_t) * (A2_crd_size * 2));
        A2_crd_size *= 2;
      }
      A2_crd[kA] = k;
      kA++;
    }

    A2_pos[i + 1] = kA - pA2_begin;
    iA_COO = A_COO1_segend;
  }

  int32_t csA2 = 0;
  for (int32_t pA20 = 1; pA20 < (A1_dimension + 1); pA20++) {
    csA2 += A2_pos[pA20];
    A2_pos[pA20] = csA2;
  }

  A->indices[1][0] = (uint8_t*)(A2_pos);
  A->indices[1][1] = (uint8_t*)(A2_crd);
  A->vals = (uint8_t*)A_vals;
  return 0;
}

int CSR_x_COO_pack_B(taco_tensor_t *B, int* B_COO1_pos, int* B_COO1_crd, int* B_COO2_crd, double* B_COO_vals) {
  int* restrict B1_pos = (int*)(B->indices[0][0]);
  int* restrict B1_crd = (int*)(B->indices[0][1]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  B1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  B1_pos[0] = 0;
  int32_t B1_crd_size = 1048576;
  B1_crd = (int32_t*)malloc(sizeof(int32_t) * B1_crd_size);
  int32_t B2_crd_size = 1048576;
  B2_crd = (int32_t*)malloc(sizeof(int32_t) * B2_crd_size);
  int32_t jB = 0;
  int32_t B_capacity = 1048576;
  B_vals = (double*)malloc(sizeof(double) * B_capacity);


  int32_t kB_COO = B_COO1_pos[0];
  int32_t pB_COO1_end = B_COO1_pos[1];

  while (kB_COO < pB_COO1_end) {
    int32_t k = B_COO1_crd[kB_COO];
    int32_t B_COO1_segend = kB_COO + 1;
    while (B_COO1_segend < pB_COO1_end && B_COO1_crd[B_COO1_segend] == k) {
      B_COO1_segend++;
    }
    int32_t jB_COO = kB_COO;

    while (jB_COO < B_COO1_segend) {
      int32_t j = B_COO2_crd[jB_COO];
      double B_COO_val = B_COO_vals[jB_COO];
      jB_COO++;
      while (jB_COO < B_COO1_segend && B_COO2_crd[jB_COO] == j) {
        B_COO_val += B_COO_vals[jB_COO];
        jB_COO++;
      }
      if (B_capacity <= jB) {
        B_vals = (double*)realloc(B_vals, sizeof(double) * (B_capacity * 2));
        B_capacity *= 2;
      }
      B_vals[jB] = B_COO_val;
      if (B2_crd_size <= jB) {
        int32_t B2_crd_new_size = TACO_MAX(B2_crd_size * 2,(jB + 1));
        B2_crd = (int32_t*)realloc(B2_crd, sizeof(int32_t) * B2_crd_new_size);
        B2_crd_size = B2_crd_new_size;
      }
      B2_crd[jB] = j;
      if (B1_crd_size <= jB) {
        B1_crd = (int32_t*)realloc(B1_crd, sizeof(int32_t) * (B1_crd_size * 2));
        B1_crd_size *= 2;
      }
      B1_crd[jB] = k;
      jB++;
    }
    kB_COO = B_COO1_segend;
  }

  B1_pos[1] = jB;

  B->indices[0][0] = (uint8_t*)(B1_pos);
  B->indices[0][1] = (uint8_t*)(B1_crd);
  B->indices[1][1] = (uint8_t*)(B2_crd);
  B->vals = (uint8_t*)B_vals;
  return 0;
}

int CSC_x_CSR_compute(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  double* restrict C_vals = (double*)(C->vals);
  int A2_dimension = (int)(A->dimensions[1]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);
  int B1_dimension = (int)(B->dimensions[0]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  int32_t jC = 0;

  for (int32_t k = 0; k < B1_dimension; k++) {
    for (int32_t iA = A2_pos[k]; iA < A2_pos[(k + 1)]; iA++) {
      for (int32_t jB = B2_pos[k]; jB < B2_pos[(k + 1)]; jB++) {
        C_vals[jC] = 0.0;
        C_vals[jC] = C_vals[jC] + A_vals[iA] * B_vals[jB];
        jC++;
      }
    }
  }
  return 0;
}

int CSC_x_CSR_assemble(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  int* restrict C1_pos = (int*)(C->indices[0][0]);
  int* restrict C1_crd = (int*)(C->indices[0][1]);
  int* restrict C2_crd = (int*)(C->indices[1][1]);
  double* restrict C_vals = (double*)(C->vals);
  int A2_dimension = (int)(A->dimensions[1]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  int B1_dimension = (int)(B->dimensions[0]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);

  C1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  C1_pos[0] = 0;
  int32_t C1_crd_size = 1048576;
  C1_crd = (int32_t*)malloc(sizeof(int32_t) * C1_crd_size);
  int32_t C2_crd_size = 1048576;
  C2_crd = (int32_t*)malloc(sizeof(int32_t) * C2_crd_size);
  int32_t jC = 0;

  for (int32_t k = 0; k < B1_dimension; k++) {

    for (int32_t iA = A2_pos[k]; iA < A2_pos[(k + 1)]; iA++) {
      int32_t i = A2_crd[iA];
      for (int32_t jB = B2_pos[k]; jB < B2_pos[(k + 1)]; jB++) {
        int32_t j = B2_crd[jB];
        if (C2_crd_size <= jC) {
          int32_t C2_crd_new_size = TACO_MAX(C2_crd_size * 2,(jC + 1));
          C2_crd = (int32_t*)realloc(C2_crd, sizeof(int32_t) * C2_crd_new_size);
          C2_crd_size = C2_crd_new_size;
        }
        C2_crd[jC] = j;
        if (C1_crd_size <= jC) {
          C1_crd = (int32_t*)realloc(C1_crd, sizeof(int32_t) * (C1_crd_size * 2));
          C1_crd_size *= 2;
        }
        C1_crd[jC] = i;
        jC++;
      }
    }

    C1_pos[1] = jC;
  }

  C_vals = (double*)malloc(sizeof(double) * jC);

  C->indices[0][0] = (uint8_t*)(C1_pos);
  C->indices[0][1] = (uint8_t*)(C1_crd);
  C->indices[1][1] = (uint8_t*)(C2_crd);
  C->vals = (uint8_t*)C_vals;
  return 0;
}

int CSC_x_CSR_pack_A(taco_tensor_t *A, int* A_COO1_pos, int* A_COO1_crd, int* A_COO2_crd, double* A_COO_vals) {
  int A2_dimension = (int)(A->dimensions[1]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);

  A2_pos = (int32_t*)malloc(sizeof(int32_t) * (A2_dimension + 1));
  A2_pos[0] = 0;
  for (int32_t pA2 = 1; pA2 < (A2_dimension + 1); pA2++) {
    A2_pos[pA2] = 0;
  }
  int32_t A2_crd_size = 1048576;
  A2_crd = (int32_t*)malloc(sizeof(int32_t) * A2_crd_size);
  int32_t iA = 0;
  int32_t A_capacity = 1048576;
  A_vals = (double*)malloc(sizeof(double) * A_capacity);

  int32_t kA_COO = A_COO1_pos[0];
  int32_t pA_COO1_end = A_COO1_pos[1];

  while (kA_COO < pA_COO1_end) {
    int32_t k = A_COO1_crd[kA_COO];
    int32_t A_COO1_segend = kA_COO + 1;
    while (A_COO1_segend < pA_COO1_end && A_COO1_crd[A_COO1_segend] == k) {
      A_COO1_segend++;
    }
    int32_t pA2_begin = iA;

    int32_t iA_COO = kA_COO;

    while (iA_COO < A_COO1_segend) {
      int32_t i = A_COO2_crd[iA_COO];
      double A_COO_val = A_COO_vals[iA_COO];
      iA_COO++;
      while (iA_COO < A_COO1_segend && A_COO2_crd[iA_COO] == i) {
        A_COO_val += A_COO_vals[iA_COO];
        iA_COO++;
      }
      if (A_capacity <= iA) {
        A_vals = (double*)realloc(A_vals, sizeof(double) * (A_capacity * 2));
        A_capacity *= 2;
      }
      A_vals[iA] = A_COO_val;
      if (A2_crd_size <= iA) {
        A2_crd = (int32_t*)realloc(A2_crd, sizeof(int32_t) * (A2_crd_size * 2));
        A2_crd_size *= 2;
      }
      A2_crd[iA] = i;
      iA++;
    }

    A2_pos[k + 1] = iA - pA2_begin;
    kA_COO = A_COO1_segend;
  }

  int32_t csA2 = 0;
  for (int32_t pA20 = 1; pA20 < (A2_dimension + 1); pA20++) {
    csA2 += A2_pos[pA20];
    A2_pos[pA20] = csA2;
  }

  A->indices[1][0] = (uint8_t*)(A2_pos);
  A->indices[1][1] = (uint8_t*)(A2_crd);
  A->vals = (uint8_t*)A_vals;
  return 0;
}

int CSC_x_CSR_pack_B(taco_tensor_t *B, int* B_COO1_pos, int* B_COO1_crd, int* B_COO2_crd, double* B_COO_vals) {
  int B1_dimension = (int)(B->dimensions[0]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  B2_pos = (int32_t*)malloc(sizeof(int32_t) * (B1_dimension + 1));
  B2_pos[0] = 0;
  for (int32_t pB2 = 1; pB2 < (B1_dimension + 1); pB2++) {
    B2_pos[pB2] = 0;
  }
  int32_t B2_crd_size = 1048576;
  B2_crd = (int32_t*)malloc(sizeof(int32_t) * B2_crd_size);
  int32_t jB = 0;
  int32_t B_capacity = 1048576;
  B_vals = (double*)malloc(sizeof(double) * B_capacity);

  int32_t kB_COO = B_COO1_pos[0];
  int32_t pB_COO1_end = B_COO1_pos[1];

  while (kB_COO < pB_COO1_end) {
    int32_t k = B_COO1_crd[kB_COO];
    int32_t B_COO1_segend = kB_COO + 1;
    while (B_COO1_segend < pB_COO1_end && B_COO1_crd[B_COO1_segend] == k) {
      B_COO1_segend++;
    }
    int32_t pB2_begin = jB;

    int32_t jB_COO = kB_COO;

    while (jB_COO < B_COO1_segend) {
      int32_t j = B_COO2_crd[jB_COO];
      double B_COO_val = B_COO_vals[jB_COO];
      jB_COO++;
      while (jB_COO < B_COO1_segend && B_COO2_crd[jB_COO] == j) {
        B_COO_val += B_COO_vals[jB_COO];
        jB_COO++;
      }
      if (B_capacity <= jB) {
        B_vals = (double*)realloc(B_vals, sizeof(double) * (B_capacity * 2));
        B_capacity *= 2;
      }
      B_vals[jB] = B_COO_val;
      if (B2_crd_size <= jB) {
        B2_crd = (int32_t*)realloc(B2_crd, sizeof(int32_t) * (B2_crd_size * 2));
        B2_crd_size *= 2;
      }
      B2_crd[jB] = j;
      jB++;
    }

    B2_pos[k + 1] = jB - pB2_begin;
    kB_COO = B_COO1_segend;
  }

  int32_t csB2 = 0;
  for (int32_t pB20 = 1; pB20 < (B1_dimension + 1); pB20++) {
    csB2 += B2_pos[pB20];
    B2_pos[pB20] = csB2;
  }

  B->indices[1][0] = (uint8_t*)(B2_pos);
  B->indices[1][1] = (uint8_t*)(B2_crd);
  B->vals = (uint8_t*)B_vals;
  return 0;
}

int CSC_x_COO_compute(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  double* restrict C_vals = (double*)(C->vals);
  int A2_dimension = (int)(A->dimensions[1]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);
  int* restrict B1_pos = (int*)(B->indices[0][0]);
  int* restrict B1_crd = (int*)(B->indices[0][1]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  int32_t jC = 0;

  int32_t kB = B1_pos[0];
  int32_t pB1_end = B1_pos[1];

  while (kB < pB1_end) {
    int32_t k = B1_crd[kB];
    int32_t B1_segend = kB + 1;
    while (B1_segend < pB1_end && B1_crd[B1_segend] == k) {
      B1_segend++;
    }
    for (int32_t iA = A2_pos[k]; iA < A2_pos[(k + 1)]; iA++) {
      for (int32_t jB = kB; jB < B1_segend; jB++) {
        C_vals[jC] = 0.0;
        C_vals[jC] = C_vals[jC] + A_vals[iA] * B_vals[jB];
        jC++;
      }
    }
    kB = B1_segend;
  }
  return 0;
}

int CSC_x_COO_assemble(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  int* restrict C1_pos = (int*)(C->indices[0][0]);
  int* restrict C1_crd = (int*)(C->indices[0][1]);
  int* restrict C2_crd = (int*)(C->indices[1][1]);
  double* restrict C_vals = (double*)(C->vals);
  int A2_dimension = (int)(A->dimensions[1]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  int* restrict B1_pos = (int*)(B->indices[0][0]);
  int* restrict B1_crd = (int*)(B->indices[0][1]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);

  C1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  C1_pos[0] = 0;
  int32_t C1_crd_size = 1048576;
  C1_crd = (int32_t*)malloc(sizeof(int32_t) * C1_crd_size);
  int32_t C2_crd_size = 1048576;
  C2_crd = (int32_t*)malloc(sizeof(int32_t) * C2_crd_size);
  int32_t jC = 0;

  int32_t kB = B1_pos[0];
  int32_t pB1_end = B1_pos[1];

  while (kB < pB1_end) {
    int32_t k = B1_crd[kB];
    int32_t B1_segend = kB + 1;
    while (B1_segend < pB1_end && B1_crd[B1_segend] == k) {
      B1_segend++;
    }

    for (int32_t iA = A2_pos[k]; iA < A2_pos[(k + 1)]; iA++) {
      int32_t i = A2_crd[iA];
      for (int32_t jB = kB; jB < B1_segend; jB++) {
        int32_t j = B2_crd[jB];
        if (C2_crd_size <= jC) {
          int32_t C2_crd_new_size = TACO_MAX(C2_crd_size * 2,(jC + 1));
          C2_crd = (int32_t*)realloc(C2_crd, sizeof(int32_t) * C2_crd_new_size);
          C2_crd_size = C2_crd_new_size;
        }
        C2_crd[jC] = j;
        if (C1_crd_size <= jC) {
          C1_crd = (int32_t*)realloc(C1_crd, sizeof(int32_t) * (C1_crd_size * 2));
          C1_crd_size *= 2;
        }
        C1_crd[jC] = i;
        jC++;
      }
    }

    C1_pos[1] = jC;
    kB = B1_segend;
  }

  C_vals = (double*)malloc(sizeof(double) * jC);

  C->indices[0][0] = (uint8_t*)(C1_pos);
  C->indices[0][1] = (uint8_t*)(C1_crd);
  C->indices[1][1] = (uint8_t*)(C2_crd);
  C->vals = (uint8_t*)C_vals;
  return 0;
}

int CSC_x_COO_pack_A(taco_tensor_t *A, int* A_COO1_pos, int* A_COO1_crd, int* A_COO2_crd, double* A_COO_vals) {
  int A2_dimension = (int)(A->dimensions[1]);
  int* restrict A2_pos = (int*)(A->indices[1][0]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);

  A2_pos = (int32_t*)malloc(sizeof(int32_t) * (A2_dimension + 1));
  A2_pos[0] = 0;
  for (int32_t pA2 = 1; pA2 < (A2_dimension + 1); pA2++) {
    A2_pos[pA2] = 0;
  }
  int32_t A2_crd_size = 1048576;
  A2_crd = (int32_t*)malloc(sizeof(int32_t) * A2_crd_size);
  int32_t iA = 0;
  int32_t A_capacity = 1048576;
  A_vals = (double*)malloc(sizeof(double) * A_capacity);

  int32_t kA_COO = A_COO1_pos[0];
  int32_t pA_COO1_end = A_COO1_pos[1];

  while (kA_COO < pA_COO1_end) {
    int32_t k = A_COO1_crd[kA_COO];
    int32_t A_COO1_segend = kA_COO + 1;
    while (A_COO1_segend < pA_COO1_end && A_COO1_crd[A_COO1_segend] == k) {
      A_COO1_segend++;
    }
    int32_t pA2_begin = iA;

    int32_t iA_COO = kA_COO;

    while (iA_COO < A_COO1_segend) {
      int32_t i = A_COO2_crd[iA_COO];
      double A_COO_val = A_COO_vals[iA_COO];
      iA_COO++;
      while (iA_COO < A_COO1_segend && A_COO2_crd[iA_COO] == i) {
        A_COO_val += A_COO_vals[iA_COO];
        iA_COO++;
      }
      if (A_capacity <= iA) {
        A_vals = (double*)realloc(A_vals, sizeof(double) * (A_capacity * 2));
        A_capacity *= 2;
      }
      A_vals[iA] = A_COO_val;
      if (A2_crd_size <= iA) {
        A2_crd = (int32_t*)realloc(A2_crd, sizeof(int32_t) * (A2_crd_size * 2));
        A2_crd_size *= 2;
      }
      A2_crd[iA] = i;
      iA++;
    }

    A2_pos[k + 1] = iA - pA2_begin;
    kA_COO = A_COO1_segend;
  }

  int32_t csA2 = 0;
  for (int32_t pA20 = 1; pA20 < (A2_dimension + 1); pA20++) {
    csA2 += A2_pos[pA20];
    A2_pos[pA20] = csA2;
  }

  A->indices[1][0] = (uint8_t*)(A2_pos);
  A->indices[1][1] = (uint8_t*)(A2_crd);
  A->vals = (uint8_t*)A_vals;
  return 0;
}

int CSC_x_COO_pack_B(taco_tensor_t *B, int* B_COO1_pos, int* B_COO1_crd, int* B_COO2_crd, double* B_COO_vals) {
  int* restrict B1_pos = (int*)(B->indices[0][0]);
  int* restrict B1_crd = (int*)(B->indices[0][1]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  B1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  B1_pos[0] = 0;
  int32_t B1_crd_size = 1048576;
  B1_crd = (int32_t*)malloc(sizeof(int32_t) * B1_crd_size);
  int32_t B2_crd_size = 1048576;
  B2_crd = (int32_t*)malloc(sizeof(int32_t) * B2_crd_size);
  int32_t jB = 0;
  int32_t B_capacity = 1048576;
  B_vals = (double*)malloc(sizeof(double) * B_capacity);


  int32_t kB_COO = B_COO1_pos[0];
  int32_t pB_COO1_end = B_COO1_pos[1];

  while (kB_COO < pB_COO1_end) {
    int32_t k = B_COO1_crd[kB_COO];
    int32_t B_COO1_segend = kB_COO + 1;
    while (B_COO1_segend < pB_COO1_end && B_COO1_crd[B_COO1_segend] == k) {
      B_COO1_segend++;
    }
    int32_t jB_COO = kB_COO;

    while (jB_COO < B_COO1_segend) {
      int32_t j = B_COO2_crd[jB_COO];
      double B_COO_val = B_COO_vals[jB_COO];
      jB_COO++;
      while (jB_COO < B_COO1_segend && B_COO2_crd[jB_COO] == j) {
        B_COO_val += B_COO_vals[jB_COO];
        jB_COO++;
      }
      if (B_capacity <= jB) {
        B_vals = (double*)realloc(B_vals, sizeof(double) * (B_capacity * 2));
        B_capacity *= 2;
      }
      B_vals[jB] = B_COO_val;
      if (B2_crd_size <= jB) {
        int32_t B2_crd_new_size = TACO_MAX(B2_crd_size * 2,(jB + 1));
        B2_crd = (int32_t*)realloc(B2_crd, sizeof(int32_t) * B2_crd_new_size);
        B2_crd_size = B2_crd_new_size;
      }
      B2_crd[jB] = j;
      if (B1_crd_size <= jB) {
        B1_crd = (int32_t*)realloc(B1_crd, sizeof(int32_t) * (B1_crd_size * 2));
        B1_crd_size *= 2;
      }
      B1_crd[jB] = k;
      jB++;
    }
    kB_COO = B_COO1_segend;
  }

  B1_pos[1] = jB;

  B->indices[0][0] = (uint8_t*)(B1_pos);
  B->indices[0][1] = (uint8_t*)(B1_crd);
  B->indices[1][1] = (uint8_t*)(B2_crd);
  B->vals = (uint8_t*)B_vals;
  return 0;
}

int COO_x_CSR_compute(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  double* restrict C_vals = (double*)(C->vals);
  int* restrict A1_pos = (int*)(A->indices[0][0]);
  int* restrict A1_crd = (int*)(A->indices[0][1]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);
  int B1_dimension = (int)(B->dimensions[0]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  int32_t jC = 0;

  int32_t iA = A1_pos[0];
  int32_t pA1_end = A1_pos[1];

  while (iA < pA1_end) {
    int32_t i = A1_crd[iA];
    int32_t A1_segend = iA + 1;
    while (A1_segend < pA1_end && A1_crd[A1_segend] == i) {
      A1_segend++;
    }
    for (int32_t kA = iA; kA < A1_segend; kA++) {
      int32_t k = A2_crd[kA];
      for (int32_t jB = B2_pos[k]; jB < B2_pos[(k + 1)]; jB++) {
        C_vals[jC] = 0.0;
        C_vals[jC] = C_vals[jC] + A_vals[kA] * B_vals[jB];
        jC++;
      }
    }
    iA = A1_segend;
  }
  return 0;
}

int COO_x_CSR_assemble(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  int* restrict C1_pos = (int*)(C->indices[0][0]);
  int* restrict C1_crd = (int*)(C->indices[0][1]);
  int* restrict C2_crd = (int*)(C->indices[1][1]);
  double* restrict C_vals = (double*)(C->vals);
  int* restrict A1_pos = (int*)(A->indices[0][0]);
  int* restrict A1_crd = (int*)(A->indices[0][1]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  int B1_dimension = (int)(B->dimensions[0]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);

  C1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  C1_pos[0] = 0;
  int32_t C1_crd_size = 1048576;
  C1_crd = (int32_t*)malloc(sizeof(int32_t) * C1_crd_size);
  int32_t C2_crd_size = 1048576;
  C2_crd = (int32_t*)malloc(sizeof(int32_t) * C2_crd_size);
  int32_t jC = 0;


  int32_t iA = A1_pos[0];
  int32_t pA1_end = A1_pos[1];

  while (iA < pA1_end) {
    int32_t i = A1_crd[iA];
    int32_t A1_segend = iA + 1;
    while (A1_segend < pA1_end && A1_crd[A1_segend] == i) {
      A1_segend++;
    }
    for (int32_t kA = iA; kA < A1_segend; kA++) {
      int32_t k = A2_crd[kA];
      for (int32_t jB = B2_pos[k]; jB < B2_pos[(k + 1)]; jB++) {
        int32_t j = B2_crd[jB];
        if (C2_crd_size <= jC) {
          int32_t C2_crd_new_size = TACO_MAX(C2_crd_size * 2,(jC + 1));
          C2_crd = (int32_t*)realloc(C2_crd, sizeof(int32_t) * C2_crd_new_size);
          C2_crd_size = C2_crd_new_size;
        }
        C2_crd[jC] = j;
        if (C1_crd_size <= jC) {
          C1_crd = (int32_t*)realloc(C1_crd, sizeof(int32_t) * (C1_crd_size * 2));
          C1_crd_size *= 2;
        }
        C1_crd[jC] = i;
        jC++;
      }
    }
    iA = A1_segend;
  }

  C1_pos[1] = jC;

  C_vals = (double*)malloc(sizeof(double) * jC);

  C->indices[0][0] = (uint8_t*)(C1_pos);
  C->indices[0][1] = (uint8_t*)(C1_crd);
  C->indices[1][1] = (uint8_t*)(C2_crd);
  C->vals = (uint8_t*)C_vals;
  return 0;
}

int COO_x_CSR_pack_A(taco_tensor_t *A, int* A_COO1_pos, int* A_COO1_crd, int* A_COO2_crd, double* A_COO_vals) {
  int* restrict A1_pos = (int*)(A->indices[0][0]);
  int* restrict A1_crd = (int*)(A->indices[0][1]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);

  A1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  A1_pos[0] = 0;
  int32_t A1_crd_size = 1048576;
  A1_crd = (int32_t*)malloc(sizeof(int32_t) * A1_crd_size);
  int32_t A2_crd_size = 1048576;
  A2_crd = (int32_t*)malloc(sizeof(int32_t) * A2_crd_size);
  int32_t kA = 0;
  int32_t A_capacity = 1048576;
  A_vals = (double*)malloc(sizeof(double) * A_capacity);


  int32_t iA_COO = A_COO1_pos[0];
  int32_t pA_COO1_end = A_COO1_pos[1];

  while (iA_COO < pA_COO1_end) {
    int32_t i = A_COO1_crd[iA_COO];
    int32_t A_COO1_segend = iA_COO + 1;
    while (A_COO1_segend < pA_COO1_end && A_COO1_crd[A_COO1_segend] == i) {
      A_COO1_segend++;
    }
    int32_t kA_COO = iA_COO;

    while (kA_COO < A_COO1_segend) {
      int32_t k = A_COO2_crd[kA_COO];
      double A_COO_val = A_COO_vals[kA_COO];
      kA_COO++;
      while (kA_COO < A_COO1_segend && A_COO2_crd[kA_COO] == k) {
        A_COO_val += A_COO_vals[kA_COO];
        kA_COO++;
      }
      if (A_capacity <= kA) {
        A_vals = (double*)realloc(A_vals, sizeof(double) * (A_capacity * 2));
        A_capacity *= 2;
      }
      A_vals[kA] = A_COO_val;
      if (A2_crd_size <= kA) {
        int32_t A2_crd_new_size = TACO_MAX(A2_crd_size * 2,(kA + 1));
        A2_crd = (int32_t*)realloc(A2_crd, sizeof(int32_t) * A2_crd_new_size);
        A2_crd_size = A2_crd_new_size;
      }
      A2_crd[kA] = k;
      if (A1_crd_size <= kA) {
        A1_crd = (int32_t*)realloc(A1_crd, sizeof(int32_t) * (A1_crd_size * 2));
        A1_crd_size *= 2;
      }
      A1_crd[kA] = i;
      kA++;
    }
    iA_COO = A_COO1_segend;
  }

  A1_pos[1] = kA;

  A->indices[0][0] = (uint8_t*)(A1_pos);
  A->indices[0][1] = (uint8_t*)(A1_crd);
  A->indices[1][1] = (uint8_t*)(A2_crd);
  A->vals = (uint8_t*)A_vals;
  return 0;
}

int COO_x_CSR_pack_B(taco_tensor_t *B, int* B_COO1_pos, int* B_COO1_crd, int* B_COO2_crd, double* B_COO_vals) {
  int B1_dimension = (int)(B->dimensions[0]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  B2_pos = (int32_t*)malloc(sizeof(int32_t) * (B1_dimension + 1));
  B2_pos[0] = 0;
  for (int32_t pB2 = 1; pB2 < (B1_dimension + 1); pB2++) {
    B2_pos[pB2] = 0;
  }
  int32_t B2_crd_size = 1048576;
  B2_crd = (int32_t*)malloc(sizeof(int32_t) * B2_crd_size);
  int32_t jB = 0;
  int32_t B_capacity = 1048576;
  B_vals = (double*)malloc(sizeof(double) * B_capacity);

  int32_t kB_COO = B_COO1_pos[0];
  int32_t pB_COO1_end = B_COO1_pos[1];

  while (kB_COO < pB_COO1_end) {
    int32_t k = B_COO1_crd[kB_COO];
    int32_t B_COO1_segend = kB_COO + 1;
    while (B_COO1_segend < pB_COO1_end && B_COO1_crd[B_COO1_segend] == k) {
      B_COO1_segend++;
    }
    int32_t pB2_begin = jB;

    int32_t jB_COO = kB_COO;

    while (jB_COO < B_COO1_segend) {
      int32_t j = B_COO2_crd[jB_COO];
      double B_COO_val = B_COO_vals[jB_COO];
      jB_COO++;
      while (jB_COO < B_COO1_segend && B_COO2_crd[jB_COO] == j) {
        B_COO_val += B_COO_vals[jB_COO];
        jB_COO++;
      }
      if (B_capacity <= jB) {
        B_vals = (double*)realloc(B_vals, sizeof(double) * (B_capacity * 2));
        B_capacity *= 2;
      }
      B_vals[jB] = B_COO_val;
      if (B2_crd_size <= jB) {
        B2_crd = (int32_t*)realloc(B2_crd, sizeof(int32_t) * (B2_crd_size * 2));
        B2_crd_size *= 2;
      }
      B2_crd[jB] = j;
      jB++;
    }

    B2_pos[k + 1] = jB - pB2_begin;
    kB_COO = B_COO1_segend;
  }

  int32_t csB2 = 0;
  for (int32_t pB20 = 1; pB20 < (B1_dimension + 1); pB20++) {
    csB2 += B2_pos[pB20];
    B2_pos[pB20] = csB2;
  }

  B->indices[1][0] = (uint8_t*)(B2_pos);
  B->indices[1][1] = (uint8_t*)(B2_crd);
  B->vals = (uint8_t*)B_vals;
  return 0;
}

int COO_x_CSC_compute(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  double* restrict C_vals = (double*)(C->vals);
  int* restrict A1_pos = (int*)(A->indices[0][0]);
  int* restrict A1_crd = (int*)(A->indices[0][1]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);
  int B2_dimension = (int)(B->dimensions[1]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  int32_t jC = 0;

  int32_t iA = A1_pos[0];
  int32_t pA1_end = A1_pos[1];

  while (iA < pA1_end) {
    int32_t i = A1_crd[iA];
    int32_t A1_segend = iA + 1;
    while (A1_segend < pA1_end && A1_crd[A1_segend] == i) {
      A1_segend++;
    }
    for (int32_t j = 0; j < B2_dimension; j++) {
      double tkC_val = 0.0;
      int32_t kA = iA;
      int32_t kB = B2_pos[j];
      int32_t pB2_end = B2_pos[(j + 1)];

      while (kA < A1_segend && kB < pB2_end) {
        int32_t kA0 = A2_crd[kA];
        int32_t kB0 = B2_crd[kB];
        int32_t k = TACO_MIN(kA0,kB0);
        if (kA0 == k && kB0 == k) {
          tkC_val += A_vals[kA] * B_vals[kB];
        }
        kA += (int32_t)(kA0 == k);
        kB += (int32_t)(kB0 == k);
      }
      C_vals[jC] = tkC_val;
      jC++;
    }
    iA = A1_segend;
  }
  return 0;
}

int COO_x_CSC_assemble(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  int* restrict C1_pos = (int*)(C->indices[0][0]);
  int* restrict C1_crd = (int*)(C->indices[0][1]);
  int* restrict C2_crd = (int*)(C->indices[1][1]);
  double* restrict C_vals = (double*)(C->vals);
  int* restrict A1_pos = (int*)(A->indices[0][0]);
  int* restrict A1_crd = (int*)(A->indices[0][1]);
  int B2_dimension = (int)(B->dimensions[1]);

  C1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  C1_pos[0] = 0;
  int32_t C1_crd_size = 1048576;
  C1_crd = (int32_t*)malloc(sizeof(int32_t) * C1_crd_size);
  int32_t C2_crd_size = 1048576;
  C2_crd = (int32_t*)malloc(sizeof(int32_t) * C2_crd_size);
  int32_t jC = 0;


  int32_t iA = A1_pos[0];
  int32_t pA1_end = A1_pos[1];

  while (iA < pA1_end) {
    int32_t i = A1_crd[iA];
    int32_t A1_segend = iA + 1;
    while (A1_segend < pA1_end && A1_crd[A1_segend] == i) {
      A1_segend++;
    }
    for (int32_t j = 0; j < B2_dimension; j++) {
      if (C2_crd_size <= jC) {
        int32_t C2_crd_new_size = TACO_MAX(C2_crd_size * 2,(jC + 1));
        C2_crd = (int32_t*)realloc(C2_crd, sizeof(int32_t) * C2_crd_new_size);
        C2_crd_size = C2_crd_new_size;
      }
      C2_crd[jC] = j;
      if (C1_crd_size <= jC) {
        C1_crd = (int32_t*)realloc(C1_crd, sizeof(int32_t) * (C1_crd_size * 2));
        C1_crd_size *= 2;
      }
      C1_crd[jC] = i;
      jC++;
    }
    iA = A1_segend;
  }

  C1_pos[1] = jC;

  C_vals = (double*)malloc(sizeof(double) * jC);

  C->indices[0][0] = (uint8_t*)(C1_pos);
  C->indices[0][1] = (uint8_t*)(C1_crd);
  C->indices[1][1] = (uint8_t*)(C2_crd);
  C->vals = (uint8_t*)C_vals;
  return 0;
}

int COO_x_CSC_pack_A(taco_tensor_t *A, int* A_COO1_pos, int* A_COO1_crd, int* A_COO2_crd, double* A_COO_vals) {
  int* restrict A1_pos = (int*)(A->indices[0][0]);
  int* restrict A1_crd = (int*)(A->indices[0][1]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);

  A1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  A1_pos[0] = 0;
  int32_t A1_crd_size = 1048576;
  A1_crd = (int32_t*)malloc(sizeof(int32_t) * A1_crd_size);
  int32_t A2_crd_size = 1048576;
  A2_crd = (int32_t*)malloc(sizeof(int32_t) * A2_crd_size);
  int32_t kA = 0;
  int32_t A_capacity = 1048576;
  A_vals = (double*)malloc(sizeof(double) * A_capacity);


  int32_t iA_COO = A_COO1_pos[0];
  int32_t pA_COO1_end = A_COO1_pos[1];

  while (iA_COO < pA_COO1_end) {
    int32_t i = A_COO1_crd[iA_COO];
    int32_t A_COO1_segend = iA_COO + 1;
    while (A_COO1_segend < pA_COO1_end && A_COO1_crd[A_COO1_segend] == i) {
      A_COO1_segend++;
    }
    int32_t kA_COO = iA_COO;

    while (kA_COO < A_COO1_segend) {
      int32_t k = A_COO2_crd[kA_COO];
      double A_COO_val = A_COO_vals[kA_COO];
      kA_COO++;
      while (kA_COO < A_COO1_segend && A_COO2_crd[kA_COO] == k) {
        A_COO_val += A_COO_vals[kA_COO];
        kA_COO++;
      }
      if (A_capacity <= kA) {
        A_vals = (double*)realloc(A_vals, sizeof(double) * (A_capacity * 2));
        A_capacity *= 2;
      }
      A_vals[kA] = A_COO_val;
      if (A2_crd_size <= kA) {
        int32_t A2_crd_new_size = TACO_MAX(A2_crd_size * 2,(kA + 1));
        A2_crd = (int32_t*)realloc(A2_crd, sizeof(int32_t) * A2_crd_new_size);
        A2_crd_size = A2_crd_new_size;
      }
      A2_crd[kA] = k;
      if (A1_crd_size <= kA) {
        A1_crd = (int32_t*)realloc(A1_crd, sizeof(int32_t) * (A1_crd_size * 2));
        A1_crd_size *= 2;
      }
      A1_crd[kA] = i;
      kA++;
    }
    iA_COO = A_COO1_segend;
  }

  A1_pos[1] = kA;

  A->indices[0][0] = (uint8_t*)(A1_pos);
  A->indices[0][1] = (uint8_t*)(A1_crd);
  A->indices[1][1] = (uint8_t*)(A2_crd);
  A->vals = (uint8_t*)A_vals;
  return 0;
}

int COO_x_CSC_pack_B(taco_tensor_t *B, int* B_COO1_pos, int* B_COO1_crd, int* B_COO2_crd, double* B_COO_vals) {
  int B2_dimension = (int)(B->dimensions[1]);
  int* restrict B2_pos = (int*)(B->indices[1][0]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  B2_pos = (int32_t*)malloc(sizeof(int32_t) * (B2_dimension + 1));
  B2_pos[0] = 0;
  for (int32_t pB2 = 1; pB2 < (B2_dimension + 1); pB2++) {
    B2_pos[pB2] = 0;
  }
  int32_t B2_crd_size = 1048576;
  B2_crd = (int32_t*)malloc(sizeof(int32_t) * B2_crd_size);
  int32_t kB = 0;
  int32_t B_capacity = 1048576;
  B_vals = (double*)malloc(sizeof(double) * B_capacity);

  int32_t jB_COO = B_COO1_pos[0];
  int32_t pB_COO1_end = B_COO1_pos[1];

  while (jB_COO < pB_COO1_end) {
    int32_t j = B_COO1_crd[jB_COO];
    int32_t B_COO1_segend = jB_COO + 1;
    while (B_COO1_segend < pB_COO1_end && B_COO1_crd[B_COO1_segend] == j) {
      B_COO1_segend++;
    }
    int32_t pB2_begin = kB;

    int32_t kB_COO = jB_COO;

    while (kB_COO < B_COO1_segend) {
      int32_t k = B_COO2_crd[kB_COO];
      double B_COO_val = B_COO_vals[kB_COO];
      kB_COO++;
      while (kB_COO < B_COO1_segend && B_COO2_crd[kB_COO] == k) {
        B_COO_val += B_COO_vals[kB_COO];
        kB_COO++;
      }
      if (B_capacity <= kB) {
        B_vals = (double*)realloc(B_vals, sizeof(double) * (B_capacity * 2));
        B_capacity *= 2;
      }
      B_vals[kB] = B_COO_val;
      if (B2_crd_size <= kB) {
        B2_crd = (int32_t*)realloc(B2_crd, sizeof(int32_t) * (B2_crd_size * 2));
        B2_crd_size *= 2;
      }
      B2_crd[kB] = k;
      kB++;
    }

    B2_pos[j + 1] = kB - pB2_begin;
    jB_COO = B_COO1_segend;
  }

  int32_t csB2 = 0;
  for (int32_t pB20 = 1; pB20 < (B2_dimension + 1); pB20++) {
    csB2 += B2_pos[pB20];
    B2_pos[pB20] = csB2;
  }

  B->indices[1][0] = (uint8_t*)(B2_pos);
  B->indices[1][1] = (uint8_t*)(B2_crd);
  B->vals = (uint8_t*)B_vals;
  return 0;
}

int COO_x_COO_compute(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  double* restrict C_vals = (double*)(C->vals);
  int* restrict A1_pos = (int*)(A->indices[0][0]);
  int* restrict A1_crd = (int*)(A->indices[0][1]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);
  int* restrict B1_pos = (int*)(B->indices[0][0]);
  int* restrict B1_crd = (int*)(B->indices[0][1]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  int32_t jC = 0;

  int32_t iA = A1_pos[0];
  int32_t pA1_end = A1_pos[1];

  while (iA < pA1_end) {
    int32_t i = A1_crd[iA];
    int32_t A1_segend = iA + 1;
    while (A1_segend < pA1_end && A1_crd[A1_segend] == i) {
      A1_segend++;
    }
    int32_t kA = iA;
    int32_t kB = B1_pos[0];
    int32_t pB1_end = B1_pos[1];

    while (kA < A1_segend && kB < pB1_end) {
      int32_t kA0 = A2_crd[kA];
      int32_t kB0 = B1_crd[kB];
      int32_t k = TACO_MIN(kA0,kB0);
      int32_t B1_segend = kB;
      while (B1_segend < pB1_end && B1_crd[B1_segend] == k) {
        B1_segend++;
      }
      if (kA0 == k && kB0 == k) {
        for (int32_t jB = kB; jB < B1_segend; jB++) {
          C_vals[jC] = 0.0;
          C_vals[jC] = C_vals[jC] + A_vals[kA] * B_vals[jB];
          jC++;
        }
      }
      kA += (int32_t)(kA0 == k);
      kB = B1_segend;
    }
    iA = A1_segend;
  }
  return 0;
}

int COO_x_COO_assemble(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B) {
  int* restrict C1_pos = (int*)(C->indices[0][0]);
  int* restrict C1_crd = (int*)(C->indices[0][1]);
  int* restrict C2_crd = (int*)(C->indices[1][1]);
  double* restrict C_vals = (double*)(C->vals);
  int* restrict A1_pos = (int*)(A->indices[0][0]);
  int* restrict A1_crd = (int*)(A->indices[0][1]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  int* restrict B1_pos = (int*)(B->indices[0][0]);
  int* restrict B1_crd = (int*)(B->indices[0][1]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);

  C1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  C1_pos[0] = 0;
  int32_t C1_crd_size = 1048576;
  C1_crd = (int32_t*)malloc(sizeof(int32_t) * C1_crd_size);
  int32_t C2_crd_size = 1048576;
  C2_crd = (int32_t*)malloc(sizeof(int32_t) * C2_crd_size);
  int32_t jC = 0;


  int32_t iA = A1_pos[0];
  int32_t pA1_end = A1_pos[1];

  while (iA < pA1_end) {
    int32_t i = A1_crd[iA];
    int32_t A1_segend = iA + 1;
    while (A1_segend < pA1_end && A1_crd[A1_segend] == i) {
      A1_segend++;
    }
    int32_t kA = iA;
    int32_t kB = B1_pos[0];
    int32_t pB1_end = B1_pos[1];

    while (kA < A1_segend && kB < pB1_end) {
      int32_t kA0 = A2_crd[kA];
      int32_t kB0 = B1_crd[kB];
      int32_t k = TACO_MIN(kA0,kB0);
      int32_t B1_segend = kB;
      while (B1_segend < pB1_end && B1_crd[B1_segend] == k) {
        B1_segend++;
      }
      if (kA0 == k && kB0 == k) {
        for (int32_t jB = kB; jB < B1_segend; jB++) {
          int32_t j = B2_crd[jB];
          if (C2_crd_size <= jC) {
            int32_t C2_crd_new_size = TACO_MAX(C2_crd_size * 2,(jC + 1));
            C2_crd = (int32_t*)realloc(C2_crd, sizeof(int32_t) * C2_crd_new_size);
            C2_crd_size = C2_crd_new_size;
          }
          C2_crd[jC] = j;
          if (C1_crd_size <= jC) {
            C1_crd = (int32_t*)realloc(C1_crd, sizeof(int32_t) * (C1_crd_size * 2));
            C1_crd_size *= 2;
          }
          C1_crd[jC] = i;
          jC++;
        }
      }
      kA += (int32_t)(kA0 == k);
      kB = B1_segend;
    }
    iA = A1_segend;
  }

  C1_pos[1] = jC;

  C_vals = (double*)malloc(sizeof(double) * jC);

  C->indices[0][0] = (uint8_t*)(C1_pos);
  C->indices[0][1] = (uint8_t*)(C1_crd);
  C->indices[1][1] = (uint8_t*)(C2_crd);
  C->vals = (uint8_t*)C_vals;
  return 0;
}

int COO_x_COO_pack_A(taco_tensor_t *A, int* A_COO1_pos, int* A_COO1_crd, int* A_COO2_crd, double* A_COO_vals) {
  int* restrict A1_pos = (int*)(A->indices[0][0]);
  int* restrict A1_crd = (int*)(A->indices[0][1]);
  int* restrict A2_crd = (int*)(A->indices[1][1]);
  double* restrict A_vals = (double*)(A->vals);

  A1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  A1_pos[0] = 0;
  int32_t A1_crd_size = 1048576;
  A1_crd = (int32_t*)malloc(sizeof(int32_t) * A1_crd_size);
  int32_t A2_crd_size = 1048576;
  A2_crd = (int32_t*)malloc(sizeof(int32_t) * A2_crd_size);
  int32_t kA = 0;
  int32_t A_capacity = 1048576;
  A_vals = (double*)malloc(sizeof(double) * A_capacity);


  int32_t iA_COO = A_COO1_pos[0];
  int32_t pA_COO1_end = A_COO1_pos[1];

  while (iA_COO < pA_COO1_end) {
    int32_t i = A_COO1_crd[iA_COO];
    int32_t A_COO1_segend = iA_COO + 1;
    while (A_COO1_segend < pA_COO1_end && A_COO1_crd[A_COO1_segend] == i) {
      A_COO1_segend++;
    }
    int32_t kA_COO = iA_COO;

    while (kA_COO < A_COO1_segend) {
      int32_t k = A_COO2_crd[kA_COO];
      double A_COO_val = A_COO_vals[kA_COO];
      kA_COO++;
      while (kA_COO < A_COO1_segend && A_COO2_crd[kA_COO] == k) {
        A_COO_val += A_COO_vals[kA_COO];
        kA_COO++;
      }
      if (A_capacity <= kA) {
        A_vals = (double*)realloc(A_vals, sizeof(double) * (A_capacity * 2));
        A_capacity *= 2;
      }
      A_vals[kA] = A_COO_val;
      if (A2_crd_size <= kA) {
        int32_t A2_crd_new_size = TACO_MAX(A2_crd_size * 2,(kA + 1));
        A2_crd = (int32_t*)realloc(A2_crd, sizeof(int32_t) * A2_crd_new_size);
        A2_crd_size = A2_crd_new_size;
      }
      A2_crd[kA] = k;
      if (A1_crd_size <= kA) {
        A1_crd = (int32_t*)realloc(A1_crd, sizeof(int32_t) * (A1_crd_size * 2));
        A1_crd_size *= 2;
      }
      A1_crd[kA] = i;
      kA++;
    }
    iA_COO = A_COO1_segend;
  }

  A1_pos[1] = kA;

  A->indices[0][0] = (uint8_t*)(A1_pos);
  A->indices[0][1] = (uint8_t*)(A1_crd);
  A->indices[1][1] = (uint8_t*)(A2_crd);
  A->vals = (uint8_t*)A_vals;
  return 0;
}

int COO_x_COO_pack_B(taco_tensor_t *B, int* B_COO1_pos, int* B_COO1_crd, int* B_COO2_crd, double* B_COO_vals) {
  int* restrict B1_pos = (int*)(B->indices[0][0]);
  int* restrict B1_crd = (int*)(B->indices[0][1]);
  int* restrict B2_crd = (int*)(B->indices[1][1]);
  double* restrict B_vals = (double*)(B->vals);

  B1_pos = (int32_t*)malloc(sizeof(int32_t) * 2);
  B1_pos[0] = 0;
  int32_t B1_crd_size = 1048576;
  B1_crd = (int32_t*)malloc(sizeof(int32_t) * B1_crd_size);
  int32_t B2_crd_size = 1048576;
  B2_crd = (int32_t*)malloc(sizeof(int32_t) * B2_crd_size);
  int32_t jB = 0;
  int32_t B_capacity = 1048576;
  B_vals = (double*)malloc(sizeof(double) * B_capacity);


  int32_t kB_COO = B_COO1_pos[0];
  int32_t pB_COO1_end = B_COO1_pos[1];

  while (kB_COO < pB_COO1_end) {
    int32_t k = B_COO1_crd[kB_COO];
    int32_t B_COO1_segend = kB_COO + 1;
    while (B_COO1_segend < pB_COO1_end && B_COO1_crd[B_COO1_segend] == k) {
      B_COO1_segend++;
    }
    int32_t jB_COO = kB_COO;

    while (jB_COO < B_COO1_segend) {
      int32_t j = B_COO2_crd[jB_COO];
      double B_COO_val = B_COO_vals[jB_COO];
      jB_COO++;
      while (jB_COO < B_COO1_segend && B_COO2_crd[jB_COO] == j) {
        B_COO_val += B_COO_vals[jB_COO];
        jB_COO++;
      }
      if (B_capacity <= jB) {
        B_vals = (double*)realloc(B_vals, sizeof(double) * (B_capacity * 2));
        B_capacity *= 2;
      }
      B_vals[jB] = B_COO_val;
      if (B2_crd_size <= jB) {
        int32_t B2_crd_new_size = TACO_MAX(B2_crd_size * 2,(jB + 1));
        B2_crd = (int32_t*)realloc(B2_crd, sizeof(int32_t) * B2_crd_new_size);
        B2_crd_size = B2_crd_new_size;
      }
      B2_crd[jB] = j;
      if (B1_crd_size <= jB) {
        B1_crd = (int32_t*)realloc(B1_crd, sizeof(int32_t) * (B1_crd_size * 2));
        B1_crd_size *= 2;
      }
      B1_crd[jB] = k;
      jB++;
    }
    kB_COO = B_COO1_segend;
  }

  B1_pos[1] = jB;

  B->indices[0][0] = (uint8_t*)(B1_pos);
  B->indices[0][1] = (uint8_t*)(B1_crd);
  B->indices[1][1] = (uint8_t*)(B2_crd);
  B->vals = (uint8_t*)B_vals;
  return 0;
}
