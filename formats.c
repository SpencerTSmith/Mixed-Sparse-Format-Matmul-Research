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
  result.col_indices  = arena_calloc(arena, result.non_zero_count, u32);
  result.row_pointers = arena_calloc(arena, result.row_count + 1, u32);

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
  result.row_indices  = arena_calloc(arena, result.non_zero_count, u32);
  result.col_pointers = arena_calloc(arena, result.col_count + 1, u32);

  isize non_zero_index = 0;
  for (isize c = 0; c < dense->col_count; c++)
  {
    for (isize r = 0; r < dense->row_count; r++)
    {
      // NOTE: Row major
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

// AHHHHHH
static
void dense_x_dense(Dense_Matrix output, Dense_Matrix left, Dense_Matrix right)
{
  for (usize row = 0; row < left.row_count; row++)
  {
    for (usize col = 0; col < right.col_count; col++)
    {
      usize output_index = row * output.col_count + col;
      f64 dot = LOAD(output.values[output_index]);

      for (usize i = 0; i < left.col_count; i++)
      {
        usize left_index  = row * left.col_count + i;
        usize right_index = i * right.col_count + col;
        f64 left_value  = LOAD(left.values[left_index]);
        f64 right_value = LOAD(right.values[right_index]);

        FMADD(dot, left_value, right_value);
      }

      STORE(output.values[output_index], dot);
    }
  }
}

static
void dense_x_csr(Dense_Matrix output, Dense_Matrix left, CSR_Matrix right)
{
  for (usize row = 0; row < left.row_count; row++)
  {
    for (usize k = 0; k < right.row_count; k++)
    {
      usize left_index = row * left.col_count + k;
      f64 left_value   = LOAD(left.values[left_index]);

      usize right_row_start = LOAD(right.row_pointers[k]);
      usize right_row_close = LOAD(right.row_pointers[k + 1]);

      for (usize rj = right_row_start; rj < right_row_close; rj++)
      {
        usize col = LOAD(right.col_indices[rj]);
        f64 right_value = LOAD(right.values[rj]);

        usize output_index = row * output.col_count + col;
        f64 output_value   = LOAD(output.values[output_index]);

        FMADD(output_value, left_value, right_value);

        STORE(output.values[output_index], output_value);
      }
    }
  }

}

static
void dense_x_csc(Dense_Matrix output, Dense_Matrix left, CSC_Matrix right)
{
  for (usize row = 0; row < left.row_count; row++)
  {
    for (usize col = 0; col < right.col_count; col++)
    {
      usize output_index = row * output.col_count + col;
      f64 output_value = LOAD(output.values[output_index]);

      usize right_col_start = LOAD(right.col_pointers[col]);
      usize right_col_close   = LOAD(right.col_pointers[col + 1]);
      for (usize kc = right_col_start; kc < right_col_close; kc++)
      {
        usize k = LOAD(right.row_indices[kc]);
        f64 right_value = LOAD(right.values[kc]);

        usize left_index = row * left.col_count + k;
        f64 left_value = LOAD(left.values[left_index]);

        FMADD(output_value, left_value, right_value);
      }

      STORE(output.values[output_index], output_value);
    }
  }

}

static
void csr_x_dense(Dense_Matrix output, CSR_Matrix left, Dense_Matrix right)
{
  for (usize row = 0; row < left.row_count; row++)
  {
    // Double count?
    usize row_start = LOAD(left.row_pointers[row]);
    usize row_end   = LOAD(left.row_pointers[row + 1]);

    for (usize i = row_start; i < row_end; i++)
    {
      usize left_col = LOAD(left.col_indices[i]);
      f64 left_value = LOAD(left.values[i]);

      for (usize right_col = 0; right_col < right.col_count; right_col++)
      {
        usize right_index  = left_col * right.col_count + right_col; // ALU op
        usize output_index = row * output.col_count + right_col;     // ALU op

        f64 right_value   = LOAD(right.values[right_index]);
        f64 current_value = LOAD(output.values[output_index]);

        f64 result_value = current_value;
        FMADD(result_value, left_value, right_value);

        STORE(output.values[output_index], result_value);
      }
    }
  }

}

static
void csr_x_csr(Dense_Matrix output, CSR_Matrix left, CSR_Matrix right)
{
  for (usize left_row = 0; left_row < left.row_count; left_row++)
  {
    usize left_row_start = LOAD(left.row_pointers[left_row]);
    usize left_row_end   = LOAD(left.row_pointers[left_row + 1]);

    for (usize i = left_row_start; i < left_row_end; i++)
    {
      usize left_col = LOAD(left.col_indices[i]);
      f64 left_value = LOAD(left.values[i]);

      usize right_row_start = LOAD(right.row_pointers[left_col]);
      usize right_row_end   = LOAD(right.row_pointers[left_col + 1]);
      for (usize j = right_row_start; j < right_row_end; j++)
      {
        usize right_col = LOAD(right.col_indices[j]);
        f64 right_value = LOAD(right.values[j]);

        usize output_index = left_row * output.col_count + right_col;
        f64 current_value = LOAD(output.values[output_index]);

        f64 result_value = current_value;
        FMADD(result_value, left_value, right_value);

        STORE(output.values[output_index], result_value);
      }
    }
  }

}

static
void csr_x_csc(Dense_Matrix output, CSR_Matrix left, CSC_Matrix right)
{
  for (usize left_row = 0; left_row < left.row_count; left_row++)
  {
    usize left_row_start = LOAD(left.row_pointers[left_row]);
    usize left_row_end   = LOAD(left.row_pointers[left_row + 1]);

    for (usize right_col = 0; right_col < right.col_count; right_col++)
    {
      usize right_col_start = LOAD(right.col_pointers[right_col]);
      usize right_col_end   = LOAD(right.col_pointers[right_col + 1]);

      usize output_index = left_row * output.col_count + right_col;
      f64 result_value = LOAD(output.values[output_index]);

      usize left_cursor  = left_row_start;
      usize right_cursor = right_col_start;
      while (left_cursor < left_row_end && right_cursor < right_col_end)
      {
        usize left_col  = LOAD(left.col_indices[left_cursor]);
        usize right_row = LOAD(right.row_indices[right_cursor]);
        usize k = MIN(left_col, right_row);

        if (left_col == k && right_row == k)
        {
          f64 left_value  = LOAD(left.values[left_cursor]);
          f64 right_value = LOAD(right.values[right_cursor]);
          FMADD(result_value, left_value, right_value);

        }
        left_cursor  += (usize)(left_col == k);
        right_cursor += (usize)(right_row == k);
      }

      STORE(output.values[output_index], result_value);
    }
  }

}

static
void csc_x_dense(Dense_Matrix output, CSC_Matrix left, Dense_Matrix right)
{
  for (usize col = 0; col < left.col_count; col++)
  {
    usize col_start = LOAD(left.col_pointers[col]);
    usize col_end   = LOAD(left.col_pointers[col + 1]);

    for (usize i = col_start; i < col_end; i++)
    {
      usize left_row = LOAD(left.row_indices[i]);
      f64 left_value = LOAD(left.values[i]);

      for (usize right_col = 0; right_col < right.col_count; right_col++)
      {
        usize right_index  = col * right.col_count + right_col;
        usize output_index = left_row * output.col_count + right_col;

        f64 right_value   = LOAD(right.values[right_index]);
        f64 current_value = LOAD(output.values[output_index]);

        f64 result_value = current_value;
        FMADD(result_value, left_value, right_value);

        STORE(output.values[output_index], result_value);
      }
    }
  }

}

static
void csc_x_csr(Dense_Matrix output, CSC_Matrix left, CSR_Matrix right)
{
  for (usize k = 0; k < right.row_count; k++)
  {
    usize left_col_start = LOAD(left.col_pointers[k]);
    usize left_col_close = LOAD(left.col_pointers[k + 1]);

    usize right_row_start = LOAD(right.row_pointers[k]);
    usize right_row_close = LOAD(right.row_pointers[k + 1]);

    for (usize left_col = left_col_start; left_col < left_col_close; left_col++)
    {
      usize row = LOAD(left.row_indices[left_col]);
      f64 left_value  = LOAD(left.values[left_col]);

      for (usize right_row = right_row_start; right_row < right_row_close; right_row++)
      {
        usize col = LOAD(right.col_indices[right_row]);
        f64 right_value = LOAD(right.values[right_row]);

        usize output_index = row * output.col_count + col;
        f64 output_value   = LOAD(output.values[output_index]);
        FMADD(output_value, left_value, right_value);

        STORE(output.values[output_index], output_value);
      }
    }
  }

}

static
void csc_x_csc(Dense_Matrix output, CSC_Matrix left, CSC_Matrix right)
{

  for (usize right_col = 0; right_col < right.col_count; right_col++)
  {
    usize right_col_start = LOAD(right.col_pointers[right_col]);
    usize right_col_end   = LOAD(right.col_pointers[right_col + 1]);

    for (usize i = right_col_start; i < right_col_end; i++)
    {
      usize right_row = LOAD(right.row_indices[i]);
      f64 right_value = LOAD(right.values[i]);

      usize left_col_start = LOAD(left.col_pointers[right_row]);
      usize left_col_end   = LOAD(left.col_pointers[right_row + 1]);
      for (usize j = left_col_start; j < left_col_end; j++)
      {
        usize left_row = LOAD(left.row_indices[j]);
        f64 left_value = LOAD(left.values[j]);

        usize output_index = left_row * output.col_count + right_col;
        f64 current_value = LOAD(output.values[output_index]);

        f64 result_value = current_value;
        FMADD(result_value, left_value, right_value);

        STORE(output.values[output_index], result_value);
      }
    }
  }
}
