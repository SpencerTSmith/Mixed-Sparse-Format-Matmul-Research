#ifndef FORMATS_H
#define FORMATS_H

#include "common.h"

// Row major
typedef struct Dense_Matrix Dense_Matrix;
struct Dense_Matrix
{
  u32 row_count;
  u32 col_count;
  f64 *values;
};

typedef struct CSR_Matrix CSR_Matrix;
struct CSR_Matrix
{
  u32 non_zero_count;
  u32 row_count;

  // Hmm, might be better to have just one buffer and relative pointers
  u32 *row_pointers;
  u32 *col_indices;
  f64 *values;
};

typedef struct CSC_Matrix CSC_Matrix;
struct CSC_Matrix
{
  u32 non_zero_count;
  u32 col_count;

  // Hmm, might be better to have just one buffer and relative pointers
  u32 *row_indices;
  u32 *col_pointers;
  f64 *values;
};

typedef enum Matrix_Format
{
  MAT_NONE,

  MAT_DENSE,
  MAT_CSR,
  MAT_CSC,

  MAT_COUNT,
} Matrix_Format;

typedef enum Matrix_Format_Combo
{
  MAT_COMBO_NONE,

  MAT_COMBO_DENSE_DENSE,
  MAT_COMBO_DENSE_CSR,
  MAT_COMBO_DENSE_CSC,
  MAT_COMBO_CSR_DENSE,
  MAT_COMBO_CSR_CSR,
  MAT_COMBO_CSR_CSC,
  MAT_COMBO_CSC_DENSE,
  MAT_COMBO_CSC_CSR,
  MAT_COMBO_CSC_CSC,

  MAT_COMBO_COUNT,
} Matrix_Format_Combo;

typedef struct Matrix_Union Matrix_Union;
struct Matrix_Union
{
  Matrix_Format format;
  union
  {
    Dense_Matrix dense;
    CSR_Matrix   csr;
    CSC_Matrix   csc;
  };
};

// Not a union, stores all 3
typedef struct Matrix_Reps Matrix_Reps;
struct Matrix_Reps
{
  Dense_Matrix dense;
  CSR_Matrix   csr;
  CSC_Matrix   csc;
};

static
Dense_Matrix make_random_dense_matrix(Arena *arena, u32 row_count, u32 col_count, f64 sparsity);

static
CSR_Matrix csr_from_dense(Arena *arena, Dense_Matrix *dense);

static
CSC_Matrix csc_from_dense(Arena *arena, Dense_Matrix *dense);

#endif // FORMATS_H
