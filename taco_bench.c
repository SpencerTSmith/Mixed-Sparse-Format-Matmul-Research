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
  b32    valid;

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

  // NOTE: Assumption about file naming!
  usize k_index = string_find_substring(result.name, 0, STR("k"));
  u64 k = string_to_u64(string_substring(result.name, k_index + 1, result.name.count));
  usize s_index = string_find_substring(result.name, 0, STR("s"));
  u64 s = string_to_u64(string_substring(result.name, s_index + 1, result.name.count));

  if (k >= k_min && k < k_max && s == sample)
  {
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

          if (result.k < k_min || result.k >= k_max)
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
      COO_Element *temps = arena_calloc(scratch.arena, result.pos[1], COO_Element);
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

    result.valid = is_wish_k && is_wish_sample;
  }

  return result;
}

typedef int Compute_Function(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B);
typedef int Pack_Function(taco_tensor_t *A, int* pos, int* crd1, int* crd2, double* vals);
typedef int Assemble_Function(taco_tensor_t *C, taco_tensor_t *A, taco_tensor_t *B);

typedef struct Operation_Entry Operation_Entry;
struct Operation_Entry
{
  const char    *name;
  Matrix_Format a_format;
  Matrix_Format b_format;
  Matrix_Format c_format;

  Compute_Function  *compute;
  Pack_Function     *pack_a;
  Pack_Function     *pack_b;
  Assemble_Function *assemble;
};

#define COMBOS(X)  \
  X(CSR, CSR, COO)
  // X(CSR, CSC, COO) \
  // X(CSR, COO, COO) \
  // X(CSC, CSR, COO) \
  // X(CSC, COO, COO) \
  // X(COO, CSR, COO) \
  // X(COO, CSC, COO) \
  // X(COO, COO, COO) \
  // X(CSR, CSR, CSR) \
  // X(CSC, CSC, CSC) \
  // X(CSR, COO, CSR) \
  // X(COO, COO, CSR)

