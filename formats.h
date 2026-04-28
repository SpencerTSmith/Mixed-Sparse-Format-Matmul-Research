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
  u16 *row_pointers;
  u16 *col_indices;
  f64 *values;
};

typedef struct CSC_Matrix CSC_Matrix;
struct CSC_Matrix
{
  u32 non_zero_count;
  u32 col_count;

  // Hmm, might be better to have just one buffer and relative pointers
  u16 *row_indices;
  u16 *col_pointers;
  f64 *values;
};

typedef struct COO_Matrix COO_Matrix;
struct COO_Matrix
{
  u32 non_zero_count;

  u16 *row_indices;
  u16 *col_indices;
  f64 *values;
};

enum Matrix_Format
{
  MAT_NONE,

  MAT_DENSE,
  MAT_CSR,
  MAT_CSC,
  MAT_COO,

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
    COO_Matrix   coo;
  };
};

// Not a union, stores all 3
typedef struct Matrix_Reps Matrix_Reps;
struct Matrix_Reps
{
  Dense_Matrix dense;
  CSR_Matrix   csr;
  CSC_Matrix   csc;
  COO_Matrix   coo;
};

static
Dense_Matrix make_random_dense_matrix(Arena *arena, u32 row_count, u32 col_count, f64 density);

static
u64 dense_non_zero_count(Dense_Matrix *dense);

static
CSR_Matrix csr_from_dense(Arena *arena, Dense_Matrix *dense);

static
CSC_Matrix csc_from_dense(Arena *arena, Dense_Matrix *dense);

static
COO_Matrix coo_from_dense(Arena *arena, Dense_Matrix *dense);

static
Dense_Matrix make_random_dense_matrix(Arena *arena, u32 row_count, u32 col_count, f64 density);

static
Matrix_Union dense_to_format(Arena *arena, Dense_Matrix matrix, Matrix_Format format);

#define dense_x_dense_impl STATEMENT(                           \
  for (usize row = 0; row < left.row_count; row++)              \
  {                                                             \
    for (usize col = 0; col < right.col_count; col++)           \
    {                                                           \
      usize output_index = row * output.col_count + col;        \
      f64 dot = LOAD(output.values[output_index]);              \
                                                                \
      for (usize i = 0; i < left.col_count; i++)                \
      {                                                         \
        usize left_index  = row * left.col_count + i;           \
        usize right_index = i * right.col_count + col;          \
        f64 left_value  = LOAD(left.values[left_index]);        \
        f64 right_value = LOAD(right.values[right_index]);      \
                                                                \
        FMADD(dot, left_value, right_value);                    \
      }                                                         \
                                                                \
      STORE(output.values[output_index], dot);                  \
    }                                                           \
  }                                                             \
)

#define dense_x_csr_impl STATEMENT(                                   \
  for (usize row = 0; row < left.row_count; row++)                    \
  {                                                                   \
    for (usize k = 0; k < right.row_count; k++)                       \
    {                                                                 \
      usize left_index = row * left.col_count + k;                    \
      f64 left_value   = LOAD(left.values[left_index]);               \
                                                                      \
      usize right_row_start = LOAD(right.row_pointers[k]);            \
      usize right_row_close = LOAD(right.row_pointers[k + 1]);        \
                                                                      \
      for (usize rj = right_row_start; rj < right_row_close; rj++)    \
      {                                                               \
        usize col = LOAD(right.col_indices[rj]);                      \
        f64 right_value = LOAD(right.values[rj]);                     \
                                                                      \
        usize output_index = row * output.col_count + col;            \
        f64 output_value   = LOAD(output.values[output_index]);       \
                                                                      \
        FMADD(output_value, left_value, right_value);                 \
                                                                      \
        STORE(output.values[output_index], output_value);             \
      }                                                               \
    }                                                                 \
  }                                                                   \
)

