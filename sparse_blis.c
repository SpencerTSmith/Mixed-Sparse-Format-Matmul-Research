#define LOG_TITLE "SPARSE_BLIS"
#define COMMON_IMPLEMENTATION

#include "../benchmark/benchmark_inc.h"
#include "../benchmark/benchmark_inc.c"

#include "../common.h"
#include "formats.h"
#include "formats.c"

#ifndef BLOCK_NC
#define BLOCK_NC 256
#endif

#ifndef BLOCK_KC
#define BLOCK_KC 256
#endif

#ifndef BLOCK_MC
#define BLOCK_MC 256
#endif

#ifndef BLOCK_NR
#define BLOCK_NR 16
#endif

#ifndef BLOCK_MR
#define BLOCK_MR 16
#endif

#ifndef BLOCK_KU
#define BLOCK_KU 8
#endif

#if (BLOCK_NC) % (BLOCK_NR) != 0
#error "NR must be a factor of NC.\n"
#endif

#if (BLOCK_MC) % (BLOCK_MR) != 0
#error "MR must be a factor of MC.\n"
#endif

#if (BLOCK_KC) % (BLOCK_KU) != 0
#error "KU must be a factor of KC.\n"
#endif

#define BLOCK_I (BLOCK_MC / BLOCK_MR)
#define BLOCK_J (BLOCK_NC / BLOCK_NR)
#define BLOCK_P (BLOCK_KC / BLOCK_KU)

// Contains info for allocation order and type.
typedef struct Blocking_Description Blocking_Description;
struct Blocking_Description
{
  Matrix_Format *block_formats;
  usize row_count;
  usize col_count;

  usize outer_step;
  usize inner_step;
};

typedef struct Multiformat_Matrix Multisparse_Matrix;
struct Multiformat_Matrix
{
  // Overall.
  usize row_count;
  usize col_count;

  Matrix_Union *blocks;
  usize blocks_row_count;
  usize blocks_col_count;
};

static
Dense_Matrix pack_matrix_block(Arena *arena, Dense_Matrix parent, usize row_start, usize col_start,
                               usize row_block_size, usize col_block_size)
{
  Dense_Matrix result =
  {
    .values    = arena_calloc(arena, row_block_size * col_block_size, f64),
    .row_count = row_block_size,
    .col_count = col_block_size,
  };

  for (usize row = 0; row < row_block_size; row += 1)
  {
    for (usize col = 0; col < col_block_size; col += 1)
    {
      result.values[row * result.col_count + col] = parent.values[(row + row_start) * parent.col_count + (col + col_start)];
    }
  }

  return result;
}

static
Multisparse_Matrix multi_sparsify(Arena *arena, Dense_Matrix matrix,
                                  Blocking_Description blocking)
{
  Multisparse_Matrix result = {0};
  result.row_count = matrix.row_count;
  result.col_count = matrix.col_count;
  result.blocks_row_count = result.row_count / blocking.row_count;
  result.blocks_col_count = result.col_count / blocking.col_count;
  ASSERT(result.row_count % blocking.row_count == 0, "Block size must be a factor of matrix size.");
  ASSERT(result.col_count % blocking.col_count == 0, "Block size must be a factor of matrix size.");

  result.blocks = arena_calloc(arena, result.blocks_row_count * result.blocks_col_count, Matrix_Union);

  for (usize outer = 0; outer < result.blocks_col_count; outer += blocking.outer_step)
  {
    for (usize inner = 0; inner < result.blocks_row_count; inner += blocking.inner_step)
    {
      for (usize inner_i = 0; inner_i < blocking.inner_step; inner_i += 1)
      {
        for (usize outer_i = 0; outer_i < blocking.outer_step; outer_i += 1)
        {
          usize block_row = inner + inner_i;
          usize block_col = outer + outer_i;

          Matrix_Format format =
            blocking.block_formats[block_row * result.blocks_col_count + block_col];

          usize actual_row = block_row * blocking.row_count;
          usize actual_col = block_col * blocking.col_count;

          // TODO: Consider allocating this on scratch arena first as not needed
          // if we make it sparse.
          Dense_Matrix dense_block = pack_matrix_block(arena, matrix, actual_row, actual_col,
                                                       blocking.row_count, blocking.col_count);

          Matrix_Union sub_matrix = {0};
          switch (format)
          {
            default:
              {
                LOG_ERROR("Invalid matrix format.");
              } break;

            case MAT_DENSE:
              {
                sub_matrix = (Matrix_Union)
                {
                  .format = MAT_DENSE,
                  .dense  = dense_block,
                };
              } break;
            case MAT_CSR:
              {
                sub_matrix = (Matrix_Union)
                {
                  .format = MAT_CSR,
                  .csr    = csr_from_dense(arena, &dense_block),
                };
              } break;
            case MAT_CSC:
              {
                sub_matrix = (Matrix_Union)
                {
                  .format = MAT_CSC,
                  .csc    = csc_from_dense(arena, &dense_block),
                };
              } break;
          }

          result.blocks[block_row * result.blocks_col_count + block_col] = sub_matrix;
        }
      }
    }
  }

  return result;
}

