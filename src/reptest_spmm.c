#define LOG_TITLE "REPETITION_TESTER"
#define COMMON_IMPLEMENTATION

#include "common.h"
#include "formats.h"
#include "instrument.h"
#include "benchmark/benchmark_inc.h"

#include "formats.c"
#include "benchmark/benchmark_inc.c"

typedef struct Operation_Parameters Operation_Parameters;
struct Operation_Parameters
{
  Matrix_Reps  left;
  Matrix_Reps  right;
  Dense_Matrix output;
};

static
void matmul_dense_dense(Repetition_Tester *tester, Operation_Parameters *params)
{
  Dense_Matrix left   = params->left.dense;
  Dense_Matrix right  = params->right.dense;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  for (usize row = 0; row < left.row_count; row++)
  {
    for (usize col = 0; col < right.col_count; col++)
    {
      f64 dot = 0.0;

      for (usize i = 0; i < left.col_count; i++)
      {
        usize left_index  = row * left.col_count + col;
        usize right_index = row * right.col_count + col;
        f64 left_value  = LOAD(left.values[left_index]);
        f64 right_value = LOAD(right.values[left_index]);

        FMADD(dot, left_value, right_value);
      }

      usize output_index = row * output.col_count + col;
      STORE(output.values[output_index], dot);
    }
  }

  repetition_tester_close_time(tester);

  repetition_tester_count_bytes(tester, output.row_count * output.col_count * sizeof(f64));
}

static
void matmul_csr_dense(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSR_Matrix left     = params->left.csr;
  Dense_Matrix right  = params->right.dense;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

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

  repetition_tester_close_time(tester);

  repetition_tester_count_bytes(tester, left.non_zero_count * right.col_count * sizeof(f64));
}

static
void matmul_csc_dense(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSC_Matrix left     = params->left.csc;
  Dense_Matrix right  = params->right.dense;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

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

  repetition_tester_close_time(tester);

  repetition_tester_count_bytes(tester, left.non_zero_count * right.col_count * sizeof(f64));
}

Operation_Entry test_entries[] =
{
  {String("dense x dense"),  matmul_dense_dense},
  {String("csr   x dense"),  matmul_csr_dense},
  {String("csc   x dense"),  matmul_csc_dense},
};

int main(int arg_count, char **args)
{
  if (arg_count != 5)
  {
    printf("Usage: %s [seconds_to_try_for_min] [row_count] [col_count] [sparsity]\n", args[0]);
  }

  u32 seconds_to_try_for_min = atoi(args[1]);
  u64 cpu_timer_frequency = estimate_cpu_timer_freq();

  Arena arena = arena_make(.reserve_size = GB(64));

  u32 row_count = atoi(args[2]);
  u32 col_count = atoi(args[3]);
  f64 sparsity  = atof(args[4]);

  Dense_Matrix left_dense  = make_random_dense_matrix(&arena, row_count, col_count, sparsity);
  Dense_Matrix right_dense = make_random_dense_matrix(&arena, row_count, col_count, sparsity);
  Dense_Matrix output =
  {
    .row_count = row_count,
    .col_count = col_count,
    .values = arena_calloc(&arena, row_count * col_count, f64),
  };

  Operation_Parameters params =
  {
    .left.dense = left_dense,
    .left.csr = csr_from_dense(&arena, &left_dense),
    .left.csc = csc_from_dense(&arena, &left_dense),

    .right.dense = right_dense,
    .right.csr = csr_from_dense(&arena, &right_dense),
    .right.csc = csc_from_dense(&arena, &right_dense),

    .output = output,
  };

  while (true)
  {
    Repetition_Tester testers[STATIC_ARRAY_COUNT(test_entries)] = {0};

    for (usize i = 0; i < STATIC_ARRAY_COUNT(test_entries); i++)
    {
      Repetition_Tester *tester = &testers[i];
      Operation_Entry *entry = &test_entries[i];

      printf("\n--- %.*s ---\n", String_Format(entry->name));
      printf("                                                          \r");
      repetition_tester_new_wave(tester, 0, cpu_timer_frequency, seconds_to_try_for_min);

      while (repetition_tester_is_testing(tester))
      {
        entry->function(tester, &params);
      }
    }
  }
}