#define dense_x_csc_impl STATEMENT(                                  \
  for (usize row = 0; row < left.row_count; row++)                   \
  {                                                                  \
    for (usize col = 0; col < right.col_count; col++)                \
    {                                                                \
      usize output_index = row * output.col_count + col;             \
      f64 output_value = LOAD(output.values[output_index]);          \
                                                                     \
      usize right_col_start = LOAD(right.col_pointers[col]);         \
      usize right_col_close   = LOAD(right.col_pointers[col + 1]);   \
      for (usize kc = right_col_start; kc < right_col_close; kc++)   \
      {                                                              \
        usize k = LOAD(right.row_indices[kc]);                       \
        f64 right_value = LOAD(right.values[kc]);                    \
                                                                     \
        usize left_index = row * left.col_count + k;                 \
        f64 left_value = LOAD(left.values[left_index]);              \
                                                                     \
        FMADD(output_value, left_value, right_value);                \
      }                                                              \
                                                                     \
      STORE(output.values[output_index], output_value);              \
    }                                                                \
  }                                                                  \
)

#define dense_x_coo_impl STATEMENT(                                  \
  for (usize row = 0; row < left.row_count; row++)                   \
  {                                                                  \
    usize kB     = 0;                                                \
    usize kB_end = right.non_zero_count;                             \
                                                                     \
    while (kB < kB_end)                                              \
    {                                                                \
      usize k = LOAD(right.row_indices[kB]);                         \
                                                                     \
      usize B_segend = kB + 1;                                       \
      usize at_right = LOAD(right.row_indices[B_segend]);            \
      while (B_segend < kB_end && at_right == k)                     \
      {                                                              \
        B_segend++;                                                  \
        at_right = LOAD(right.row_indices[B_segend]);                \
      }                                                              \
                                                                     \
      usize left_index = row * left.col_count + k;                   \
      f64   left_value = LOAD(left.values[left_index]);              \
                                                                     \
      for (usize jB = kB; jB < B_segend; jB++)                       \
      {                                                              \
        usize col         = LOAD(right.col_indices[jB]);             \
        f64   right_value = LOAD(right.values[jB]);                  \
                                                                     \
        usize output_index = row * output.col_count + col;           \
        f64   output_value = LOAD(output.values[output_index]);      \
        FMADD(output_value, left_value, right_value);                \
        STORE(output.values[output_index], output_value);            \
      }                                                              \
                                                                     \
      kB = B_segend;                                                 \
    }                                                                \
  }                                                                  \
)

#define csr_x_dense_impl STATEMENT(                                          \
  for (usize row = 0; row < left.row_count; row++)                           \
  {                                                                          \
    usize row_start = LOAD(left.row_pointers[row]);                          \
    usize row_end   = LOAD(left.row_pointers[row + 1]);                      \
                                                                             \
    for (usize i = row_start; i < row_end; i++)                              \
    {                                                                        \
      usize left_col = LOAD(left.col_indices[i]);                            \
      f64 left_value = LOAD(left.values[i]);                                 \
                                                                             \
      for (usize right_col = 0; right_col < right.col_count; right_col++)    \
      {                                                                      \
        usize right_index  = left_col * right.col_count + right_col;         \
        usize output_index = row * output.col_count + right_col;             \
                                                                             \
        f64 right_value   = LOAD(right.values[right_index]);                 \
        f64 current_value = LOAD(output.values[output_index]);               \
                                                                             \
        f64 result_value = current_value;                                    \
        FMADD(result_value, left_value, right_value);                        \
                                                                             \
        STORE(output.values[output_index], result_value);                    \
      }                                                                      \
    }                                                                        \
  }                                                                          \
)

#define csr_x_csr_impl STATEMENT(                                            \
  for (usize left_row = 0; left_row < left.row_count; left_row++)            \
  {                                                                          \
    usize left_row_start = LOAD(left.row_pointers[left_row]);                \
    usize left_row_end   = LOAD(left.row_pointers[left_row + 1]);            \
                                                                             \
    for (usize i = left_row_start; i < left_row_end; i++)                    \
    {                                                                        \
      usize left_col = LOAD(left.col_indices[i]);                            \
      f64 left_value = LOAD(left.values[i]);                                 \
                                                                             \
      usize right_row_start = LOAD(right.row_pointers[left_col]);            \
      usize right_row_end   = LOAD(right.row_pointers[left_col + 1]);        \
      for (usize j = right_row_start; j < right_row_end; j++)                \
      {                                                                      \
        usize right_col = LOAD(right.col_indices[j]);                        \
        f64 right_value = LOAD(right.values[j]);                             \
                                                                             \
        usize output_index = left_row * output.col_count + right_col;        \
        f64 current_value = LOAD(output.values[output_index]);               \
                                                                             \
        f64 result_value = current_value;                                    \
        FMADD(result_value, left_value, right_value);                        \
                                                                             \
        STORE(output.values[output_index], result_value);                    \
      }                                                                      \
    }                                                                        \
  }                                                                          \
)

