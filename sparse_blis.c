#define LOG_TITLE "SPARSE_BLIS"
#define COMMON_IMPLEMENTATION

#include <omp.h>

#include "benchmark/benchmark_inc.h"
#include "benchmark/benchmark_inc.c"

#include "common.h"
#include "formats.h"
#include "formats.c"

#ifndef BLOCK_NC
#define BLOCK_NC 128
#endif

#ifndef BLOCK_KC
#define BLOCK_KC 128
#endif

#ifndef BLOCK_MC
#define BLOCK_MC 128
#endif

#ifndef BLOCK_NR
#define BLOCK_NR 32
#endif

#ifndef BLOCK_MR
#define BLOCK_MR 32
#endif

#ifndef BLOCK_KU
#define BLOCK_KU 32
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

typedef struct Multisparse_Matrix Multisparse_Matrix;
struct Multisparse_Matrix
{
  // Overall.
  usize row_count;
  usize col_count;

  Matrix_Union *blocks;
  usize blocks_row_count;
  usize blocks_col_count;
};

// Horribly inefficient.
static
Dense_Matrix pack_matrix_block(Arena *arena, COO_Matrix parent, usize row_start, usize col_start,
                               usize row_block_size, usize col_block_size)
{
  Dense_Matrix result =
  {
    .values    = arena_calloc(arena, row_block_size * col_block_size, f64),
    .row_count = row_block_size,
    .col_count = col_block_size,
  };

  for (u32 i = 0; i < parent.non_zero_count; i += 1)
  {
    u32 r = parent.row_indices[i];
    u32 c = parent.col_indices[i];
    if (r >= row_start && r < row_start + row_block_size &&
        c >= col_start && c < col_start + col_block_size)
    {
      result.values[(r - row_start) * col_block_size + (c - col_start)] = parent.values[i];
    }
  }

  return result;
}

static
Multisparse_Matrix multi_sparsify(Arena *arena, COO_Matrix matrix,
                                  usize row_count, usize col_count,
                                  Blocking_Description blocking)
{
  Multisparse_Matrix result = {0};
  result.row_count = row_count;
  result.col_count = col_count;
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

          Matrix_Union sub_matrix = dense_to_format(arena, dense_block, format);

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
  MAT_COMBO_DENSE_COO,
  MAT_COMBO_CSR_DENSE,
  MAT_COMBO_CSR_CSR,
  MAT_COMBO_CSR_CSC,
  MAT_COMBO_CSR_COO,
  MAT_COMBO_CSC_DENSE,
  MAT_COMBO_CSC_CSR,
  MAT_COMBO_CSC_CSC,
  MAT_COMBO_CSC_COO,
  MAT_COMBO_COO_DENSE,
  MAT_COMBO_COO_CSR,
  MAT_COMBO_COO_CSC,
  MAT_COMBO_COO_COO,
} Matrix_Format_Dispatch;

