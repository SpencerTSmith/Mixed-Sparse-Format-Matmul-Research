#define LOG_TITLE "REPETITION_TESTER"
#define COMMON_IMPLEMENTATION

#include "common.h"
#include "benchmark/benchmark_inc.h"
#include "benchmark/benchmark_inc.c"
#include "formats.h"

#include "taco_bullshit.c"

typedef struct Taco_COO Taco_COO;
struct Taco_COO
{
  // For me.
  String name;
  u64    k;

  // For the taco bullshit.
  int    pos[2]; // {0, nnz}
  int    *crd1;
  int    *crd2;
  double *vals;
  int    dimensions[2];
};

// Rigamarole round trip because c std sucks.
typedef struct COO_Element COO_Element;
struct COO_Element
{
  int    row;
  int    col;
  double val;
};

int kron_compare(const void *a, const void *b)
{
  const COO_Element *element_a = a;
  const COO_Element *element_b = b;

  int result = 0;

  if (element_a->row < element_b->row)
  {
    result = -1;
  }
  else if (element_a->row > element_b->row)
  {
    result = 1;
  }
  else // Same row.
  {
    if (element_a->col < element_b->col)
    {
      result = -1;
    }
    else if (element_a->col > element_b->col)
    {
      result = 1;
    }
    else // Shouldn't happen, but same edge.
    {
      result = 0;
    }
  }

  return result;
}

static
Taco_COO load_kron(Arena *arena, String filename)
{
  Taco_COO result = {0};

  result.name = file_basename(filename);

  String data = read_file_to_arena(arena, filename);

  Stream parser = { data, 0 };

  usize element_cursor = 0;

  for (String line = stream_get_next_line(&parser); string_valid(line); line = stream_get_next_line(&parser))
  {
    // Info lines.
    if (line.v[0] == '#')
    {
      String k_line = STR("# K : ");
      String nnz_line = STR("# Number of edges: ");

      // Get a k line potentially.
      usize potential_k_line_index = string_find_substring(line, 0, k_line);
      if (potential_k_line_index != line.count)
      {
        String substring = string_substring(line, potential_k_line_index + k_line.count, line.count - k_line.count);
        result.k = string_to_u64(substring);

        result.dimensions[0] = pow(2, result.k);
        result.dimensions[1] = result.dimensions[0];
      }

      usize potential_nnz_line_index = string_find_substring(line, 0, nnz_line);
      if (potential_nnz_line_index != line.count)
      {
        String substring = string_substring(line, potential_nnz_line_index + nnz_line.count, line.count - nnz_line.count);
        u64 nnz = string_to_u64(substring);

        result.pos[1] = nnz;

        // Allocate space for elements. Should only happen once.
        result.crd1 = arena_calloc(arena, nnz, int);
        result.crd2 = arena_calloc(arena, nnz, int);
        result.vals = arena_calloc(arena, nnz, double);
      }
    }

    // Non-info lines
    else
    {
      ASSERT(result.crd1 && result.crd2 && result.vals, "Kron file did not contain nnz info before declaring edges.");

      Scratch scratch = scratch_begin(arena);

        String_Array split = string_split(scratch.arena, line, STR(" "));
        ASSERT(split.count == 2, "Kron file edge does not contain 2 numbers.");
        u64 row = string_to_u64(split.v[0]);
        u64 col = string_to_u64(split.v[1]);

        ASSERT(element_cursor < result.pos[1], "More edges in file than declared nnz.");

        result.crd1[element_cursor] = row;
        result.crd2[element_cursor] = col;
        result.vals[element_cursor] = 1.0; // For now.

        element_cursor += 1;

      scratch_close(&scratch);
    }
  }

  // Sort, since these were not generated in correct order.
  Scratch scratch = scratch_begin(arena);

    // Stupid round tripping just to use c std qsort.
    COO_Element *temps = arena_calloc(arena, result.pos[1], COO_Element);
    for (usize i = 0; i < result.pos[1]; i++)
    {
      temps[i] = (COO_Element)
      {
        .row = result.crd1[i],
        .col = result.crd2[i],
        .val = result.vals[i],
      };
    }
    qsort(temps, result.pos[1], sizeof(temps[0]), kron_compare);
    for (usize i = 0; i < result.pos[1]; i++)
    {
      result.crd1[i] = temps[i].row;
      result.crd2[i] = temps[i].col;
      result.vals[i] = temps[i].val;
    }

  scratch_close(&scratch);

  return result;
}

