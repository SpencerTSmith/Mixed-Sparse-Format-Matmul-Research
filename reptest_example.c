// NOTE: This is all my common stuff
#define LOG_TITLE "REPETITION_TESTER"
#define COMMON_IMPLEMENTATION
#include "common.h"

// NOTE: This whole interface borrows heavily from Casey Muratori's computer enhance stuff,
// so if you want to see a slightly different (perhaps better?) spin on this type of interface,
// check his out.

#include "benchmark/benchmark_inc.h"
// NOTE: I like to do 'unity' builds, so I just #include all c files
// into one translation unit, so only this file gets passed to compiler, not every c file.
#include "benchmark/benchmark_inc.c"

// NOTE: Define the interface that all the functions we want to test conform to.
typedef void (*Test_Function)(u8 *buffer, usize buffer_size);

// NOTE: As we can see this function follows the
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

// NOTE: Each entry is just going to be the function and
// then a little string to name it.
typedef struct Function_Entry Function_Entry;
struct Function_Entry
{
  const char    *name;
  Test_Function function;
};
// NOTE: Ok now we can just make an array of all the functions we want to test.
Function_Entry entries[] =
{
  {"linearly", read_buffer_linearly},
  {"randomly", read_buffer_randomly},
};

int main(int argc, char **argv)
{
  Arena arena = arena_make(.reserve_size = GB(64));

  // NOTE: Call this to get an estimate of actual current rdtsc timer freq.
  u64 cpu_timer_frequency = estimate_cpu_timer_freq();

  // NOTE: How many seconds to keep trying for a new min, will reset once
  // we find a new minimum.
  u32 seconds_to_try_for_min = 1;

  const char *test[] = {"buffer_size", "function"};

  Repetition_Series *series = __repetition_series_make(60 * STATIC_COUNT(entries), (const char *[]){"buffer_size", "function"}, 2);

  // NOTE: The repetition series API assumes you iterate colum by column, that is,
  // it assumes you test all functions at a given size/input/etc before moving on to the
  // next size/input/etc. I've found this is usually what I want to do anyways,
  // if I'm loading a big matrix from disk. In that case I also usually prefault
  // (OS_ALLOCATION_PREFAULT flag passed to os_allocate, if using my COMMON lib)
  // any memory needed by the input so that it's fair.
  for (usize buffer_size = MB(1); buffer_size <= MB(4); buffer_size *= 4)
  {
    for (usize test_func_idx = 0; test_func_idx < STATIC_COUNT(entries); test_func_idx++)
    {
      // NOTE: Allocating and deallocating every time to make it fair page-fault wise.
      u8 *buffer = os_allocate(buffer_size, OS_ALLOCATION_COMMIT);

      Function_Entry *entry = &entries[test_func_idx];

      Repetition_Tester tester = repetition_series_new_tester(series, buffer_size,
                                                              cpu_timer_frequency,
                                                              seconds_to_try_for_min,
                                                              "--- %s @ %lu MB ---", entry->name,
                                                              buffer_size / MB(1));

      // Possible api:
      repetition_series_set_field(series, "buffer_size", "%lu", buffer_size);
      repetition_series_set_field(series, "function", entry->name);

      while (repetition_series_is_testing(series, &tester))
      {
        repetition_tester_begin_time(&tester);
        entry->function(buffer, buffer_size);
        repetition_tester_close_time(&tester);

        repetition_tester_count_bytes(&tester, buffer_size);
      }

      os_deallocate(buffer, buffer_size);
    }
  }

  repetition_series_save_csv(series, "out.csv");
}