typedef enum Matrix_Format_Dispatch
{
  MAT_COMBO_DENSE_DENSE,
  MAT_COMBO_DENSE_CSR,
  MAT_COMBO_DENSE_CSC,
  MAT_COMBO_CSR_DENSE,
  MAT_COMBO_CSR_CSR,
  MAT_COMBO_CSR_CSC,
  MAT_COMBO_CSC_DENSE,
  MAT_COMBO_CSC_CSR,
  MAT_COMBO_CSC_CSC,
} Matrix_Format_Dispatch;

static
void do_sparse_microkernel(Dense_Matrix output, Matrix_Union left_union, Matrix_Union right_union)
{
  ASSERT(left_union.format != MAT_NONE, "Invalid Matrix Format for micro-kernel.");
  ASSERT(right_union.format != MAT_NONE, "Invalid Matrix Format for micro-kernel.");

  Matrix_Format_Dispatch dispatch = (left_union.format - 1) * (MAT_COUNT - 1) + (right_union.format - 1);
  switch (dispatch)
  {
    default:
    {
      LOG_ERROR("Invalid matrix format dispatch.");;
    } break;

    case MAT_COMBO_DENSE_DENSE:
    {
      Dense_Matrix left  = left_union.dense;
      Dense_Matrix right = right_union.dense;
      dense_x_dense(output, left, right);
    } break;
    case MAT_COMBO_DENSE_CSR:
    {
      Dense_Matrix left = left_union.dense;
      CSR_Matrix right  = right_union.csr;
      dense_x_csr(output, left, right);
    } break;
    case MAT_COMBO_DENSE_CSC:
    {
      Dense_Matrix left = left_union.dense;
      CSC_Matrix right  = right_union.csc;
      dense_x_csc(output, left, right);
    } break;
    case MAT_COMBO_CSR_DENSE:
    {
      CSR_Matrix left    = left_union.csr;
      Dense_Matrix right = right_union.dense;
      csr_x_dense(output, left, right);
    } break;
    case MAT_COMBO_CSR_CSR:
    {
      CSR_Matrix left  = left_union.csr;
      CSR_Matrix right = right_union.csr;
      csr_x_csr(output, left, right);
    } break;
    case MAT_COMBO_CSR_CSC:
    {
      CSR_Matrix left  = left_union.csr;
      CSC_Matrix right = right_union.csc;
      csr_x_csc(output, left, right);
    } break;
    case MAT_COMBO_CSC_DENSE:
    {
      CSC_Matrix left    = left_union.csc;
      Dense_Matrix right = right_union.dense;
      csc_x_dense(output, left, right);
    } break;
    case MAT_COMBO_CSC_CSR:
    {
      CSC_Matrix left  = left_union.csc;
      CSR_Matrix right = right_union.csr;
      csc_x_csr(output, left, right);
    } break;
    case MAT_COMBO_CSC_CSC:
    {
      CSC_Matrix left  = left_union.csc;
      CSC_Matrix right = right_union.csc;
      csc_x_csc(output, left, right);
    } break;
  }
}

