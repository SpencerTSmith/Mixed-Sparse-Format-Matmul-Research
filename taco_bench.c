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

typedef struct Taco_Mode_Info Taco_Mode_Info;
struct Taco_Mode_Info
{
  // Max 4 dims.
  int ordering[4];
  taco_mode_t types[4];
};

typedef struct Blocked_COO Blocked_COO;
struct Blocked_COO
{
  int    pos[2];
  int    *crd1;
  int    *crd2;
  int    *crd3;
  int    *crd4;
  double *vals;
  int    dimensions[4]; // {grid_size, grid_size, block_size, block_size}
};

// Rigamarole round trip because c std sucks.
typedef struct COO_Element COO_Element;
struct COO_Element
{
  int    row;
  int    col;
  double val;
};

typedef struct Block_Element Block_Element;
struct Block_Element
{
  u64    key; // Lexicographic thingy
  double val;
};

// Great, fucking incompetents.
void free_taco_tensor(taco_tensor_t *tensor)
{
  if (tensor->vals && tensor->vals != (uint8_t *)fake_buffer)
  {
    free(tensor->vals);
  }

  for (size_t i = 0; i < tensor->order; i++)
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

int block_element_compare(const void *a, const void *b)
{
  u64 x = ((const Block_Element *)a)->key;
  u64 y = ((const Block_Element *)b)->key;
  return (x > y) - (x < y);
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

static
Blocked_COO blocked_coo_from_coo(Arena *arena, Taco_COO *src, usize block_size)
{
  Blocked_COO out = {0};
  int N = src->dimensions[0]; // NOTE: Square matrices!
  int grid_size = N / block_size;
  int nnz = src->pos[1];

  out.pos[1]        = nnz;
  out.dimensions[0] = grid_size;
  out.dimensions[1] = grid_size;
  out.dimensions[2] = block_size;
  out.dimensions[3] = block_size;
  out.crd1 = arena_calloc(arena, nnz, int);
  out.crd2 = arena_calloc(arena, nnz, int);
  out.crd3 = arena_calloc(arena, nnz, int);
  out.crd4 = arena_calloc(arena, nnz, int);
  out.vals = arena_calloc(arena, nnz, double);

  // NOTE: Claude helped me with pointing out you can do this cool lexicographic key thing.
  Scratch scratch = scratch_begin(arena);
    Block_Element *e = arena_calloc(scratch.arena, nnz, Block_Element);
    for (usize i = 0; i < nnz; i++)
    {
      u64 r = src->crd1[i], c = src->crd2[i];
      e[i].key = (((r / block_size) * grid_size + (c / block_size)) * block_size + (r % block_size)) * block_size + (c % block_size);
      e[i].val = src->vals[i];
    }
    qsort(e, nnz, sizeof(e[0]), block_element_compare);

    for (usize i = 0; i < nnz; i++)
    {
      u64 key = e[i].key;
      out.crd4[i] = key % block_size;
      key /= block_size;
      out.crd3[i] = key % block_size;
      key /= block_size;
      out.crd2[i] = key % grid_size;
      key /= grid_size;
      out.crd1[i] = key;
      out.vals[i] = e[i].val;
    }
  scratch_close(&scratch);

  return out;
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
  X(CSR, CSR, COO) \
  X(CSR, CSC, COO) \
  X(CSR, COO, COO) \
  X(CSC, CSR, COO) \
  X(CSC, COO, COO) \
  X(COO, CSR, COO) \
  X(COO, CSC, COO) \
  X(COO, COO, COO) \
  X(CSR, CSR, CSR) \
  X(CSC, CSC, CSC)

#define TEST_ENTRY(A, B, C) \
 {#A "_x_" #B "_" #C, MAT_##A, MAT_##B, MAT_##C, A##_x_##B##_##C##_compute, A##_x_##B##_##C##_pack_A, A##_x_##B##_##C##_pack_B, A##_x_##B##_##C##_assemble},

Operation_Entry test_entries[] =
{
  COMBOS(TEST_ENTRY)
};

// NOTE: Not x-macroing this since its just a couple.
typedef int Pack_Function_4D(taco_tensor_t *T, int *pos, int *c1, int *c2, int *c3, int *c4, double *vals);

typedef struct Blocking_Entry Blocking_Entry;
struct Blocking_Entry
{
  const char    *name;
  Taco_Mode_Info a_mode;
  Taco_Mode_Info b_mode;
  Taco_Mode_Info c_mode;

  Compute_Function  *compute;
  Pack_Function_4D  *pack_a;
  Pack_Function_4D  *pack_b;
  Assemble_Function *assemble;
};

Blocking_Entry blocking_entries[] =
{
  {
    .name     = "Blocked_CSR_x_CSR",
    .a_mode   = {.ordering = {0,1,2,3}, .types = {taco_mode_dense, taco_mode_dense, taco_mode_dense, taco_mode_sparse} },
    .b_mode   = {.ordering = {0,1,2,3}, .types = {taco_mode_dense, taco_mode_dense, taco_mode_dense, taco_mode_sparse} },
    .c_mode   = {.ordering = {0,1,2,3}, .types = {taco_mode_sparse, taco_mode_sparse, taco_mode_sparse, taco_mode_sparse} }, // Always blocked coo
    .compute  = blocked_CSR_x_CSR_compute,
    .pack_a   = blocked_CSR_x_CSR_pack_A,
    .pack_b   = blocked_CSR_x_CSR_pack_B,
    .assemble = blocked_CSR_x_CSR_assemble,
  },
  {
    .name     = "Blocked_CSC_x_CSR",
    .a_mode   = {.ordering = {0,1,3,2}, .types = {taco_mode_dense, taco_mode_dense, taco_mode_dense, taco_mode_sparse} },
    .b_mode   = {.ordering = {0,1,2,3}, .types = {taco_mode_dense, taco_mode_dense, taco_mode_dense, taco_mode_sparse} },
    .c_mode   = {.ordering = {0,1,2,3}, .types = {taco_mode_sparse, taco_mode_sparse, taco_mode_sparse, taco_mode_sparse} }, // Always blocked coo
    .compute  = blocked_CSC_x_CSR_compute,
    .pack_a   = blocked_CSC_x_CSR_pack_A,
    .pack_b   = blocked_CSC_x_CSR_pack_B,
    .assemble = blocked_CSC_x_CSR_assemble,
  },
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
  TACO_Memory_Alloc allocs[512]; // Max pool should be sufficient.
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
  // LOG_INFO("%s in %s:%lu (%lu)", alloc_type, function_name, line, memory_request);

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

  String out_dir = args_get_string_value(&args, STR("out_dir"), STR("taco_kron_results"));
  String kron_dir = args_get_string_value(&args, STR("kron_dir"), STR("krons/AS-Newman"));

  u64 sample = args_get_integer_value(&args, STR("s"), 2);
  u64 k_max = args_get_integer_value(&args, STR("k_max"), 20);
  u64 k_min = args_get_integer_value(&args, STR("k_min"), 0);

  b32 blocking_mode = args_has_flag(&args, STR("blocking"));

  String node_name = args_get_string_value(&args, STR("node_name"), STR(""));

  String output_formats = args_get_string_value(&args, STR("output_format"), STR("coo"));
  b32 use_coo_output = string_find_substring(output_formats, 0, STR("coo")) != output_formats.count;
  b32 use_csr_output = string_find_substring(output_formats, 0, STR("csr")) != output_formats.count;
  b32 use_csc_output = string_find_substring(output_formats, 0, STR("csc")) != output_formats.count;

  String timestamp = string_timestamp(&arena);
  String out_filename  = string_formatted(&arena, "%.*s/%.*s_s%lu_%s_%.*s_%.*s.csv",
                                          STRF(out_dir),
                                          STRF(file_basename(kron_dir)), sample, blocking_mode ? "blocking" : "",
                                          STRF(timestamp), STRF(node_name));

  mkdir(string_to_c_string(&arena, out_dir), 0755);

  String_List krons = folder_children(&arena, kron_dir);

  Repetition_Series *series = repetition_series_make(128 * STATIC_COUNT(test_entries),
                                                     ((const char *[]){"k", "function", "alloc", "kron_nnz", "block_size", "grid_size"}));

  u64 cpu_timer_frequency = estimate_cpu_timer_freq();

  for (String_Node *kron_file = krons.first; kron_file; kron_file = kron_file->link_next)
  {
    Scratch scratch = scratch_begin(&arena);

    Taco_COO kron_coo = load_kron(scratch.arena, kron_file->value, k_min, k_max, sample);
    if (!kron_coo.valid)
    {
      scratch_close(&scratch);
      continue;
    }
    u64 k = kron_coo.k;
    u64 nnz = kron_coo.pos[1];
    LOG_INFO("Loaded, k=%lu s=%lu nnz=%d.", k, kron_coo.s, nnz);


    if (k > k_max || k < k_min || kron_coo.s != sample)
    {
      scratch_close(&scratch);
      continue;
    }

    // No blocking? -> do the normal reptest loop.
    if (!blocking_mode)
    {
      for (usize func_idx = 0; func_idx < STATIC_COUNT(test_entries); func_idx++)
      {
        Operation_Entry *entry = test_entries + func_idx;

        if ((entry->c_format == MAT_COO && !use_coo_output)
            ||  (entry->c_format == MAT_CSR && !use_csr_output)
            ||  (entry->c_format == MAT_CSC && !use_csc_output))
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
                                       "--- %.*s %.*s, %s (%d C nnz) ---",
                                       STRF(file_basename(kron_dir)),
                                       STRF(kron_coo.name),
                                       entry->name, C->vals_size);


        repetition_series_set_field(series, "k", "%lu", k);
        repetition_series_set_field(series, "function", entry->name);
        repetition_series_set_field(series, "alloc", "%lu", memory_log->memory_usage);
        repetition_series_set_field(series, "kron_nnz", "%lu", nnz);

        // Zero out any logs now.
        if (memory_log != &nil_log)
        {
          ZERO_SIZE(memory_log->allocs, sizeof(memory_log->allocs));
        }

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
    }

    // Blocking mode will do one more loop nesting level.
    else
    {
      // Max 32x32 blocks
      for (usize grid_size = 1; grid_size <= 32; grid_size *= 2)
      {
        usize block_size = kron_coo.dimensions[0] / grid_size;

        // If grid will actually fit.
        if (block_size)
        {
          Scratch scratch2 = scratch_begin(scratch.arena);

          Blocked_COO blocked_coo = blocked_coo_from_coo(scratch2.arena, &kron_coo, block_size);

          for (usize func_idx = 0; func_idx < STATIC_COUNT(blocking_entries); func_idx++)
          {
            Blocking_Entry *entry = blocking_entries + func_idx;

            Taco_Mode_Info a_info = entry->a_mode;
            taco_tensor_t *A = init_taco_tensor_t(4, sizeof(double), blocked_coo.dimensions, a_info.ordering,
                                                  a_info.types);

            Taco_Mode_Info b_info = entry->b_mode;
            taco_tensor_t *B = init_taco_tensor_t(4, sizeof(double), blocked_coo.dimensions, b_info.ordering,
                                                  b_info.types);

            Taco_Mode_Info c_info = entry->c_mode;
            taco_tensor_t *C = init_taco_tensor_t(4, sizeof(double), blocked_coo.dimensions, c_info.ordering,
                                                  c_info.types);
            entry->pack_a(A, blocked_coo.pos, blocked_coo.crd1, blocked_coo.crd2, blocked_coo.crd3, blocked_coo.crd4, blocked_coo.vals);
            entry->pack_b(B, blocked_coo.pos, blocked_coo.crd1, blocked_coo.crd2, blocked_coo.crd3, blocked_coo.crd4, blocked_coo.vals);
            entry->assemble(C, A, B);

            Repetition_Tester tester =
              repetition_series_new_tester(series, 0,
                                          cpu_timer_frequency,
                                          seconds_to_try_for_min,
                                          "--- %.*s %.*s, %s (%lu x %lu Block Size) ---",
                                          STRF(file_basename(kron_dir)),
                                          STRF(kron_coo.name),
                                          entry->name, block_size, block_size);

            // NOTE: No allocation logging, too many allocations going on due to blocking... and don't have enough time to
            // fix it.
            // TACO_Memory_Log *memory_log = memory_log_from_name(entry->name);

            repetition_series_set_field(series, "k", "%lu", k);
            repetition_series_set_field(series, "function", entry->name);
            repetition_series_set_field(series, "kron_nnz", "%lu", nnz);
            // repetition_series_set_field(series, "alloc", "%lu", memory_log->memory_usage);
            repetition_series_set_field(series, "grid_size", "%lu", grid_size);
            repetition_series_set_field(series, "block_size", "%lu", block_size);

            // if (memory_log != &nil_log)
            // {
            //   ZERO_SIZE(memory_log->allocs, sizeof(memory_log->allocs));
            // }

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

          scratch_close(&scratch2);
        }
      }
    }

    scratch_close(&scratch);

    // Checkpoint write out results.
    repetition_series_save_csv(series, "%.*s", STRF(out_filename));
  }

  LOG_INFO("Completed reptest on kron dir: %.*s", STRF(kron_dir));
}