typedef void Operation_Function(Repetition_Tester *tester, taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B);

typedef struct Operation_Entry Operation_Entry;
struct Operation_Entry
{
  Matrix_Format a_format;
  Matrix_Format b_format;
  Operation_Function *function;
};

void CSR_x_CSR(Repetition_Tester *tester, taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B)
{
  repetition_tester_begin_time(tester);
  CSR_x_CSR_compute(C, A, B);
  repetition_tester_close_time(tester);
}

void CSR_x_CSC(Repetition_Tester *tester, taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B)
{
  repetition_tester_begin_time(tester);
  CSR_x_CSC_compute(C, A, B);
  repetition_tester_close_time(tester);
}

void CSR_x_COO(Repetition_Tester *tester, taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B)
{
  repetition_tester_begin_time(tester);
  CSR_x_COO_compute(C, A, B);
  repetition_tester_close_time(tester);
}

void CSC_x_CSR(Repetition_Tester *tester, taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B)
{
  repetition_tester_begin_time(tester);
  CSC_x_CSR_compute(C, A, B);
  repetition_tester_close_time(tester);
}

void CSC_x_COO(Repetition_Tester *tester, taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B)
{
  repetition_tester_begin_time(tester);
  CSC_x_COO_compute(C, A, B);
  repetition_tester_close_time(tester);
}

void COO_x_CSR(Repetition_Tester *tester, taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B)
{
  repetition_tester_begin_time(tester);
  COO_x_CSR_compute(C, A, B);
  repetition_tester_close_time(tester);
}

void COO_x_CSC(Repetition_Tester *tester, taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B)
{
  repetition_tester_begin_time(tester);
  COO_x_CSC_compute(C, A, B);
  repetition_tester_close_time(tester);
}

void COO_x_COO(Repetition_Tester *tester, taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B)
{
  repetition_tester_begin_time(tester);
  COO_x_COO_compute(C, A, B);
  repetition_tester_close_time(tester);
}

Operation_Entry test_entries[] =
{
  {MAT_CSR, MAT_CSR, CSR_x_CSR},
  {MAT_CSR, MAT_CSC, CSR_x_CSC},
  {MAT_CSR, MAT_COO, CSR_x_COO},
  {MAT_CSC, MAT_CSR, CSC_x_CSR},
  {MAT_CSC, MAT_COO, CSC_x_COO},
  {MAT_COO, MAT_CSR, COO_x_CSR},
  {MAT_COO, MAT_CSC, COO_x_CSC},
  {MAT_COO, MAT_COO, COO_x_COO},
};

typedef struct Taco_Mode_Info Taco_Mode_Info;
struct Taco_Mode_Info
{
  int ordering[2];
  taco_mode_t types[2];
};

Taco_Mode_Info taco_mode_info_from_format(Matrix_Format format)
{
  Taco_Mode_Info result = {0};

  switch (format)
  {
    // Default to COO if invalid.
    case MAT_NONE:
    case MAT_COUNT:
    case MAT_DENSE:
    case MAT_COO:
    {
      result.ordering[0] = 0;
      result.ordering[1] = 1;
      result.types[0] = taco_mode_sparse;
      result.types[1] = taco_mode_sparse;
    } break;
    case MAT_CSR:
    {
      result.ordering[0] = 0;
      result.ordering[1] = 1;
      result.types[0] = taco_mode_dense;
      result.types[1] = taco_mode_sparse;
    } break;
    case MAT_CSC:
    {
      result.ordering[0] = 1;
      result.ordering[1] = 0;
      result.types[0] = taco_mode_dense;
      result.types[1] = taco_mode_sparse;
    } break;
  }

  return result;
}