static
void do_sparse_matmul(Dense_Matrix output, Matrix_Union left_union, Matrix_Union right_union)
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
      if (right.non_zero_count)
      {
        dense_x_csr(output, left, right);
      }
    } break;
    case MAT_COMBO_DENSE_CSC:
    {
      Dense_Matrix left = left_union.dense;
      CSC_Matrix right  = right_union.csc;
      if (right.non_zero_count)
      {
        dense_x_csc(output, left, right);
      }
    } break;
    case MAT_COMBO_DENSE_COO:
    {
      Dense_Matrix left = left_union.dense;
      COO_Matrix right  = right_union.coo;
      if (right.non_zero_count)
      {
        dense_x_coo(output, left, right);
      }
    } break;
    case MAT_COMBO_CSR_DENSE:
    {
      CSR_Matrix left    = left_union.csr;
      Dense_Matrix right = right_union.dense;
      if (left.non_zero_count)
      {
        csr_x_dense(output, left, right);
      }
    } break;
    case MAT_COMBO_CSR_CSR:
    {
      CSR_Matrix left  = left_union.csr;
      CSR_Matrix right = right_union.csr;
      if (left.non_zero_count && right.non_zero_count)
      {
        csr_x_csr(output, left, right);
      }
    } break;
    case MAT_COMBO_CSR_CSC:
    {
      CSR_Matrix left  = left_union.csr;
      CSC_Matrix right = right_union.csc;
      if (left.non_zero_count && right.non_zero_count)
      {
        csr_x_csc(output, left, right);
      }
    } break;
    case MAT_COMBO_CSR_COO:
    {
      CSR_Matrix left  = left_union.csr;
      COO_Matrix right = right_union.coo;
      if (left.non_zero_count && right.non_zero_count)
      {
        csr_x_coo(output, left, right);
      }
    } break;
    case MAT_COMBO_CSC_DENSE:
    {
      CSC_Matrix left    = left_union.csc;
      Dense_Matrix right = right_union.dense;
      if (left.non_zero_count)
      {
        csc_x_dense(output, left, right);
      }
    } break;
    case MAT_COMBO_CSC_CSR:
    {
      CSC_Matrix left  = left_union.csc;
      CSR_Matrix right = right_union.csr;
      if (left.non_zero_count && right.non_zero_count)
      {
        csc_x_csr(output, left, right);
      }
    } break;
    case MAT_COMBO_CSC_CSC:
    {
      CSC_Matrix left  = left_union.csc;
      CSC_Matrix right = right_union.csc;
      if (left.non_zero_count && right.non_zero_count)
      {
        csc_x_csc(output, left, right);
      }
    } break;
    case MAT_COMBO_CSC_COO:
    {
      CSC_Matrix left  = left_union.csc;
      COO_Matrix right = right_union.coo;
      if (left.non_zero_count)
      {
        csc_x_coo(output, left, right);
      }
    } break;
    case MAT_COMBO_COO_DENSE:
    {
      COO_Matrix left    = left_union.coo;
      Dense_Matrix right = right_union.dense;
      if (left.non_zero_count)
      {
        coo_x_dense(output, left, right);
      }
    } break;
    case MAT_COMBO_COO_CSR:
    {
      COO_Matrix left  = left_union.coo;
      CSR_Matrix right = right_union.csr;
      if (left.non_zero_count && right.non_zero_count)
      {
        coo_x_csr(output, left, right);
      }
    } break;
    case MAT_COMBO_COO_CSC:
    {
      COO_Matrix left  = left_union.coo;
      CSC_Matrix right = right_union.csc;
      if (left.non_zero_count && right.non_zero_count)
      {
        coo_x_csc(output, left, right);
      }
    } break;
    case MAT_COMBO_COO_COO:
    {
      COO_Matrix left  = left_union.coo;
      COO_Matrix right = right_union.coo;
      if (left.non_zero_count)
      {
        coo_x_coo(output, left, right);
      }
    } break;
  }
}

static
void sparse_blis(Dense_Matrix *output, Multisparse_Matrix left, Multisparse_Matrix right)
{
  ASSERT(left.col_count == right.row_count, "Matrices are not compatible for multiplication.");

  usize block_m = left.blocks_row_count;
  usize block_n = right.blocks_col_count;
  usize block_k = left.blocks_col_count;

  // Since we iterate by blocks and not be elements, gotta change steps and conditions.
  #pragma omp parallel for
  for (usize block_j_o = 0; block_j_o < block_n; block_j_o += BLOCK_J)
  {
    for (usize block_p_o = 0; block_p_o < block_k; block_p_o += BLOCK_P)
    {
      // DLT for B usually here.

      // #pragma omp parallel for
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
              do_sparse_matmul(temp, left_block, right_block);
            }

            // Update output with temp
            for (usize j_r = 0; j_r < BLOCK_NR; j_r += 1)
            {
              for (usize i_r = 0; i_r < BLOCK_MR; i_r += 1)
              {
                usize row = (block_i_o + block_i_i) * BLOCK_MR + i_r;
                usize col = (block_j_o + block_j_i) * BLOCK_NR + j_r;

                output->values[row * output->col_count + col] += temp.values[i_r * temp.col_count + j_r];
              }
            }
          }
        }
      }
    }
  }
}

