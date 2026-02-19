#define LOG_TITLE "REPETITION_TESTER"
#define COMMON_IMPLEMENTATION


#include "common.h"
#include "formats.h"
#include "benchmark/benchmark_inc.h"

String string_make_formatted(Arena *arena, const char *format_string, ...)
{
  u8 *base = arena_new(arena, u8);
  va_list args;
  va_start(args, format_string);
  // TODO: no stdlib bullshit
  vsnprintf(base, arena->commit_size - arena->next_offset, format_string, args);
  va_end(args);

  String result = string_from_c_string(base);

  // FIXME: DANGER!
  arena->next_offset += result.count + 1;

  return result;
}

#include <time.h>
String string_create_timestamp(Arena *arena)
{
  time_t t = time(NULL);
  struct tm *time_thing = localtime(&t);
  i32 year  = time_thing->tm_year + 1900; // why?
  i32 month = time_thing->tm_mon;
  i32 day   = time_thing->tm_wday;
  i32 hour  = time_thing->tm_hour;
  i32 min   = time_thing->tm_min;
  i32 sec   = time_thing->tm_sec;

  String result = string_make_formatted(arena, "%d-%d-%d_%d:%d:%d",
                                        year, month, day, hour, min, sec);

  return result;
}

#ifndef OBSERVE_FLOPS
#define FMADD(dst, a, b) dst += (a * b);
#else
#define FMADD(dst, a, b) dst += (a * b); repetition_tester_count_flops(tester, 2)
#endif // OBSERVE_FLOPS

#ifndef OBSERVE_MEMOPS
#define LOAD(src)       src;
#define STORE(dst, src) dst = src;
#else
#define LOAD(src)       src;       repetition_tester_count_memops(tester, 1)
#define STORE(dst, src) dst = src; repetition_tester_count_memops(tester, 1)
#endif // OBSERVE_MEMOPS

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
        usize left_index  = row * left.col_count + i;
        usize right_index = i * right.col_count + col;
        f64 left_value  = LOAD(left.values[left_index]);
        f64 right_value = LOAD(right.values[right_index]);

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

static
void matmul_csr_csr(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSR_Matrix left  = params->left.csr;
  CSR_Matrix right = params->right.csr;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

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

  repetition_tester_close_time(tester);

  repetition_tester_count_bytes(tester,
                                left.non_zero_count * right.non_zero_count / params->right.dense.col_count * sizeof(f64));
}

static
void matmul_csc_csc(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSC_Matrix left  = params->left.csc;
  CSC_Matrix right = params->right.csc;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

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

  repetition_tester_close_time(tester);

  repetition_tester_count_bytes(tester,
                                left.non_zero_count * right.non_zero_count / params->right.dense.col_count * sizeof(f64));
}


Operation_Entry test_entries[] =
{
  {STR("dense_X_dense"), matmul_dense_dense},
  {STR("csr_X_dense"),   matmul_csr_dense},
  {STR("csc_X_dense"),   matmul_csc_dense},
  {STR("csr_X_csr"),     matmul_csr_csr},
  {STR("csc_X_csc"),     matmul_csc_csc},
};

#include <math.h>

static
b32 epsilon_equal(f64 a, f64 b)
{
  f64 epsilon = 0.00001;

  return fabs(a - b) <= epsilon;
}

Operation_Parameters init_params(Arena *arena, u32 row_count, u32 col_count, u32 inner_count, f64 density)
{
  Dense_Matrix left_dense  = make_random_dense_matrix(arena, row_count, inner_count, density);
  Dense_Matrix right_dense = make_random_dense_matrix(arena, inner_count, col_count, density);
  Dense_Matrix output =
  {
    .row_count = row_count,
    .col_count = col_count,
    .values = arena_calloc(arena, row_count * col_count, f64),
  };

  Operation_Parameters params =
  {
    .left.dense = left_dense,
    .left.csr = csr_from_dense(arena, &left_dense),
    .left.csc = csc_from_dense(arena, &left_dense),

    .right.dense = right_dense,
    .right.csr = csr_from_dense(arena, &right_dense),
    .right.csc = csc_from_dense(arena, &right_dense),

    .output = output,
  };

  return params;
}