// Great, fucking incompetents.
void free_taco_tensor(taco_tensor_t *tensor)
{
  if (tensor->vals)
  {
    free(tensor->vals);
  }

  for (usize i = 0; i < tensor->order; i++)
  {
    if (tensor->mode_types[i] != taco_mode_dense)
    {
      if (tensor->indices[i][0])
      {
        free(tensor->indices[i][0]);
      }
      if (tensor->indices[i][1])
      {
        free(tensor->indices[i][1]);
      }
    }
  }

  deinit_taco_tensor_t(tensor);
}

int main(int argc, char **argv)
{
  Arena arena = arena_make(.reserve_size = GB(64));

  Args args = parse_args(&arena, argc, argv);

  u32 seconds_to_try_for_min = args_get_integer_value(&args, STR("seconds_to_try_for_min"), 1);

  String out_dir = args_get_string_value(&args, STR("out_dir"), STR("taco_kron_results"));
  String kron_dir = args_get_string_value(&args, STR("kron_folder"), STR("krons/Web-Notredame"));

  String_List krons = folder_children(&arena, kron_dir);

  // Max 30.
  Repetition_Tester testers[30][STATIC_COUNT(test_entries)] = {0};
  u64 k_for_test[30] = {0};

  // ASSERT(STATIC_COUNT(testers) > krons.count, "Too many kroneckers to test in one run.");

  u64 cpu_timer_frequency = estimate_cpu_timer_freq();

  usize kron_index = 0;
  for (String_Node *kron_file = krons.first; kron_file; kron_file = kron_file->link_next)
  {
    // TODO: Make which s we look at configurable.
    if (!string_contains_substring(kron_file->value, STR("s2")))
    {
      continue;
    }

    Scratch scratch = scratch_begin(&arena);

    Taco_COO kron_coo = load_kron(scratch.arena, kron_file->value);

    if (kron_coo.k > 20)
    {
      scratch_close(&scratch);
      continue;
    }

    k_for_test[kron_index] = kron_coo.k;

    for (usize func_idx = 0; func_idx < STATIC_COUNT(test_entries); func_idx++)
    {
      Repetition_Tester *tester = &testers[kron_index][func_idx];

      Operation_Entry *entry = test_entries + func_idx;

      printf("\n--- %.*s, %.*s x %.*s ---\n", STRF(kron_coo.name),
             STRF(matrix_format_string(entry->a_format)),
             STRF(matrix_format_string(entry->b_format)));

      printf("                                                          \r");
      repetition_tester_new_wave(tester, 0, cpu_timer_frequency, seconds_to_try_for_min);


      Taco_Mode_Info a_info = taco_mode_info_from_format(entry->a_format);
      taco_tensor_t *A = init_taco_tensor_t(2, sizeof(double), kron_coo.dimensions, a_info.ordering,
                                            a_info.types);

      Taco_Mode_Info b_info = taco_mode_info_from_format(entry->b_format);
      taco_tensor_t *B = init_taco_tensor_t(2, sizeof(double), kron_coo.dimensions, b_info.ordering,
                                            b_info.types);

      // Always COO for C.
      Taco_Mode_Info c_info = taco_mode_info_from_format(MAT_COO);
      taco_tensor_t *C = init_taco_tensor_t(2, sizeof(double), kron_coo.dimensions, c_info.ordering,
                                            c_info.types);

      if (0) {}
      else if (entry->a_format == MAT_CSR && entry->b_format == MAT_CSR)
      {
        CSR_x_CSR_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSR_x_CSR_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSR_x_CSR_assemble(C, A, B);
      }
      else if (entry->a_format == MAT_CSR && entry->b_format == MAT_CSC)
      {
        // Simply too large for the assembling step as done by taco.
        if (k_for_test[kron_index] > 15)
        {
          continue;
        }

        CSR_x_CSC_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSR_x_CSC_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSR_x_CSC_assemble(C, A, B);
      }
      else if (entry->a_format == MAT_CSR && entry->b_format == MAT_COO)
      {
        if (k_for_test[kron_index] > 15)
        {
          continue;
        }

        CSR_x_COO_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSR_x_COO_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSR_x_COO_assemble(C, A, B);
      }
      else if (entry->a_format == MAT_CSC && entry->b_format == MAT_CSR)
      {
        CSC_x_CSR_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSC_x_CSR_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSC_x_CSR_assemble(C, A, B);
      }
      else if (entry->a_format == MAT_CSC && entry->b_format == MAT_COO)
      {
        CSC_x_COO_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSC_x_COO_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSC_x_COO_assemble(C, A, B);
      }
      else if (entry->a_format == MAT_COO && entry->b_format == MAT_CSR)
      {
        COO_x_CSR_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        COO_x_CSR_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        COO_x_CSR_assemble(C, A, B);
      }
      else if (entry->a_format == MAT_COO && entry->b_format == MAT_CSC)
      {
        if (k_for_test[kron_index] > 15)
        {
          continue;
        }

        COO_x_CSC_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        COO_x_CSC_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        COO_x_CSC_assemble(C, A, B);
      }
      else if (entry->a_format == MAT_COO && entry->b_format == MAT_COO)
      {
        if (k_for_test[kron_index] > 15)
        {
          continue;
        }

        COO_x_COO_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        COO_x_COO_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        COO_x_COO_assemble(C, A, B);
      }

      while (repetition_tester_is_testing(tester))
      {
        entry->function(tester, C, A, B);
      }

      free_taco_tensor(C);
      free_taco_tensor(A);
      free_taco_tensor(B);
    }

    kron_index += 1;

    scratch_close(&scratch);
  }

  String timestamp = string_timestamp(&arena);
  String test_run_info = string_formatted(&arena, "%.*s_%.*s", STRF(file_basename(kron_dir)), STRF(timestamp));
  String test_run_dir = string_formatted(&arena, "%.*s/%.*s", STRF(out_dir), STRF(test_run_info));

  mkdir(string_to_c_string(&arena, out_dir), 0755);
  mkdir(string_to_c_string(&arena, test_run_dir), 0755);

  for (usize func_idx = 0; func_idx < STATIC_COUNT(test_entries); func_idx++)
  {
    for (usize kron_index = 0; kron_index < krons.count; kron_index++)
    {
      Operation_Entry *entry = test_entries + func_idx;

      u64 k = k_for_test[kron_index];
      String filename = string_formatted(&arena, "%.*s/k%lu_%.*s_%.*s.csv", STRF(test_run_dir), k, STRF(matrix_format_string(entry->a_format)),
                                         STRF(matrix_format_string(entry->b_format)));

      FILE *csv = fopen(string_to_c_string(&arena, filename), "w");

      if (csv)
      {
        LOG_INFO("Dumping csv: %.*s", STRF(filename));

        Repetition_Tester *tester = &testers[kron_index][func_idx];
        Repetition_Test_Values v = tester->results.min;
        u64 time  = v.v[REPTEST_VALUE_TIME];
        u64 cache = v.v[REPTEST_VALUE_CACHE_COUNT];
        u64 branch = v.v[REPTEST_VALUE_BRANCH_COUNT];

        fprintf(csv, "k,time,cache,branch\n");
        fprintf(csv, "%lu,%lu,%lu,%lu\n", k, time, cache, branch);
      }
      else
      {
        LOG_ERROR("Unable to open csv file: %.*s", STRF(filename));
      }
    }
  }
}
