#define LOG_TITLE "REPETITION_TESTER"
#define COMMON_IMPLEMENTATION


#include "common.h"
#include "formats.h"
#include "formats.c"
#include "benchmark/benchmark_inc.h"
#include "benchmark/benchmark_inc.c"

#ifdef OBSERVE_FLOPS
#define FMADD(dst, a, b) dst += (a * b); repetition_tester_count_flops(tester, 2)
#endif // OBSERVE_FLOPS

#ifdef OBSERVE_MEMOPS
#define LOAD(src)       src;                          \
  repetition_tester_count_memops(tester, 1);          \
  repetition_tester_count_bytes(tester, sizeof(src))
#define STORE(dst, src) dst = src;                    \
  repetition_tester_count_memops(tester, 1);          \
  repetition_tester_count_bytes(tester, sizeof(dst))
#endif // OBSERVE_MEMOPS

typedef struct Operation_Parameters Operation_Parameters;
struct Operation_Parameters
{
  Matrix_Reps  left;
  Matrix_Reps  right;
  Dense_Matrix output;
};

// extern void read256_asm(u64 count, u8 *data);
// extern void fmadd_asm(u64 count);
//
// static
// void roofline_bandwidth(Repetition_Tester *tester)
// {
//   static u8 bandwidth_buffer[GB(1)] = {0};
//
//   u64 byte_count = STATIC_COUNT(bandwidth_buffer);
//
//   repetition_tester_begin_time(tester);
//
//   read256_asm(byte_count, bandwidth_buffer);
//
//   repetition_tester_close_time(tester);
//
//   repetition_tester_count_bytes(tester, byte_count);
// }
//
// static
// void roofline_flops(Repetition_Tester *tester)
// {
//   u64 flop_count = GB(4);
//
//   repetition_tester_begin_time(tester);
//
//   fmadd_asm(flop_count);
//
//   repetition_tester_close_time(tester);
//
//   repetition_tester_count_flops(tester, flop_count);
// }

