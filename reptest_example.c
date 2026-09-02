// NOTE(spencer): This is all my common stuff
#define LOG_TITLE "REPETITION_TESTER"
#define COMMON_IMPLEMENTATION
#include "common.h"

// NOTE(spencer): I like to do 'unity' builds, so I just include all c files
// into one file, so only this file gets passed to compiler.
#include "benchmark/benchmark_inc.h"
#include "benchmark/benchmark_inc.c"

// NOTE(spencer): Define the interface that all the functions we want to test conform to.
typedef void (*Test_Function)(u8 *buffer, usize buffer_size);

// NOTE(spencer): As we can see this function follows the
// interface of the 'Test_Function' type before.
void read_buffer_linearly(u8 *buffer, usize buffer_size)
{
  volatile u8 sink = 0;
  for (usize i = 0; i < buffer_size; i += 1)
  {
    sink = buffer[i];
  }
}

void read_buffer_randomly(u8 *buffer, usize buffer_size)
{
  volatile u8 sink = 0;

  usize bytes_left = buffer_size;
  while (bytes_left)
  {
    // Probably time will be mostly dominated by modulo in addition to cache misses, but just for demo purposes.
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
  Repetition_Tester testers[STATIC_COUNT(entries)] = {0};

  // NOTE(spencer): Call this to get an estimate of actual current rdtsc timer freq.
  u64 cpu_timer_frequency = estimate_cpu_timer_freq();

  // NOTE(spencer): How many seconds to keep trying for a new min, will reset once
  // we find a new minimum.
  u32 seconds_to_try_for_min = 5;

  for (usize test_func_idx = 0; test_func_idx < STATIC_COUNT(testers); test_func_idx++)
  {
    Function_Entry *entry = &entries[test_func_idx];

    Repetition_Tester *tester = &testers[test_func_idx];

    // Get a new buffer every time to make it fair page fault wise.
    usize buffer_size = GB(1);
    u8 *buffer = os_allocate(buffer_size, OS_ALLOCATION_COMMIT);

    printf("\n--- %s ---\n", entry->name);

    repetition_tester_new_wave(tester, buffer_size, cpu_timer_frequency, seconds_to_try_for_min);
    while (repetition_tester_is_testing(tester))
    {
      repetition_tester_begin_time(tester);
      entry->function(buffer, buffer_size);
      repetition_tester_close_time(tester);

      repetition_tester_count_bytes(tester, buffer_size);
    }

    os_deallocate(buffer, buffer_size);
  }
}