static
Dense_Matrix sparse_blis(Arena *arena, Multisparse_Matrix left, Multisparse_Matrix right)
{
  ASSERT(left.col_count == right.row_count, "Matrices are not compatible for multiplication.");

  Dense_Matrix output =
  {
    .row_count = left.row_count,
    .col_count = right.col_count,
    .values = arena_calloc(arena, left.row_count * right.col_count, f64),
  };

  usize block_m = left.blocks_row_count;
  usize block_n = right.blocks_col_count;
  usize block_k = left.blocks_col_count;

  // I still really don't understand what this blocking gets us when sparse. A simpler loop structure I
  // think would be able to get all the reuse out of our smaller blocks.

  // Since we iterate by blocks and not be elements, gotta change steps and conditions.
  for (usize block_j_o = 0; block_j_o < block_n; block_j_o += BLOCK_J)
  {
    for (usize block_p_o = 0; block_p_o < block_k; block_p_o += BLOCK_P)
    {
      // DLT for B usually here.

      for (usize block_i_o = 0; block_i_o < block_m; block_i_o += BLOCK_I)
      {
        // DLT for A usually here.

        for (usize block_j_i = 0; block_j_i < BLOCK_J; block_j_i += 1)
        {
          for (usize block_i_i = 0; block_i_i < BLOCK_I; block_i_i += 1)
          {
            Dense_Matrix temp =
            {
              .values = (f64[BLOCK_MR * BLOCK_NR]) {0},
              .row_count = BLOCK_MR,
              .col_count = BLOCK_NR,
            };

            for (usize block_p_i = 0; block_p_i < BLOCK_P; block_p_i += 1)
            {
              usize block_i = block_i_o + block_i_i;
              usize block_j = block_j_o + block_j_i;
              usize block_p = block_p_o + block_p_i;

              Matrix_Union left_block  = left.blocks[block_i * left.blocks_col_count + block_p];
              Matrix_Union right_block = right.blocks[block_p * right.blocks_col_count + block_j];
              do_sparse_microkernel(temp, left_block, right_block);
            }

            // Update output with temp
            for (usize j_r = 0; j_r < BLOCK_NR; j_r += 1)
            {
              for (usize i_r = 0; i_r < BLOCK_MR; i_r += 1)
              {
                usize row = (block_i_o + block_i_i) * BLOCK_MR + i_r;
                usize col = (block_j_o + block_j_i) * BLOCK_NR + j_r;

                output.values[row * output.col_count + col] += temp.values[i_r * temp.col_count + j_r];
              }
            }
          }
        }
      }
    }
  }

  return output;
}

#include <math.h>

static
b32 epsilon_equal(f64 a, f64 b)
{
  f64 epsilon = 0.00001;

  return fabs(a - b) <= epsilon;
}

int main(int argc, char **argv)
{
  Arena arena = arena_make(.reserve_size = GB(64));
  Args args = parse_args(&arena, argc, argv);

  b32 verify = args_has_flag(&args, STR("verify"));

  f64 left_density  = args_get_f64_value(&args, STR("left_density"),  0.1);
  f64 right_density = args_get_f64_value(&args, STR("right_density"), 0.1);

  // TODO: Load from file argument
#define MATRIX_SIZE 1024
  Dense_Matrix left_dense  = make_random_dense_matrix(&arena, MATRIX_SIZE, MATRIX_SIZE, left_density);
  Dense_Matrix right_dense = make_random_dense_matrix(&arena, MATRIX_SIZE, MATRIX_SIZE, right_density);

  Blocking_Description left_blocking =
  {
    .block_formats = (Matrix_Format[(MATRIX_SIZE/BLOCK_MR) * (MATRIX_SIZE/BLOCK_KU)]){0},
    .row_count     = BLOCK_MR,
    .col_count     = BLOCK_KU,
    .outer_step    = BLOCK_I,
    .inner_step    = BLOCK_P,
  };
  for (usize i = 0; i < (MATRIX_SIZE/BLOCK_MR) * (MATRIX_SIZE/BLOCK_KU); i += 1) { left_blocking.block_formats[i] = MAT_DENSE; }
  Multisparse_Matrix left  = multi_sparsify(&arena, left_dense, left_blocking);

  Blocking_Description right_blocking =
  {
    .block_formats = (Matrix_Format[(MATRIX_SIZE/BLOCK_KU) * (MATRIX_SIZE/BLOCK_NR)]){0},
    .row_count     = BLOCK_KU,
    .col_count     = BLOCK_NR,
    .outer_step    = BLOCK_J,
    .inner_step    = BLOCK_P,
  };
  for (usize i = 0; i < (MATRIX_SIZE/BLOCK_KU) * (MATRIX_SIZE/BLOCK_NR); i += 1) { right_blocking.block_formats[i] = MAT_CSC; }
  Multisparse_Matrix right = multi_sparsify(&arena, right_dense, right_blocking);

  Dense_Matrix output = sparse_blis(&arena, left, right);

  if (verify)
  {
    b32 had_failure = false;

    Dense_Matrix reference =
    {
      .row_count = left.row_count,
      .col_count = right.col_count,
      .values = arena_calloc(&arena, left.row_count * right.col_count, f64),
    };

    dense_x_dense(reference, left_dense, right_dense);

    for (usize i = 0; i < output.row_count * output.col_count; i += 1)
    {
      if (!epsilon_equal(output.values[i], reference.values[i]))
      {
        LOG_ERROR("Output does not match reference (%f:%f)", reference.values[i], output.values[i]);
        had_failure = true;
        break;
      }
    }

    arena_clear(&arena);

    if (!had_failure)
    {
      LOG_INFO("All entries match reference");
    }
  }
}
