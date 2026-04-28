#include "formats.h"

static
u64 dense_non_zero_count(Dense_Matrix *dense)
{
  u64 result = 0;

  isize buffer_count = dense->row_count * dense->col_count;

  for (isize i = 0; i < buffer_count; i++)
  {
    if (dense->values[i])
    {
      result += 1;
    }
  }

  return result;
}

static
CSR_Matrix csr_from_dense(Arena *arena, Dense_Matrix *dense)
{
  CSR_Matrix result = {0};
  result.non_zero_count = dense_non_zero_count(dense);
  result.row_count = dense->row_count;

  result.values       = arena_calloc(arena, result.non_zero_count, f64);
  result.col_indices  = arena_calloc(arena, result.non_zero_count, u16);
  result.row_pointers = arena_calloc(arena, result.row_count + 1, u16);

  isize non_zero_index = 0;
  for (isize r = 0; r < dense->row_count; r++)
  {
    for (isize c = 0; c < dense->col_count; c++)
    {
      // NOTE: Row major
      f64 value = dense->values[r * dense->col_count + c];

      if (value)
      {
        result.values[non_zero_index]      = value;
        result.col_indices[non_zero_index] = c;
        non_zero_index += 1;
      }
    }

    result.row_pointers[r + 1] = non_zero_index;
  }

  return result;
}

static
CSC_Matrix csc_from_dense(Arena *arena, Dense_Matrix *dense)
{
  CSC_Matrix result = {0};
  result.non_zero_count = dense_non_zero_count(dense);
  result.col_count = dense->col_count;

  result.values       = arena_calloc(arena, result.non_zero_count, f64);
  result.row_indices  = arena_calloc(arena, result.non_zero_count, u16);
  result.col_pointers = arena_calloc(arena, result.col_count + 1, u16);

  isize non_zero_index = 0;
  for (isize c = 0; c < dense->col_count; c++)
  {
    for (isize r = 0; r < dense->row_count; r++)
    {
      f64 value = dense->values[r * dense->col_count + c];

      if (value)
      {
        result.values[non_zero_index]      = value;
        result.row_indices[non_zero_index] = r;
        non_zero_index += 1;
      }
    }

    result.col_pointers[c + 1] = non_zero_index;
  }

  return result;
}

static
COO_Matrix coo_from_dense(Arena *arena, Dense_Matrix *dense)
{
  COO_Matrix result = {0};
  result.non_zero_count = dense_non_zero_count(dense);
  result.values      = arena_calloc(arena, result.non_zero_count, f64);
  result.row_indices = arena_calloc(arena, result.non_zero_count, u16);
  result.col_indices = arena_calloc(arena, result.non_zero_count, u16);

  isize non_zero_index = 0;
  for (isize r = 0; r < dense->row_count; r++)
  {
    for (isize c = 0; c < dense->col_count; c++)
    {
      f64 value = dense->values[r * dense->col_count + c];

      if (value)
      {
        result.values[non_zero_index]      = value;
        result.row_indices[non_zero_index] = r;
        result.col_indices[non_zero_index] = c;
        non_zero_index += 1;
      }
    }
  }

  return result;
}

#include <stdlib.h>

static
Dense_Matrix make_random_dense_matrix(Arena *arena, u32 row_count, u32 col_count, f64 density)
{
  Dense_Matrix result =
  {
    .row_count = row_count,
    .col_count = col_count,
    .values = arena_calloc(arena, row_count * col_count, f64),
  };

  for (u32 r = 0; r < row_count; r++)
  {
    for (u32 c = 0; c < col_count; c++)
    {
      f64 check = (f64)rand() / RAND_MAX;

      // We should put a non-zero
      if (check < density)
      {
        f64 value = ((f64)rand() / RAND_MAX) * 2.0 - 1.0;
        result.values[r * col_count + c] = value;
      }
    }
  }

  return result;
}

#ifndef FMADD
#define FMADD(dst, a, b) dst += (a * b)
#endif

#ifndef LOAD
#define LOAD(src)       src
#endif

#ifndef STORE
#define STORE(dst, src) dst = src
#endif

static
void dense_x_dense(Dense_Matrix output, Dense_Matrix left, Dense_Matrix right)
{
  dense_x_dense_impl;
}

static
void dense_x_csr(Dense_Matrix output, Dense_Matrix left, CSR_Matrix right)
{
  dense_x_csr_impl;
}

static
void dense_x_csc(Dense_Matrix output, Dense_Matrix left, CSC_Matrix right)
{
  dense_x_csc_impl;
}

static
void dense_x_coo(Dense_Matrix output, Dense_Matrix left, COO_Matrix right)
{
  dense_x_coo_impl;
}

static
void csr_x_dense(Dense_Matrix output, CSR_Matrix left, Dense_Matrix right)
{
  csr_x_dense_impl;
}

static
void csr_x_csr(Dense_Matrix output, CSR_Matrix left, CSR_Matrix right)
{
  csr_x_csr_impl;
}

static
void csr_x_csc(Dense_Matrix output, CSR_Matrix left, CSC_Matrix right)
{
  csr_x_csc_impl;
}

static
void csr_x_coo(Dense_Matrix output, CSR_Matrix left, COO_Matrix right)
{
  csr_x_coo_impl;
}

static
void csc_x_dense(Dense_Matrix output, CSC_Matrix left, Dense_Matrix right)
{
  csc_x_dense_impl;
}

static
void csc_x_csr(Dense_Matrix output, CSC_Matrix left, CSR_Matrix right)
{
  csc_x_csr_impl;
}

static
void csc_x_csc(Dense_Matrix output, CSC_Matrix left, CSC_Matrix right)
{
  csc_x_csc_impl;
}

static
void csc_x_coo(Dense_Matrix output, CSC_Matrix left, COO_Matrix right)
{
  csc_x_coo_impl;
}

static
void coo_x_dense(Dense_Matrix output, COO_Matrix left, Dense_Matrix right)
{
  coo_x_dense_impl;
}

static
void coo_x_csr(Dense_Matrix output, COO_Matrix left, CSR_Matrix right)
{
  coo_x_csr_impl;
}

static
void coo_x_csc(Dense_Matrix output, COO_Matrix left, CSC_Matrix right)
{
  coo_x_csc_impl;
}

static
void coo_x_coo(Dense_Matrix output, COO_Matrix left, COO_Matrix right)
{
  coo_x_coo_impl;
}

static
Matrix_Union dense_to_format(Arena *arena, Dense_Matrix matrix, Matrix_Format format)
{
  Matrix_Union result =
  {
    .format = format,
  };

  switch (format)
  {
    case MAT_NONE: case MAT_COUNT: { ASSERT(false, "idiot."); } break;
    case MAT_DENSE: { result.dense = matrix; } break;
    case MAT_CSR: { result.csr = csr_from_dense(arena, &matrix); } break;
    case MAT_CSC: { result.csc = csc_from_dense(arena, &matrix); } break;
    case MAT_COO: { result.coo = coo_from_dense(arena, &matrix); } break;
  }

  return result;
}