static
void matmul_dense_dense(Repetition_Tester *tester, Operation_Parameters *params)
{
  Dense_Matrix left   = params->left.dense;
  Dense_Matrix right  = params->right.dense;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  dense_x_dense_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_dense_csr(Repetition_Tester *tester, Operation_Parameters *params)
{
  Dense_Matrix left   = params->left.dense;
  CSR_Matrix right    = params->right.csr;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  dense_x_csr_impl;
  repetition_tester_close_time(tester);
}

static
void matmul_dense_csc(Repetition_Tester *tester, Operation_Parameters *params)
{
  Dense_Matrix left   = params->left.dense;
  CSC_Matrix right    = params->right.csc;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  dense_x_csc_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_dense_coo(Repetition_Tester *tester, Operation_Parameters *params)
{
  Dense_Matrix left   = params->left.dense;
  COO_Matrix right    = params->right.coo;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  dense_x_coo_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_csr_dense(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSR_Matrix left     = params->left.csr;
  Dense_Matrix right  = params->right.dense;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  csr_x_dense_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_csr_csr(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSR_Matrix left  = params->left.csr;
  CSR_Matrix right = params->right.csr;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

#define PARALLEL_FOR
  csr_x_csr_impl;
#undef PARALLEL_FOR

  repetition_tester_close_time(tester);
}

static
void matmul_csr_csc(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSR_Matrix left  = params->left.csr;
  CSC_Matrix right = params->right.csc;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  csr_x_csc_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_csr_coo(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSR_Matrix left  = params->left.csr;
  COO_Matrix right = params->right.coo;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  csr_x_coo_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_csc_dense(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSC_Matrix left     = params->left.csc;
  Dense_Matrix right  = params->right.dense;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  csc_x_dense_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_csc_csr(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSC_Matrix left  = params->left.csc;
  CSR_Matrix right = params->right.csr;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  csc_x_csr_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_csc_csc(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSC_Matrix left  = params->left.csc;
  CSC_Matrix right = params->right.csc;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  csc_x_csc_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_csc_coo(Repetition_Tester *tester, Operation_Parameters *params)
{
  CSC_Matrix left  = params->left.csc;
  COO_Matrix right = params->right.coo;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  csc_x_coo_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_coo_dense(Repetition_Tester *tester, Operation_Parameters *params)
{
  COO_Matrix left    = params->left.coo;
  Dense_Matrix right = params->right.dense;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  coo_x_dense_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_coo_csr(Repetition_Tester *tester, Operation_Parameters *params)
{
  COO_Matrix left  = params->left.coo;
  CSR_Matrix right = params->right.csr;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  coo_x_csr_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_coo_csc(Repetition_Tester *tester, Operation_Parameters *params)
{
  COO_Matrix left  = params->left.coo;
  CSC_Matrix right = params->right.csc;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  coo_x_csc_impl;

  repetition_tester_close_time(tester);
}

static
void matmul_coo_coo(Repetition_Tester *tester, Operation_Parameters *params)
{
  COO_Matrix left  = params->left.coo;
  COO_Matrix right = params->right.coo;
  Dense_Matrix output = params->output;

  repetition_tester_begin_time(tester);

  coo_x_coo_impl;

  repetition_tester_close_time(tester);
}

Operation_Entry test_entries[] =
{
  // {STR("dense_X_dense"), matmul_dense_dense},
  {STR("dense_X_csr"),   matmul_dense_csr},
  {STR("dense_X_csc"),   matmul_dense_csc},
  {STR("dense_X_coo"),   matmul_dense_coo},
  {STR("csr_X_dense"),   matmul_csr_dense},
  {STR("csr_X_csr"),     matmul_csr_csr},
  {STR("csr_X_csc"),     matmul_csr_csc},
  {STR("csr_X_coo"),     matmul_csr_coo},
  {STR("csc_X_dense"),   matmul_csc_dense},
  {STR("csc_X_csr"),     matmul_csc_csr},
  {STR("csc_X_csc"),     matmul_csc_csc},
  {STR("csc_X_coo"),     matmul_csc_coo},
  {STR("coo_X_dense"),   matmul_coo_dense},
  {STR("coo_X_csr"),     matmul_coo_csr},
  {STR("coo_X_csc"),     matmul_coo_csc},
  {STR("coo_X_coo"),     matmul_coo_coo},
};

#include <math.h>

static
b32 epsilon_equal(f64 a, f64 b)
{
  f64 epsilon = 0.00001;

  return fabs(a - b) <= epsilon;
}

Operation_Parameters init_params(Arena *arena, u32 row_count, u32 col_count, u32 inner_count, f64 left_density, f64 right_density)
{
  Dense_Matrix left_dense  = make_random_dense_matrix(arena, row_count, inner_count, left_density);
  Dense_Matrix right_dense = make_random_dense_matrix(arena, inner_count, col_count, right_density);
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
    .left.coo = coo_from_dense(arena, &left_dense),

    .right.dense = right_dense,
    .right.csr = csr_from_dense(arena, &right_dense),
    .right.csc = csc_from_dense(arena, &right_dense),
    .right.coo = coo_from_dense(arena, &right_dense),

    .output = output,
  };

  return params;
}

int main(int argc, char **argv)
{
  Arena arena = arena_make(.reserve_size = GB(64));

  Args args = parse_args(&arena, argc, argv);
  b32 verify = args_has_flag(&args, STR("verify"));

  u32 seconds_to_try_for_min = args_get_integer_value(&args, STR("seconds_to_try_for_min"), 1);

  u32 row_count   = args_get_integer_value(&args, STR("row_count"), 512);
  u32 col_count   = args_get_integer_value(&args, STR("col_count"), 512);
  u32 inner_count = args_get_integer_value(&args, STR("inner_count"), 512);

  f64 fixed_density = args_get_f64_value(&args, STR("fixed_density"), 1.0);

  String sweep_string = args_get_string_value(&args, STR("sweep"), STR("both"));

  String out_dir = args_get_string_value(&args, STR("out_dir"), STR("data"));

  b32 sweep_left  = false;
  b32 sweep_right = false;
  if (string_match(sweep_string, STR("both")))
  {
    sweep_left  = true;
    sweep_right = true;
  }
  else if (string_match(sweep_string, STR("left")))
  {
    sweep_left  = true;
  }
  else if (string_match(sweep_string, STR("right")))
  {
    sweep_right = true;
  }

  u64 cpu_timer_frequency = estimate_cpu_timer_freq();

  if (verify)
  {
    // Arbitrary sparsity to check
    Operation_Parameters params = init_params(&arena, row_count, col_count, inner_count, 0.4, 0.4);

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

  f64 densities[] =
  {
    0.1, 0.3, 0.5, 0.7, 0.9
  };

  Repetition_Tester testers[STATIC_COUNT(test_entries)][STATIC_COUNT(densities)] = {0};

  u32 non_zero_counts[STATIC_COUNT(densities)][2] = {0};

  for (usize density_idx = 0; density_idx < STATIC_COUNT(densities); density_idx++)
  {
    f64 left_density  = sweep_left  ? densities[density_idx] : fixed_density;
    f64 right_density = sweep_right ? densities[density_idx] : fixed_density;

    // FIXME: So SLOW! But don't know of a better way to test a bunch of densities of different
    // matrix sizes
    Operation_Parameters params = init_params(&arena,
                                              row_count, col_count, inner_count,
                                              left_density, right_density);

    // NOTE: Should be the same across all formats, so just look at csr
    non_zero_counts[density_idx][0] = params.left.csr.non_zero_count;
    non_zero_counts[density_idx][1] = params.right.csr.non_zero_count;

    for (usize func_idx = 0; func_idx < STATIC_COUNT(test_entries); func_idx++)
    {
      Repetition_Tester *tester = &testers[func_idx][density_idx];
      Operation_Entry *entry = test_entries + func_idx;

      printf("\n--- %.*s @ %.4f X %.4f density ---\n", STRF(entry->name), left_density, right_density);
      printf("                                                          \r");
      repetition_tester_new_wave(tester, 0, cpu_timer_frequency, seconds_to_try_for_min);

      while (repetition_tester_is_testing(tester))
      {
        entry->function(tester, &params);
      }
    }

    arena_clear(&arena); // Reset any memory taken by params
  }

  mkdir(string_to_c_string(&arena, out_dir), 0755);
  String timestamp = string_timestamp(&arena);
  String test_run_info = string_formatted(&arena, "%.*s_sweep_%.*s_fixed_%.2f",
                                          STRF(timestamp),
                                          STRF(sweep_string), fixed_density);
  String dir = string_formatted(&arena, "%.*s/%.*s", STRF(out_dir), STRF(test_run_info));
  mkdir(string_to_c_string(&arena, dir), 0755);

  // Roofline
  // {
  //   String filename = string_formatted(&arena, "%.*s/%s", STRF(dir), "roofline.csv");
  //
  //   FILE *roofline_dump = fopen(string_to_c_string(&arena, filename), "w");
  //   {
  //     Repetition_Tester bandwidth_tester = {0};
  //     repetition_tester_new_wave(&bandwidth_tester, 0, cpu_timer_frequency, seconds_to_try_for_min);
  //
  //     printf("\n--- Roofline Bandwidth ---\n");
  //     printf("                                                          \r");
  //     while (repetition_tester_is_testing(&bandwidth_tester))
  //     {
  //       roofline_bandwidth(&bandwidth_tester);
  //     }
  //     Repetition_Test_Values v = bandwidth_tester.results.min;
  //     u64 time    = v.v[REPTEST_VALUE_TIME];
  //     u64 bytes   = v.v[REPTEST_VALUE_BYTE_COUNT];
  //
  //     fprintf(roofline_dump, "%f,", (f64)bytes/time);
  //   }
  //
  //   {
  //     Repetition_Tester flop_tester = {0};
  //     repetition_tester_new_wave(&flop_tester, 0, cpu_timer_frequency, seconds_to_try_for_min);
  //
  //     printf("\n--- Roofline flops/s ---\n");
  //     printf("                                                          \r");
  //     while (repetition_tester_is_testing(&flop_tester))
  //     {
  //       roofline_flops(&flop_tester);
  //     }
  //
  //     Repetition_Test_Values v = flop_tester.results.min;
  //     u64 time    = v.v[REPTEST_VALUE_TIME];
  //     u64 flops   = v.v[REPTEST_VALUE_FLOP_COUNT];
  //
  //     fprintf(roofline_dump, "%f\n", (f64)flops/time);
  //   }
  // }

  // Dump csv
  for (usize func_idx = 0; func_idx < STATIC_COUNT(test_entries); func_idx++)
  {
    Operation_Entry *entry = test_entries + func_idx;

    String filename = string_formatted(&arena, "%.*s/%.*s.csv", STRF(dir), STRF(entry->name));

    FILE *csv = fopen(string_to_c_string(&arena, filename), "w");

    if (csv)
    {
      LOG_INFO("Dumping csv: %.*s", STRF(filename));
      fprintf(csv, "row_count,col_count,inner_count,left_non_zero_count,right_non_zero_count,density,flops,memops,time,bytes,cache\n");

      for (usize density_idx = 0; density_idx < STATIC_COUNT(densities); density_idx++)
      {
        Repetition_Tester *tester = &testers[func_idx][density_idx];
        Repetition_Test_Values v = tester->results.min;
        u64 flops   = v.v[REPTEST_VALUE_FLOP_COUNT];
        u64 memops  = v.v[REPTEST_VALUE_MEMOP_COUNT];
        u64 time    = v.v[REPTEST_VALUE_TIME];
        u64 bytes   = v.v[REPTEST_VALUE_BYTE_COUNT];
        u64 cache   = v.v[REPTEST_VALUE_CACHE_COUNT];
        f64 density = densities[density_idx];

        u32 left_non_zero_count  = non_zero_counts[density_idx][0];
        u32 right_non_zero_count = non_zero_counts[density_idx][1];

        fprintf(csv, "%u,%u,%u,%u,%u,%f,%lu,%lu,%lu,%lu,%lu\n",
                row_count, col_count, inner_count, left_non_zero_count, right_non_zero_count,
                density, flops, memops, time, bytes, cache);
      }
    }
    else
    {
      LOG_ERROR("Unable to open csv file: %.*s", STRF(filename));
    }
  }
}
