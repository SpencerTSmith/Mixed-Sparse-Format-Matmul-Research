// NOTE(spencer): This is all my common stuff
#define LOG_TITLE "REPETITION_TESTER"
#define COMMON_IMPLEMENTATION
#include "common.h"

// NOTE(spencer): I like to do 'unity' builds, so I just include all c files
// into one translation unit, so only this file gets passed to compiler.
#include "benchmark/benchmark_inc.h"
#include "benchmark/benchmark_inc.c"

// NOTE(spencer): Define the interface that all the functions we want to test conform to.
typedef void (*Test_Function)(u8 *buffer, usize buffer_size);

// NOTE(spencer): As we can see this function follows the
// interface of the 'Test_Function' type before.
static
void read_buffer_linearly(u8 *buffer, usize buffer_size)
{
  volatile u8 sink = 0;
  for (usize i = 0; i < buffer_size; i += 1)
  {
    sink = buffer[i];
  }
}

static
void read_buffer_randomly(u8 *buffer, usize buffer_size)
{
  volatile u8 sink = 0;

  usize bytes_left = buffer_size;
  while (bytes_left)
  {
    // Probably time will be mostly dominated by modulo, in addition to cache misses,
    // but just for demo purposes.
    usize random_index = rand() % bytes_left;

    sink = buffer[random_index];

    bytes_left -= 1;
  }
}

// NOTE(spencer): Each entry is just going to be the function and
// then a little string to name it.
typedef struct Function_Entry Function_Entry;
struct Function_Entry
{
  const char    *name;
  Test_Function function;
};
// NOTE(spencer): Ok now we can just make an array of all the functions we want to test.
Function_Entry entries[] =
{
  {"linearly", read_buffer_linearly},
  {"randomly", read_buffer_randomly},
};

int main(int argc, char **argv)
{
  Arena arena = arena_make({.reserve_size = GB(64)});

  // NOTE(spencer): Max 60 on row axis, and a column for every function.
  Repetition_Series *series = repetition_series_make(&arena, 60, STATIC_COUNT(entries));

  // NOTE(spencer): Call this to get an estimate of actual current rdtsc timer freq.
  u64 cpu_timer_frequency = estimate_cpu_timer_freq();

  // NOTE(spencer): How many seconds to keep trying for a new min, will reset once
  // we find a new minimum.
  u32 seconds_to_try_for_min = 5;

  for (usize test_func_idx = 0; test_func_idx < STATIC_COUNT(entries); test_func_idx++)
  {
    Function_Entry *entry = &entries[test_func_idx];

    repetition_series_set_col_label(series, entry->name);

    for (usize buffer_size = MB(1), tester_index = 0;
         buffer_size <= GB(1) && tester_index < series->max_row;
         buffer_size *= 4, tester_index += 1)
    {
      Repetition_Tester tester = repetition_series_new_tester(series, buffer_size, cpu_timer_frequency, seconds_to_try_for_min);

      printf("\n--- %s (%lu MB) ---\n", entry->name, buffer_size / MB(1));

      u8 *buffer = os_allocate(buffer_size, OS_ALLOCATION_COMMIT);

      while (repetition_tester_is_testing(&tester))
      {
        repetition_tester_begin_time(&tester);
        entry->function(buffer, buffer_size);
        repetition_tester_close_time(&tester);

        repetition_tester_count_bytes(&tester, buffer_size);
      }

      os_deallocate(buffer, buffer_size);
    }

  }
}