#include <math.h>

static
b32 epsilon_equal(f64 a, f64 b)
{
  f64 epsilon = 0.00001;

  return fabs(a - b) <= epsilon;
}

typedef struct Matrix_Solution Matrix_Solution;
struct Matrix_Solution
{
  COO_Matrix left;
  usize left_row_count, left_col_count;
  COO_Matrix right;
  usize right_row_count, right_col_count;
  Blocking_Description left_blocking;
  Blocking_Description right_blocking;
};

static
const char *string_from_format(Matrix_Format format)
{
  const char *result = "";
  switch (format)
  {
    case MAT_NONE:
    {
    } break;
  }

  return result;
}

static
Matrix_Solution load_matrix_solution(Arena *arena, String filename, b32 is_dummy)
{
  Matrix_Solution result = {0};

  // NOTE: Hardcoded
  result.left_blocking.row_count = BLOCK_MR;
  result.left_blocking.col_count = BLOCK_KU;
  result.left_blocking.outer_step = BLOCK_I;
  result.left_blocking.inner_step = BLOCK_P;

  result.right_blocking.row_count = BLOCK_KU;
  result.right_blocking.col_count = BLOCK_NR;
  result.right_blocking.outer_step = BLOCK_J;
  result.right_blocking.inner_step = BLOCK_P;

  FILE *file;
  DEFER_SCOPE(file = fopen(string_to_c_string(arena, filename), "rb"), fclose(file))
  {
    if (!is_dummy)
    {
      // TODO: Do some asserts based on this data to make sure everything valid
      usize block_m_count = 0;
      usize block_n_count = 0;
      usize block_k_count = 0;
      fread(&block_m_count, sizeof(block_m_count), 1, file);
      fread(&block_n_count, sizeof(block_n_count), 1, file);
      fread(&block_k_count, sizeof(block_k_count), 1, file);


      result.left_blocking.block_formats = arena_calloc(arena,
                                                        block_m_count * block_k_count,
                                                        Matrix_Format);
      fread(result.left_blocking.block_formats, sizeof(Matrix_Format),
            block_m_count * block_k_count, file);

      result.right_blocking.block_formats = arena_calloc(arena,
                                                         block_n_count * block_k_count,
                                                         Matrix_Format);
      fread(result.right_blocking.block_formats, sizeof(Matrix_Format),
            block_n_count * block_k_count, file);
    }

    fread(&result.left_row_count, sizeof(u32), 1, file);
    fread(&result.left_col_count, sizeof(u32), 1, file);
    fread(&result.left.non_zero_count,  sizeof(u32), 1, file);
    result.left.values      = arena_calloc(arena, result.left.non_zero_count, f64);
    result.left.row_indices = arena_calloc(arena, result.left.non_zero_count, u16);
    result.left.col_indices = arena_calloc(arena, result.left.non_zero_count, u16);
    fread(result.left.row_indices, sizeof(u16), result.left.non_zero_count, file);
    fread(result.left.col_indices, sizeof(u16), result.left.non_zero_count, file);
    fread(result.left.values, sizeof(f64), result.left.non_zero_count, file);

    fread(&result.right_row_count, sizeof(u32), 1, file);
    fread(&result.right_col_count, sizeof(u32), 1, file);
    fread(&result.right.non_zero_count,  sizeof(u32), 1, file);
    result.right.values      = arena_calloc(arena, result.right.non_zero_count, f64);
    result.right.row_indices = arena_calloc(arena, result.right.non_zero_count, u16);
    result.right.col_indices = arena_calloc(arena, result.right.non_zero_count, u16);
    fread(result.right.row_indices, sizeof(u16), result.right.non_zero_count, file);
    fread(result.right.col_indices, sizeof(u16), result.right.non_zero_count, file);
    fread(result.right.values, sizeof(f64), result.right.non_zero_count, file);
  }

  return result;
}

typedef struct Operation_Parameters Operation_Parameters;
struct Operation_Parameters
{
  String name;
  Multisparse_Matrix left;
  Multisparse_Matrix right;
  Matrix_Union left_union;
  Matrix_Union right_union;
  Dense_Matrix output;
};

