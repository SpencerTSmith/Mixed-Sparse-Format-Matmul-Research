#define LOG_TITLE "SPARSE_BLIS"
#define COMMON_IMPLEMENTATION

#include "../common.h"
#include "formats.h"
#include "formats.c"

#ifndef BLOCK_NC
#define BLOCK_NC 192
#endif

#ifndef BLOCK_KC
#define BLOCK_KC 128
#endif

#ifndef BLOCK_MC
#define BLOCK_MC 128
#endif

#ifndef BLOCK_NR
#define BLOCK_NR 6
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
  Matrix_Format *formats;
  usize count;
};

typedef struct Multiformat_Matrix Multiformat_Matrix;
struct Multiformat_Matrix
{
  usize row_count;
  usize col_count;
  // TODO
};


static
Multiformat_Matrix multi_sparsify(Dense_Matrix matrix, Blocking_Description blocking)
{
  // TODO:
  Multiformat_Matrix result = {0};
  result.row_count = matrix.row_count;
  result.col_count = matrix.col_count;

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

  f64 left_density  = args_get_f64_value(&args, STR("left_density"), 0.1);
  f64 right_density = args_get_f64_value(&args, STR("right_density"), 0.1);

  // TODO: Load from file argument
  Dense_Matrix left_dense  = make_random_dense_matrix(&arena, 4096, 4096, left_density);
  Dense_Matrix right_dense = make_random_dense_matrix(&arena, 4096, 4096, right_density);

  // TODO: Once have pulp working probably should choose blocking description inside function.
  Multiformat_Matrix left  = multi_sparsify(left_dense, (Blocking_Description){0});
  Multiformat_Matrix right = multi_sparsify(right_dense, (Blocking_Description){0});

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
