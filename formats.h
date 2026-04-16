#ifndef FORMATS_H
#define FORMATS_H

#include "../common.h"

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

enum Matrix_Format
{
  MAT_NONE,

  MAT_DENSE,
  MAT_CSR,
  MAT_CSC,

  MAT_COUNT,
};
typedef u8 Matrix_Format;

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

// FIXME: TODO: Implementations are just copy pasted from the reptest, figure out how to reuse instead while still keeping macro capabilities....
static
void dense_x_dense(Dense_Matrix output, Dense_Matrix left, Dense_Matrix right);

static
void dense_x_csr(Dense_Matrix output, Dense_Matrix left, CSR_Matrix right);

static
void dense_x_csc(Dense_Matrix output, Dense_Matrix left, CSC_Matrix right);

static
void csr_x_dense(Dense_Matrix output, CSR_Matrix left, Dense_Matrix right);

static
void csr_x_csr(Dense_Matrix output, CSR_Matrix left, CSR_Matrix right);

static
void csr_x_csc(Dense_Matrix output, CSR_Matrix left, CSC_Matrix right);

static
void csc_x_dense(Dense_Matrix output, CSC_Matrix left, Dense_Matrix right);

static
void csc_x_csr(Dense_Matrix output, CSC_Matrix left, CSR_Matrix right);

static
void csc_x_csc(Dense_Matrix output, CSC_Matrix left, CSC_Matrix right);

#endif // FORMATS_H