static
void reptest_solution(Repetition_Tester *tester, Operation_Parameters params)
{
  if (params.left_union.format == MAT_NONE)
  {
    repetition_tester_begin_time(tester);

    sparse_blis(&params.output, params.left, params.right);

    repetition_tester_close_time(tester);
  }
  else
  {
    ASSERT(params.left_union.format == MAT_CSR && params.right_union.format == MAT_CSR,
           "I know this is hacky.");
    CSR_Matrix left  = params.left_union.csr;
    CSR_Matrix right = params.right_union.csr;
    memset(params.output.values, 0,
          params.output.row_count * params.output.col_count * sizeof(f64));

    repetition_tester_begin_time(tester);

    csr_x_csr_parallel(params.output, left, right);

    repetition_tester_close_time(tester);
  }
}

static
Matrix_Format format_from_string(String string)
{
  Matrix_Format format = MAT_NONE;

  if (string_match(STR("MAT_DENSE"), string))
  {
    format = MAT_DENSE;
  }
  else if (string_match(STR("MAT_CSR"), string))
  {
    format = MAT_CSR;
  }
  else if (string_match(STR("MAT_CSC"), string))
  {
    format = MAT_CSR;
  }
  else if (string_match(STR("MAT_COO"), string))
  {
    format = MAT_COO;
  }
  else
  {
    format = MAT_DENSE;
    LOG_ERROR("Unkown matrix format string, defaulting to dense.");
  }

  return format;
}