#define csr_x_csc_impl STATEMENT(                                            \
  for (usize left_row = 0; left_row < left.row_count; left_row++)            \
  {                                                                          \
    usize left_row_start = LOAD(left.row_pointers[left_row]);                \
    usize left_row_end   = LOAD(left.row_pointers[left_row + 1]);            \
                                                                             \
    for (usize right_col = 0; right_col < right.col_count; right_col++)      \
    {                                                                        \
      usize right_col_start = LOAD(right.col_pointers[right_col]);           \
      usize right_col_end   = LOAD(right.col_pointers[right_col + 1]);       \
                                                                             \
      usize output_index = left_row * output.col_count + right_col;          \
      f64 result_value = LOAD(output.values[output_index]);                  \
                                                                             \
      usize left_cursor  = left_row_start;                                   \
      usize right_cursor = right_col_start;                                  \
      while (left_cursor < left_row_end && right_cursor < right_col_end)     \
      {                                                                      \
        usize left_col  = LOAD(left.col_indices[left_cursor]);               \
        usize right_row = LOAD(right.row_indices[right_cursor]);             \
        usize k = MIN(left_col, right_row);                                  \
                                                                             \
        if (left_col == k && right_row == k)                                 \
        {                                                                    \
          f64 left_value  = LOAD(left.values[left_cursor]);                  \
          f64 right_value = LOAD(right.values[right_cursor]);                \
          FMADD(result_value, left_value, right_value);                      \
        }                                                                    \
        left_cursor  += (usize)(left_col == k);                              \
        right_cursor += (usize)(right_row == k);                             \
      }                                                                      \
                                                                             \
      STORE(output.values[output_index], result_value);                      \
    }                                                                        \
  }                                                                          \
)

#define csr_x_coo_impl STATEMENT(                                            \
  for (usize row = 0; row < left.row_count; row++)                           \
  {                                                                          \
    usize kA        = LOAD(left.row_pointers[row]);                          \
    usize kA_end    = LOAD(left.row_pointers[row + 1]);                      \
    usize kB        = 0;                                                     \
    usize kB_end    = right.non_zero_count;                                  \
                                                                             \
    while (kA < kA_end && kB < kB_end)                                       \
    {                                                                        \
      usize kA0 = LOAD(left.col_indices[kA]);                                \
      usize kB0 = LOAD(right.row_indices[kB]);                               \
      usize k   = MIN(kA0, kB0);                                             \
                                                                             \
      usize B_segend = kB;                                                   \
      usize at_right = LOAD(right.row_indices[B_segend]);                                               \
      while (B_segend < kB_end && at_right == k)                             \
      {                                                                      \
        B_segend++;                                                          \
        at_right = LOAD(right.row_indices[B_segend]);                        \
      }                                                                      \
                                                                             \
      if (kA0 == k && kB0 == k)                                              \
      {                                                                      \
        f64 left_value = LOAD(left.values[kA]);                              \
                                                                             \
        for (usize jB = kB; jB < B_segend; jB++)                             \
        {                                                                    \
          usize col         = LOAD(right.col_indices[jB]);                   \
          f64   right_value = LOAD(right.values[jB]);                        \
                                                                             \
          usize output_index = row * output.col_count + col;                 \
          f64   output_value = LOAD(output.values[output_index]);            \
          FMADD(output_value, left_value, right_value);                      \
          STORE(output.values[output_index], output_value);                  \
        }                                                                    \
      }                                                                      \
                                                                             \
      kA += (kA0 == k);                                                      \
      kB  = B_segend;                                                        \
    }                                                                        \
  }                                                                          \
)

