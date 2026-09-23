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
  u64    s;

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
Taco_COO load_kron(Arena *arena, String filename, u64 k_min, u64 k_max, u64 sample)
{
  Taco_COO result = {0};

  result.name = file_basename(filename);

  String data = read_file_to_arena(arena, filename);

  Stream parser = { data, 0 };

  usize element_cursor = 0;

  // NOTE: Just leaking loading the file right now if we don't meet this...
  // need to get my thread scratch arena set up asap!
  b32 is_wish_sample = true;
  b32 is_wish_k      = true;

  for (String line = stream_get_next_line(&parser);
       string_valid(line) && is_wish_sample && is_wish_k;
       line = stream_get_next_line(&parser))
  {
    // Info lines.
    if (line.v[0] == '#')
    {
      String k_line = STR("# K : ");
      String s_line = STR("Sample: ");
      String nnz_line = STR("# Number of edges: ");

      // Get a k line potentially.
      usize potential_k_line_index = string_find_substring(line, 0, k_line);
      if (potential_k_line_index != line.count)
      {
        String substring = string_substring(line, potential_k_line_index + k_line.count, line.count - k_line.count);
        result.k = string_to_u64(substring);

        result.dimensions[0] = pow(2, result.k);
        result.dimensions[1] = result.dimensions[0];

        if (result.k < k_min || result.k > k_max)
        {
          is_wish_k = false;
        }
      }

      usize potential_s_line_index = string_find_substring(line, 0, s_line);
      if (potential_s_line_index != line.count)
      {
        String substring = string_substring(line, potential_s_line_index + s_line.count, line.count);
        result.s = string_to_u64(substring);

        if (result.s != sample)
        {
          is_wish_sample = false;
        }
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

typedef int Operation_Function(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B);

typedef struct Operation_Entry Operation_Entry;
struct Operation_Entry
{
  const char    *name;
  Matrix_Format a_format;
  Matrix_Format b_format;
  Operation_Function *function;
};

Operation_Entry test_entries[] =
{
  {"CSR_x_CSR", MAT_CSR, MAT_CSR, CSR_x_CSR_compute},
  {"CSR_x_CSC", MAT_CSR, MAT_CSC, CSR_x_CSC_compute},
  {"CSR_x_COO", MAT_CSR, MAT_COO, CSR_x_COO_compute},
  {"CSC_x_CSR", MAT_CSC, MAT_CSR, CSC_x_CSR_compute},
  {"CSC_x_COO", MAT_CSC, MAT_COO, CSC_x_COO_compute},
  {"COO_x_CSR", MAT_COO, MAT_CSR, COO_x_CSR_compute},
  {"COO_x_CSC", MAT_COO, MAT_CSC, COO_x_CSC_compute},
  {"COO_x_COO", MAT_COO, MAT_COO, COO_x_COO_compute},
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
  String kron_dir = args_get_string_value(&args, STR("kron_dir"), STR("krons/AS-Newman"));
  u64 sample = args_get_integer_value(&args, STR("sample"), 2);
  u64 k_max = args_get_integer_value(&args, STR("k_max"), 20);
  u64 k_min = args_get_integer_value(&args, STR("k_min"), 0);

  String_List krons = folder_children(&arena, kron_dir);

  // Max 30 k.
  Repetition_Series *series = repetition_series_make(30 * STATIC_COUNT(test_entries),
                                                     ((const char *[]){"k", "function"}));

  u64 cpu_timer_frequency = estimate_cpu_timer_freq();

  u64 actual_kron_count = 0;
  for (String_Node *kron_file = krons.first; kron_file; kron_file = kron_file->link_next)
  {
    Scratch scratch = scratch_begin(&arena);

    Taco_COO kron_coo = load_kron(scratch.arena, kron_file->value, k_min, k_max, sample);

    u64 k = kron_coo.k;

    // TODO: check this before parsing and laoding.
    if (k > k_max || k < k_min || kron_coo.s != sample)
    {
      scratch_close(&scratch);
      continue;
    }

    for (usize func_idx = 0; func_idx < STATIC_COUNT(test_entries); func_idx++)
    {
      Operation_Entry *entry = test_entries + func_idx;

      // Simply too large for the assembling step as done by taco for these functions.
      if (k > 15)
      {
        if (entry->a_format == MAT_CSR && entry->b_format == MAT_CSC ||
            entry->a_format == MAT_CSR && entry->b_format == MAT_COO ||
            entry->a_format == MAT_COO && entry->b_format == MAT_CSC ||
            entry->a_format == MAT_COO && entry->b_format == MAT_COO)
        {
          continue;
        }
      }

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

      Repetition_Tester tester =
        repetition_series_new_tester(series, 0,
                                     cpu_timer_frequency,
                                     seconds_to_try_for_min,
                                     "--- %.*s, %s ---",
                                     STRF(kron_coo.name),
                                     entry->name);
      repetition_series_set_field(series, "k", "%lu", k);
      repetition_series_set_field(series, "function", entry->name);

      if (0) {}
      else if (entry->a_format == MAT_CSR && entry->b_format == MAT_CSR)
      {
        CSR_x_CSR_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSR_x_CSR_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSR_x_CSR_assemble(C, A, B);
      }
      else if (entry->a_format == MAT_CSR && entry->b_format == MAT_CSC)
      {
        CSR_x_CSC_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSR_x_CSC_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        CSR_x_CSC_assemble(C, A, B);
      }
      else if (entry->a_format == MAT_CSR && entry->b_format == MAT_COO)
      {
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
        COO_x_CSC_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        COO_x_CSC_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        COO_x_CSC_assemble(C, A, B);
      }
      else if (entry->a_format == MAT_COO && entry->b_format == MAT_COO)
      {
        COO_x_COO_pack_A(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        COO_x_COO_pack_B(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
        COO_x_COO_assemble(C, A, B);
      }

      while (repetition_series_is_testing(series, &tester))
      {
        repetition_tester_begin_time(&tester);
        entry->function(C, A, B);
        repetition_tester_close_time(&tester);
      }

      free_taco_tensor(C);
      free_taco_tensor(A);
      free_taco_tensor(B);
    }

    actual_kron_count += 1;

    scratch_close(&scratch);
  }

  String timestamp = string_timestamp(&arena);
  String filename  = string_formatted(&arena, "%.*s/%.*s_s%lu_%.*s.csv",
                                          STRF(out_dir),
                                          STRF(file_basename(kron_dir)), sample,
                                          STRF(timestamp));

  mkdir(string_to_c_string(&arena, out_dir), 0755);

  repetition_series_save_csv(series, "%.*s", STRF(filename));
}