int main(int argc, char **argv)
{
  Arena arena = arena_make(.reserve_size = GB(64));
  Args args = parse_args(&arena, argc, argv);

  b32 verify = args_has_flag(&args, STR("verify"));
  b32 is_dummy_solution = args_has_flag(&args, STR("dummy_solution"));

  String left_constant_blocking_string = args_get_string_value(&args,
                                                              STR("left_constant_blocking"),
                                                              STR("MAT_CSC"));
  String right_constant_blocking_string = args_get_string_value(&args,
                                                                STR("right_constant_blocking"),
                                                                STR("MAT_CSR"));
  String solution_filename = args_get_string_value(&args,
                                                   STR("solution"),
                                                   STR("solution.bin"));

  Matrix_Format left_constant_blocking = format_from_string(left_constant_blocking_string);
  Matrix_Format right_constant_blocking = format_from_string(right_constant_blocking_string);
  Matrix_Format left_global  = MAT_CSR;
  Matrix_Format right_global = MAT_CSR;

  Matrix_Solution solution = load_matrix_solution(&arena, solution_filename, is_dummy_solution);

  usize left_block_count  = (solution.left_row_count / solution.left_blocking.row_count)
                          * (solution.left_col_count / solution.left_blocking.col_count);

  usize right_block_count = (solution.right_row_count / solution.right_blocking.row_count)
                          * (solution.right_col_count / solution.right_blocking.col_count);


  Blocking_Description constant_left_blocking =
  {
    .block_formats = arena_calloc(&arena, left_block_count, Matrix_Format),
    .row_count     = solution.left_blocking.row_count,
    .col_count     = solution.left_blocking.col_count,
    .outer_step    = solution.left_blocking.outer_step,
    .inner_step    = solution.left_blocking.inner_step,
  };
  for (usize i = 0; i < left_block_count; i += 1)
  {
    constant_left_blocking.block_formats[i] = left_constant_blocking;
  }
  Blocking_Description constant_right_blocking =
  {
    .block_formats = arena_calloc(&arena, right_block_count, Matrix_Format),
    .row_count     = solution.right_blocking.row_count,
    .col_count     = solution.right_blocking.col_count,
    .outer_step    = solution.right_blocking.outer_step,
    .inner_step    = solution.right_blocking.inner_step,
  };
  for (usize i = 0; i < right_block_count; i += 1)
  {
    constant_right_blocking.block_formats[i] = right_constant_blocking;
  }

  Operation_Parameters params[] =
  {
    // {
    //   .name  = STR("Solver"),
    //   .left  = multi_sparsify(&arena, solution.left, solution.left_row_count, solution.left_col_count, solution.left_blocking),
    //   .right = multi_sparsify(&arena, solution.right, solution.left_row_count, solution.left_col_count, solution.right_blocking),
    //   .output =
    //   {
    //     .row_count = solution.left_row_count,
    //     .col_count = solution.right_col_count,
    //     .values = arena_calloc(&arena, solution.left_row_count * solution.right_col_count, f64),
    //   }
    // },
    {
      .name = string_formatted(&arena, "Constant Blocking %.*s x %.*s",
                               STRF(left_constant_blocking_string),
                               STRF(right_constant_blocking_string)),
      .left  = multi_sparsify(&arena, solution.left, solution.left_row_count, solution.left_col_count, constant_left_blocking),
      .right = multi_sparsify(&arena, solution.right, solution.right_row_count, solution.right_col_count, constant_right_blocking),
      .output =
      {
        .row_count = solution.left_row_count,
        .col_count = solution.right_col_count,
        .values = arena_calloc(&arena, solution.left_row_count * solution.right_col_count, f64),
      }
    },
    {
      .name = string_formatted(&arena, "Global CSR x CSR"),
      .left_union  = coo_to_format(&arena, solution.left, solution.left_row_count, solution.left_col_count, left_global),
      .right_union = coo_to_format(&arena, solution.right, solution.right_row_count, solution.right_col_count, right_global),
      .output =
      {
        .row_count = solution.left_row_count,
        .col_count = solution.right_col_count,
        .values = arena_calloc(&arena, solution.left_row_count * solution.right_col_count, f64),
      }
    }
  };
  LOG_INFO("left nnz: %zu, right nnz: %zu",
          params[1].left_union.csr.non_zero_count,
          params[1].right_union.csr.non_zero_count);

  Repetition_Tester testers[STATIC_COUNT(params)] = {0};

  u64 cpu_timer_frequency = estimate_cpu_timer_freq();

  omp_set_num_threads(2);

  u64 min_time = ~(u64)0;
  usize min_tester_index = 0;
  for (usize tester_index = 0; tester_index < STATIC_COUNT(testers); tester_index += 1)
  {
    Repetition_Tester *tester = testers + tester_index;
    Operation_Parameters entry = params[tester_index];

    printf("\n--- %.*s ---\n", STRF(entry.name));
    repetition_tester_new_wave(tester, 0, cpu_timer_frequency, 3);
    while (repetition_tester_is_testing(tester))
    {
      reptest_solution(tester, entry);
    }

    Repetition_Test_Values v = tester->results.min;
    if (v.v[REPTEST_VALUE_TIME] < min_time)
    {
      min_time = v.v[REPTEST_VALUE_TIME];
      min_tester_index = tester_index;
    }
  }

  for (usize tester_index = 0; tester_index < STATIC_COUNT(testers); tester_index += 1)
  {
    if (tester_index != min_tester_index)
    {
      Repetition_Tester *tester = testers + tester_index;
      u64 other_time = tester->results.min.v[REPTEST_VALUE_TIME];

      f64 percent_better = (f64)(other_time - min_time)/(f64)(other_time) * 100.0;

      LOG_INFO("%.*s is %.4f%% better than %.*s\n", STRF(params[min_tester_index].name), percent_better, STRF(params[tester_index].name));
    }
  }

  if (verify)
  {
    b32 had_failure = false;

    Dense_Matrix output =
    {
      .row_count = solution.left_row_count,
      .col_count = solution.right_col_count,
      .values = arena_calloc(&arena, solution.left_row_count * solution.right_col_count, f64),
    };
    sparse_blis(&output, params[0].left, params[0].right);

    Dense_Matrix reference =
    {
      .row_count = solution.left_row_count,
      .col_count = solution.right_col_count,
      .values = arena_calloc(&arena, solution.left_row_count * solution.right_col_count, f64),
    };

    coo_x_coo(reference, solution.left, solution.right);

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