#define csc_x_dense_impl STATEMENT(                                          \
  for (usize col = 0; col < left.col_count; col++)                           \
  {                                                                          \
    usize col_start = LOAD(left.col_pointers[col]);                          \
    usize col_end   = LOAD(left.col_pointers[col + 1]);                      \
                                                                             \
    for (usize i = col_start; i < col_end; i++)                              \
    {                                                                        \
      usize left_row = LOAD(left.row_indices[i]);                            \
      f64 left_value = LOAD(left.values[i]);                                 \
                                                                             \
      for (usize right_col = 0; right_col < right.col_count; right_col++)    \
      {                                                                      \
        usize right_index  = col * right.col_count + right_col;              \
        usize output_index = left_row * output.col_count + right_col;        \
                                                                             \
        f64 right_value   = LOAD(right.values[right_index]);                 \
        f64 current_value = LOAD(output.values[output_index]);               \
                                                                             \
        f64 result_value = current_value;                                    \
        FMADD(result_value, left_value, right_value);                        \
                                                                             \
        STORE(output.values[output_index], result_value);                    \
      }                                                                      \
    }                                                                        \
  }                                                                          \
)

#define csc_x_csr_impl STATEMENT(                                                        \
  for (usize k = 0; k < right.row_count; k++)                                            \
  {                                                                                      \
    usize left_col_start = LOAD(left.col_pointers[k]);                                   \
    usize left_col_close = LOAD(left.col_pointers[k + 1]);                               \
                                                                                         \
    usize right_row_start = LOAD(right.row_pointers[k]);                                 \
    usize right_row_close = LOAD(right.row_pointers[k + 1]);                             \
                                                                                         \
    for (usize left_col = left_col_start; left_col < left_col_close; left_col++)         \
    {                                                                                    \
      usize row = LOAD(left.row_indices[left_col]);                                      \
      f64 left_value  = LOAD(left.values[left_col]);                                     \
                                                                                         \
      for (usize right_row = right_row_start; right_row < right_row_close; right_row++)  \
      {                                                                                  \
        usize col = LOAD(right.col_indices[right_row]);                                  \
        f64 right_value = LOAD(right.values[right_row]);                                 \
                                                                                         \
        usize output_index = row * output.col_count + col;                               \
        f64 output_value   = LOAD(output.values[output_index]);                          \
        FMADD(output_value, left_value, right_value);                                    \
                                                                                         \
        STORE(output.values[output_index], output_value);                                \
      }                                                                                  \
    }                                                                                    \
  }                                                                                      \
)

#define csc_x_csc_impl STATEMENT(                                          \
  for (usize right_col = 0; right_col < right.col_count; right_col++)      \
  {                                                                        \
    usize right_col_start = LOAD(right.col_pointers[right_col]);           \
    usize right_col_end   = LOAD(right.col_pointers[right_col + 1]);       \
                                                                           \
    for (usize i = right_col_start; i < right_col_end; i++)                \
    {                                                                      \
      usize right_row = LOAD(right.row_indices[i]);                        \
      f64 right_value = LOAD(right.values[i]);                             \
                                                                           \
      usize left_col_start = LOAD(left.col_pointers[right_row]);           \
      usize left_col_end   = LOAD(left.col_pointers[right_row + 1]);       \
      for (usize j = left_col_start; j < left_col_end; j++)                \
      {                                                                    \
        usize left_row = LOAD(left.row_indices[j]);                        \
        f64 left_value = LOAD(left.values[j]);                             \
                                                                           \
        usize output_index = left_row * output.col_count + right_col;      \
        f64 current_value = LOAD(output.values[output_index]);             \
                                                                           \
        f64 result_value = current_value;                                  \
        FMADD(result_value, left_value, right_value);                      \
                                                                           \
        STORE(output.values[output_index], result_value);                  \
      }                                                                    \
    }                                                                      \
  }                                                                        \
)

#define csc_x_coo_impl STATEMENT(                                         \
  usize kB     = 0;                                                       \
  usize kB_end = right.non_zero_count;                                    \
                                                                          \
  while (kB < kB_end)                                                     \
  {                                                                       \
    usize k = LOAD(right.row_indices[kB]);                                \
                                                                          \
    usize B_segend = kB + 1;                                              \
    usize at_right = LOAD(right.row_indices[B_segend]);                   \
    while (B_segend < kB_end && at_right == k)                            \
    {                                                                     \
      B_segend++;                                                         \
      at_right = LOAD(right.row_indices[B_segend]);                       \
    }                                                                     \
                                                                          \
    usize col_start = LOAD(left.col_pointers[k]);                         \
    usize col_end   = LOAD(left.col_pointers[k + 1]);                     \
    for (usize iA = col_start; iA < col_end; iA++)                        \
    {                                                                     \
      usize row        = LOAD(left.row_indices[iA]);                      \
      f64   left_value = LOAD(left.values[iA]);                           \
                                                                          \
      for (usize jB = kB; jB < B_segend; jB++)                            \
      {                                                                   \
        usize col         = LOAD(right.col_indices[jB]);                  \
        f64   right_value = LOAD(right.values[jB]);                       \
                                                                          \
        usize output_index = row * output.col_count + col;                \
        f64   output_value = LOAD(output.values[output_index]);           \
        FMADD(output_value, left_value, right_value);                     \
        STORE(output.values[output_index], output_value);                 \
      }                                                                   \
    }                                                                     \
                                                                          \
    kB = B_segend;                                                        \
  }                                                                       \
)