int main(int arg_count, char **args)
{
  if (arg_count < 5)
  {
    printf("Usage: %s [seconds_to_try_for_min] [row_count] [col_count] [inner_count] [verify/no-verify]\n", args[0]);
    return -1;
  }

  Arena arena = arena_make(.reserve_size = GB(64));

  u32 seconds_to_try_for_min = atoi(args[1]);
  u64 cpu_timer_frequency = estimate_cpu_timer_freq();

  u32 row_count = atoi(args[2]);
  u32 col_count = atoi(args[3]);
  u32 inner_count = atoi(args[4]);

  if (arg_count == 6)
  {
    if (strcmp(args[5], "verify") == 0)
    {
      // Arbitrary sparsity to check
      Operation_Parameters params = init_params(&arena, row_count, col_count, inner_count, 0.1);

      b32 had_failure = false;
      Repetition_Tester dummy = {0};
      // Just gonna take a copy of the dense dense to compare against
      matmul_dense_dense(&dummy, &params);

      usize count = params.output.row_count * params.output.col_count;
      f64 *reference = arena_calloc(&arena, count, f64);
      MEM_COPY(reference, params.output.values, sizeof(f64) * count);

      for (isize i = 1; i < STATIC_COUNT(test_entries); i++)
      {
        Operation_Entry *entry = test_entries + i;

        MEM_SET(params.output.values, sizeof(f64) * params.output.row_count * params.output.col_count, 0);
        entry->function(&dummy, &params);

        for (isize v = 0; v < count; v++)
        {
          if (!epsilon_equal(params.output.values[v], reference[v]))
          {
            LOG_ERROR("Entry '%.*s' does not match reference (%f:%f)",
                      STRF(entry->name), reference[v], params.output.values[v]);
            had_failure = true;
            break;
          }
        }
      }

      arena_clear(&arena);

      if (!had_failure)
      {
        LOG_INFO("All entries match reference");
      }
    }
  }

  f64 densities[20] = {0};
  f64 density_delta = 0.001;
  f64 density_accum = 0.01;
  for (usize density_idx = 0; density_idx < STATIC_COUNT(densities); density_idx++)
  {
    densities[density_idx] = density_accum;

    // A little floating point error but good enough
    if ((density_idx % 10) == 0)
    {
      density_delta *= 10;
    }
    density_accum += density_delta;
  }

  Repetition_Tester testers[STATIC_COUNT(test_entries)][STATIC_COUNT(densities)] = {0};

  u32 non_zero_counts[STATIC_COUNT(densities)][2] = {0};

  for (usize density_idx = 1; density_idx < STATIC_COUNT(densities); density_idx++)
  {
    // FIXME: So SLOW! But don't know of a better way to test a bunch of densities of different
    // matrix sizes
    Operation_Parameters params = init_params(&arena,
                                              row_count, col_count, inner_count,
                                              densities[density_idx]);

    // NOTE: Should be the same across all formats, so just look at csr
    non_zero_counts[density_idx][0] = params.left.csr.non_zero_count;
    non_zero_counts[density_idx][1] = params.right.csr.non_zero_count;

    printf("left: %u, right: %u\n", non_zero_counts[density_idx][0], non_zero_counts[density_idx][1]);

    f64 density = densities[density_idx];

    for (usize func_idx = 0; func_idx < STATIC_COUNT(test_entries); func_idx++)
    {
      Repetition_Tester *tester = &testers[func_idx][density_idx];
      Operation_Entry *entry = test_entries + func_idx;

      printf("\n--- %.*s @ %.4f density ---\n", STRF(entry->name), density);
      printf("                                                          \r");
      repetition_tester_new_wave(tester, 0, cpu_timer_frequency, seconds_to_try_for_min);

      while (repetition_tester_is_testing(tester))
      {
        entry->function(tester, &params);
      }
    }

    arena_clear(&arena); // Reset any memory taken by params
  }

  // Dump csv
  for (usize func_idx = 0; func_idx < STATIC_COUNT(test_entries); func_idx++)
  {
    Operation_Entry *entry = test_entries + func_idx;

    // FIXME: Holy jank, simplify
    String timestamp = string_create_timestamp(&arena);
    mkdir(string_to_c_string(&arena, timestamp), 0777);

    String join[] = {timestamp, STR("/"), entry->name,  STR(".csv"), };
    String filename = string_join_array(&arena, (String_Array){.v = join, .count = STATIC_COUNT(join)}, STR(""));

    FILE *csv = fopen(string_to_c_string(&arena, filename), "w");

    if (csv)
    {
      LOG_INFO("Dumping csv: %.*s", STRF(filename));
      fprintf(csv, "row_count,col_count,inner_count,left_non_zero_count,right_non_zero_count,density,flops,memops,time\n");

      for (usize density_idx = 0; density_idx < STATIC_COUNT(densities); density_idx++)
      {
        Repetition_Tester *tester = &testers[func_idx][density_idx];
        Repetition_Test_Values v = tester->results.min;
        u64 flops   = v.v[REPTEST_VALUE_FLOP_COUNT];
        u64 memops  = v.v[REPTEST_VALUE_MEMOP_COUNT];
        u64 time    = v.v[REPTEST_VALUE_TIME];
        f64 density = densities[density_idx];

        u32 left_non_zero_count  = non_zero_counts[density_idx][0];
        u32 right_non_zero_count = non_zero_counts[density_idx][1];

        fprintf(csv, "%u,%u,%u,%u,%u,%f,%lu,%lu,%lu\n",
                row_count, col_count, inner_count, left_non_zero_count, right_non_zero_count,
                density, flops, memops, time);
      }
    }
    else
    {
      LOG_ERROR("Unable to open csv file: %.*s", STRF(filename));
    }
  }
}
