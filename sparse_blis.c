#define LOG_TITLE "SPARSE_BLIS"
#define COMMON_IMPLEMENTATION

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

typedef struct Blocking_Description Blocking_Description;
struct Blocking_Description
{
  Matrix_Format *block_formats;
  usize row_count;
  usize col_count;
};

typedef struct Multiformat_Matrix Multiformat_Matrix;
struct Multiformat_Matrix
{
  // Overall.
  usize row_count;
  usize col_count;

  // How many blocks
  usize blocks_row_count;
  usize blocks_col_count;

  // For left  blocks should be of size MR x KC
  // For right blocks should be of size KC x NR
  Matrix_Union *blocks;
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

  for (usize row = row_start; row < row_block_size; row += 1)
  {
    for (usize col = col_start; col < col_block_size; col += 1)
    {
      result.values[row * result.col_count + col] = parent.values[row * parent.col_count + col];
    }
  }

  return result;
}

static
Multiformat_Matrix multi_sparsify(Arena *arena, Dense_Matrix matrix,
                                  Blocking_Description blocking)
{
  Multiformat_Matrix result = {0};
  result.blocks_row_count = result.row_count / blocking.row_count;
  result.blocks_col_count = result.col_count / blocking.col_count;
  result.row_count = matrix.row_count;
  result.col_count = matrix.col_count;
  result.blocks = arena_calloc(arena, result.blocks_row_count * result.blocks_col_count, f64);

  for (usize block_row = 0; block_row < result.blocks_row_count; block_row += 1)
  {
    for (usize block_col = 0; block_col < result.blocks_col_count; block_col += 1)
    {
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

  return result;
}

static
Matrix_Union get_matrix_block(Multiformat_Matrix matrix, usize row_block, usize col_block)
{
  // TODO:
  Matrix_Union result = {0};

  return result;
}

static
Dense_Matrix do_sparse_microkernel(Matrix_Union left, Matrix_Union right)
{
  // TODO:
  Dense_Matrix result = {0};

  return result;
}

int main(int argc, char **argv)
{
  Arena arena = arena_make(.reserve_size = GB(64));
  Args args = parse_args(&arena, argc, argv);

  f64 left_density  = args_get_f64_value(&args, STR("left_density"),  0.1);
  f64 right_density = args_get_f64_value(&args, STR("right_density"), 0.1);

  // TODO: Load from file argument
  Dense_Matrix left_dense  = make_random_dense_matrix(&arena, BLOCK_MR, BLOCK_KC, left_density);
  Dense_Matrix right_dense = make_random_dense_matrix(&arena, BLOCK_KC, BLOCK_NR, right_density);

  Blocking_Description left_blocking =
  {
    .block_formats = (Matrix_Format[BLOCK_MR * BLOCK_KC]){0}, // TODO
    .row_count     = BLOCK_MR,
    .col_count     = BLOCK_KC,
  };
  Multiformat_Matrix left  = multi_sparsify(&arena, left_dense, left_blocking);

  Blocking_Description right_blocking =
  {
    .block_formats = (Matrix_Format[BLOCK_KC * BLOCK_NR]){0}, // TODO
    .row_count     = BLOCK_KC,
    .col_count     = BLOCK_NR,
  };
  Multiformat_Matrix right = multi_sparsify(&arena, right_dense, right_blocking);

  ASSERT(left.col_count == right.row_count, "Matrices are not compatible for multiplication.");

  Dense_Matrix output =
  {
    .row_count = left.row_count,
    .col_count = right.col_count,
    .values = arena_calloc(&arena, left.row_count * right.col_count, f64),
  };

  // TODO: Fringes if present
  usize m = left.row_count;
  usize n = right.col_count;
  usize k = left.col_count;

  for (usize j_o = 0; j_o < n; j_o += BLOCK_NC)
  {
    for (usize p_o = 0; p_o < k; p_o += BLOCK_KC)
    {
      // DLT for B usually here.

      for (usize i_o = 0; i_o < BLOCK_NR; i_o += BLOCK_MC)
      {
        // DLT for A usually here.

        for (usize j_i = 0; j_i < BLOCK_NC; j_i += BLOCK_NR)
        {
          for (usize i_i = 0; i_i < BLOCK_NC; i_i += BLOCK_NR)
          {
            Matrix_Union left_block  = get_matrix_block(left, i_i, j_i);
            Matrix_Union right_block = get_matrix_block(left, j_i, i_i);
            Dense_Matrix temp = do_sparse_microkernel(left_block, right_block);

            // Update output with temp
            for (usize j_r = 0; j_r < BLOCK_NR; j_r += 1)
            {
              for (usize i_r = 0; i_r < BLOCK_MR; i_r += 1)
              {
                // TODO: allow for different orderings
                usize j = j_o + j_i + j_r;
                usize i = i_o + i_i + i_r;

                output.values[i * output.col_count + j] +=
                  temp.values[i_r * temp.col_count + j_r];
              }
            }
          }
        }
      }
    }
  }
}