#define coo_x_dense_impl STATEMENT(                                       \
  usize kA     = 0;                                                       \
  usize kA_end = left.non_zero_count;                                     \
                                                                          \
  while (kA < kA_end)                                                     \
  {                                                                       \
    usize row = LOAD(left.row_indices[kA]);                               \
                                                                          \
    usize A_segend = kA + 1;                                              \
    usize at_left = LOAD(left.row_indices[A_segend]);                     \
    while (A_segend < kA_end && at_left == row)                           \
    {                                                                     \
      A_segend++;                                                         \
      at_left = LOAD(left.row_indices[A_segend]);                         \
    }                                                                     \
                                                                          \
    for (usize iA = kA; iA < A_segend; iA++)                              \
    {                                                                     \
      usize k          = LOAD(left.col_indices[iA]);                      \
      f64   left_value = LOAD(left.values[iA]);                           \
                                                                          \
      for (usize col = 0; col < right.col_count; col++)                   \
      {                                                                   \
        usize right_index  = k * right.col_count + col;                   \
        f64   right_value  = LOAD(right.values[right_index]);             \
                                                                          \
        usize output_index = row * output.col_count + col;                \
        f64   output_value = LOAD(output.values[output_index]);           \
        FMADD(output_value, left_value, right_value);                     \
        STORE(output.values[output_index], output_value);                 \
      }                                                                   \
    }                                                                     \
                                                                          \
    kA = A_segend;                                                        \
  }                                                                       \
)

#define coo_x_csr_impl STATEMENT(                                       \
  usize kA     = 0;                                                     \
  usize kA_end = left.non_zero_count;                                   \
                                                                        \
  while (kA < kA_end)                                                   \
  {                                                                     \
    usize row = LOAD(left.row_indices[kA]);                             \
                                                                        \
    usize A_segend = kA + 1;                                            \
    usize at_left = LOAD(left.row_indices[A_segend]);                   \
    while (A_segend < kA_end && at_left == row)                         \
    {                                                                   \
      A_segend++;                                                       \
      at_left = LOAD(left.row_indices[A_segend]);                       \
    }                                                                   \
                                                                        \
    for (usize iA = kA; iA < A_segend; iA++)                            \
    {                                                                   \
      usize k          = LOAD(left.col_indices[iA]);                    \
      f64   left_value = LOAD(left.values[iA]);                         \
                                                                        \
      usize row_start = LOAD(right.row_pointers[k]);                    \
      usize row_end   = LOAD(right.row_pointers[k + 1]);                \
      for (usize jB = row_start; jB < row_end; jB++)                    \
      {                                                                 \
        usize col         = LOAD(right.col_indices[jB]);                \
        f64   right_value = LOAD(right.values[jB]);                     \
                                                                        \
        usize output_index = row * output.col_count + col;              \
        f64   output_value = LOAD(output.values[output_index]);         \
        FMADD(output_value, left_value, right_value);                   \
        STORE(output.values[output_index], output_value);               \
      }                                                                 \
    }                                                                   \
                                                                        \
    kA = A_segend;                                                      \
  }                                                                     \
)