#define TEST_ENTRY(A, B, C) \
 {#A "_x_" #B "_" #C, MAT_##A, MAT_##B, MAT_##C, A##_x_##B##_##C##_compute, A##_x_##B##_##C##_pack_A, A##_x_##B##_##C##_pack_B, A##_x_##B##_##C##_assemble},

Operation_Entry test_entries[] =
{
  COMBOS(TEST_ENTRY)
};

typedef struct TACO_Memory_Alloc TACO_Memory_Alloc;
struct TACO_Memory_Alloc
{
  void  *pointer;
  usize request;
};

typedef struct TACO_Memory_Log TACO_Memory_Log;
struct TACO_Memory_Log
{
  String function_prefix;
  usize  memory_usage;
  TACO_Memory_Alloc allocs[64]; // Max pool should be sufficient.
};

TACO_Memory_Log taco_mem_logs[STATIC_COUNT(test_entries)] = {0};
usize taco_mem_logs_count = 0;

String chop_function_prefix(String function_name, String chop_suffix)
{
  usize end = string_find_substring(function_name, 0, chop_suffix);
  return string_substring(function_name, 0, end);
}

void log_taco_memory_usage(const char *function_name, size_t line, size_t memory_request, void *returned_pointer, void *previous_pointer)
{
  const char *alloc_type = memory_request == 0 ? "Free" :
                           previous_pointer    ? "Realloc" : "Malloc";
  LOG_INFO("%s in %s:%lu (%lu)", alloc_type, function_name, line, memory_request);

  String function_string = string_from_c_string((char *)function_name);

  TACO_Memory_Log *log = 0;

  // See if we already started a log.
  for (usize log_idx = 0; log_idx < taco_mem_logs_count; log_idx++)
  {
    TACO_Memory_Log *potential_log = taco_mem_logs + log_idx;
    if (string_starts_with(function_string, potential_log->function_prefix))
    {
      log = potential_log;
      break;
    }
  }

  // No existent log yet, add new.
  if (!log)
  {
    ASSERT(taco_mem_logs_count < STATIC_COUNT(taco_mem_logs), "Too many TACO memory logs!");
    log = taco_mem_logs + taco_mem_logs_count;
    taco_mem_logs_count += 1;
    ZERO_STRUCT(log);

    log->function_prefix = function_string;
    log->function_prefix = chop_function_prefix(log->function_prefix, STR("_compute"));
    log->function_prefix = chop_function_prefix(log->function_prefix, STR("_assemble"));
    log->function_prefix = chop_function_prefix(log->function_prefix, STR("_pack_A"));
    log->function_prefix = chop_function_prefix(log->function_prefix, STR("_pack_B"));
    log->function_prefix = chop_function_prefix(log->function_prefix, STR("_pack_C"));
  }

  // Subtract out any previous request in the case of realloc or free, and reset the slot for reuse.
  if (previous_pointer)
  {
    // Find a previous log if we have it.
    for (usize alloc_idx = 0; alloc_idx < STATIC_COUNT(log->allocs); alloc_idx++)
    {
      // Subtract out any previous request.
      if (log->allocs[alloc_idx].pointer == previous_pointer)
      {
        log->memory_usage -= log->allocs[alloc_idx].request;

        // Reset for reuse.
        ZERO_STRUCT(&log->allocs[alloc_idx]);
        break;
      }
    }
  }

  // Asked for memory -> log it.
  if (memory_request)
  {
    // If we the memory request succeeed and we got a pointer to it.
    if (returned_pointer)
    {
      // Find a free allocation log slot.
      TACO_Memory_Alloc *alloc = 0;
      for (usize alloc_idx = 0; alloc_idx < STATIC_COUNT(log->allocs); alloc_idx++)
      {
        if (STRUCT_IS_ZERO(&log->allocs[alloc_idx]))
        {
          alloc = &log->allocs[alloc_idx];
          break;
        }
      }

      if (alloc)
      {
        alloc->pointer = returned_pointer;
        alloc->request = memory_request;
        log->memory_usage += memory_request;
      }
      else
      {
        LOG_ERROR("Unable to find free alloc slot for TACO memory logging: %s,%lu", function_name, line);
      }
    }
    else
    {
      LOG_FATAL("TACO allocation failed: %s,%lu (request = %lu)", 1, function_name, line, memory_request);
    }
  }
}

__attribute__((section(".rodata")))
static TACO_Memory_Log nil_log = {0};

TACO_Memory_Log *memory_log_from_name(const char *name)
{
  String function_string = string_from_c_string((char *)name);

  TACO_Memory_Log *log = 0;

  // See if we already started a log.
  for (usize log_idx = 0; log_idx < taco_mem_logs_count; log_idx++)
  {
    TACO_Memory_Log *potential_log = taco_mem_logs + log_idx;
    if (string_starts_with(function_string, potential_log->function_prefix))
    {
      log = potential_log;
      break;
    }
  }

  // If we don't have a log give out the nil one.
  if (!log)
  {
    log = &nil_log;
  }

  return log;
}

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


int main(int argc, char **argv)
{
  Arena arena = arena_make(.reserve_size = GB(64));

  Args args = parse_args(&arena, argc, argv);

  u32 seconds_to_try_for_min = args_get_integer_value(&args, STR("seconds_to_try_for_min"), 1);

  if (args_has_flag(&args, STR("oscer")))
  {
    // HACK: For debugging purposes.
    setvbuf(stdout, NULL, _IONBF, 0);
  }

  String output_formats = args_get_string_value(&args, STR("output_format"), STR("coo,csr,csc"));
  b32 use_coo_output = string_find_substring(output_formats, 0, STR("coo")) != output_formats.count;
  b32 use_csr_output = string_find_substring(output_formats, 0, STR("csr")) != output_formats.count;
  b32 use_csc_output = string_find_substring(output_formats, 0, STR("csc")) != output_formats.count;

  String out_dir = args_get_string_value(&args, STR("out_dir"), STR("taco_kron_results"));
  String kron_dir = args_get_string_value(&args, STR("kron_dir"), STR("krons/AS-Newman"));
  u64 sample = args_get_integer_value(&args, STR("sample"), 2);
  u64 k_max = args_get_integer_value(&args, STR("k_max"), 20);
  u64 k_min = args_get_integer_value(&args, STR("k_min"), 0);

  String_List krons = folder_children(&arena, kron_dir);

  // Max 30 k.
  Repetition_Series *series = repetition_series_make(30 * STATIC_COUNT(test_entries),
                                                     ((const char *[]){"k", "function", "alloc"}));

  u64 cpu_timer_frequency = estimate_cpu_timer_freq();

  for (String_Node *kron_file = krons.first; kron_file; kron_file = kron_file->link_next)
  {
    Scratch scratch = scratch_begin(&arena);

    // LOG_INFO("Loading kron: %.*s.", STRF(kron_file->value));
    Taco_COO kron_coo = load_kron(scratch.arena, kron_file->value, k_min, k_max, sample);
    if (!kron_coo.valid)
    {
      // LOG_INFO("Kron determined invalid: %.*s.", STRF(kron_file->value));
      continue;
    }
    LOG_INFO("Loaded, k=%lu s=%lu nnz=%d.", kron_coo.k, kron_coo.s, kron_coo.pos[1]);

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

      if ((entry->c_format == MAT_COO && !use_coo_output)
      ||  (entry->c_format == MAT_CSR && !use_csr_output)
      ||  (entry->c_format == MAT_COO && k > 22))
      {
        continue;
      }

      Taco_Mode_Info a_info = taco_mode_info_from_format(entry->a_format);
      taco_tensor_t *A = init_taco_tensor_t(2, sizeof(double), kron_coo.dimensions, a_info.ordering,
                                            a_info.types);

      Taco_Mode_Info b_info = taco_mode_info_from_format(entry->b_format);
      taco_tensor_t *B = init_taco_tensor_t(2, sizeof(double), kron_coo.dimensions, b_info.ordering,
                                            b_info.types);

      Taco_Mode_Info c_info = taco_mode_info_from_format(entry->c_format);
      taco_tensor_t *C = init_taco_tensor_t(2, sizeof(double), kron_coo.dimensions, c_info.ordering,
                                            c_info.types);

      if (entry->c_format == MAT_COO && use_coo_output)
      {
        // Simply too large for these combos
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
      }
      else if (entry->c_format == MAT_CSR && use_csr_output)
      {
        if (k > 18)
        {
          if (entry->a_format == MAT_CSR && entry->b_format == MAT_CSC ||
              entry->a_format == MAT_CSR && entry->b_format == MAT_COO ||
              entry->a_format == MAT_COO && entry->b_format == MAT_CSR)
          {
            continue;
          }
        }
      }

      entry->pack_a(A, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
      entry->pack_b(B, kron_coo.pos, kron_coo.crd1, kron_coo.crd2, kron_coo.vals);
      entry->assemble(C, A, B);

      // Should have our memory log now after running our pack and assemble funcs.
      TACO_Memory_Log *memory_log = memory_log_from_name(entry->name);

      Repetition_Tester tester =
        repetition_series_new_tester(series, 0,
                                     cpu_timer_frequency,
                                     seconds_to_try_for_min,
                                     "--- %.*s, %s (%d C nnz) ---",
                                     STRF(kron_coo.name),
                                     entry->name, C->vals_size);


      repetition_series_set_field(series, "k", "%lu", k);
      repetition_series_set_field(series, "function", entry->name);
      repetition_series_set_field(series, "alloc", "%lu", memory_log->memory_usage);

      // Zero out any logs now.
      ZERO_SIZE(memory_log->allocs, sizeof(memory_log->allocs));

      while (repetition_series_is_testing(series, &tester))
      {
        repetition_tester_begin_time(&tester);
        entry->compute(C, A, B);
        repetition_tester_close_time(&tester);
      }

      free_taco_tensor(C);
      free_taco_tensor(A);
      free_taco_tensor(B);
    }

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