#define coo_x_csc_impl STATEMENT(                                       \
  usize kA     = 0;                                                     \
  usize kA_end = left.non_zero_count;                                   \
                                                                        \
  while (kA < kA_end)                                                   \
  {                                                                     \
    usize row = LOAD(left.row_indices[kA]);                             \
                                                                        \
    usize A_segend = kA + 1;                                            \
    usize at_left = LOAD(left.row_indices[A_segend]);                   \
    while (A_segend < kA_end && at_left == row)                         \
    {                                                                   \
      A_segend++;                                                       \
      at_left = LOAD(left.row_indices[A_segend]);                       \
    }                                                                   \
                                                                        \
    for (usize col = 0; col < right.col_count; col++)                   \
    {                                                                   \
      f64 accumulated = 0.0;                                            \
                                                                        \
      usize iA      = kA;                                               \
      usize jB      = LOAD(right.col_pointers[col]);                    \
      usize jB_end  = LOAD(right.col_pointers[col + 1]);                \
                                                                        \
      while (iA < A_segend && jB < jB_end)                              \
      {                                                                 \
        usize kA0 = LOAD(left.col_indices[iA]);                         \
        usize kB0 = LOAD(right.row_indices[jB]);                        \
        usize k   = MIN(kA0, kB0);                                      \
                                                                        \
        if (kA0 == k && kB0 == k)                                       \
        {                                                               \
          f64 left_value  = LOAD(left.values[iA]);                      \
          f64 right_value = LOAD(right.values[jB]);                     \
          FMADD(accumulated, left_value, right_value);                  \
        }                                                               \
                                                                        \
        iA += (kA0 == k);                                               \
        jB += (kB0 == k);                                               \
      }                                                                 \
                                                                        \
      usize output_index = row * output.col_count + col;                \
      STORE(output.values[output_index], accumulated);                  \
    }                                                                   \
                                                                        \
    kA = A_segend;                                                      \
  }                                                                     \
)

#define coo_x_coo_impl STATEMENT(                                       \
  usize kA     = 0;                                                     \
  usize kA_end = left.non_zero_count;                                   \
                                                                        \
  while (kA < kA_end)                                                   \
  {                                                                     \
    usize row = LOAD(left.row_indices[kA]);                             \
                                                                        \
    usize A_segend = kA + 1;                                            \
    usize at_left = LOAD(left.row_indices[A_segend]);                   \
    while (A_segend < kA_end && at_left == row)                         \
    {                                                                   \
      A_segend++;                                                       \
      at_left = LOAD(left.row_indices[A_segend]);                       \
    }                                                                   \
                                                                        \
    usize iA     = kA;                                                  \
    usize jB     = 0;                                                   \
    usize jB_end = right.non_zero_count;                                \
                                                                        \
    while (iA < A_segend && jB < jB_end)                                \
    {                                                                   \
      usize kA0 = LOAD(left.col_indices[iA]);                           \
      usize kB0 = LOAD(right.row_indices[jB]);                          \
      usize k   = MIN(kA0, kB0);                                        \
                                                                        \
      usize B_segend = jB;                                              \
      usize at_right = LOAD(right.row_indices[B_segend]);               \
      while (B_segend < jB_end && at_right == k)                        \
      {                                                                 \
        B_segend++;                                                     \
        at_right = LOAD(right.row_indices[B_segend]);                   \
      }                                                                 \
                                                                        \
      if (kA0 == k && kB0 == k)                                         \
      {                                                                 \
        f64 left_value = LOAD(left.values[iA]);                         \
                                                                        \
        for (usize jC = jB; jC < B_segend; jC++)                        \
        {                                                               \
          usize col         = LOAD(right.col_indices[jC]);              \
          f64   right_value = LOAD(right.values[jC]);                   \
                                                                        \
          usize output_index = row * output.col_count + col;            \
          f64   output_value = LOAD(output.values[output_index]);       \
          FMADD(output_value, left_value, right_value);                 \
          STORE(output.values[output_index], output_value);             \
        }                                                               \
      }                                                                 \
                                                                        \
      iA += (kA0 == k);                                                 \
      jB  = B_segend;                                                   \
    }                                                                   \
                                                                        \
    kA = A_segend;                                                      \
  }                                                                     \
)

static
void dense_x_dense(Dense_Matrix output, Dense_Matrix left, Dense_Matrix right);

static
void dense_x_csr(Dense_Matrix output, Dense_Matrix left, CSR_Matrix right);

static
void dense_x_csc(Dense_Matrix output, Dense_Matrix left, CSC_Matrix right);

static
void dense_x_coo(Dense_Matrix output, Dense_Matrix left, COO_Matrix right);

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
