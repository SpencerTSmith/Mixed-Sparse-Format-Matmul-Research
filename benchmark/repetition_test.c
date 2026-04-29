#include "repetition_test.h"

#include "platform_timing.h"

static
void repetition_tester_begin_time(Repetition_Tester *tester)
{
  Repetition_Test *curr = &tester->current_test;
  curr->begin_block_count += 1;
  curr->accum.v[REPTEST_VALUE_TIME] -= read_cpu_timer();
  curr->accum.v[REPTEST_VALUE_PAGE_FAULTS]  -= read_os_page_faults();
}

static
void repetition_tester_close_time(Repetition_Tester *tester)
{
  Repetition_Test *curr = &tester->current_test;
  curr->accum.v[REPTEST_VALUE_TIME] += read_cpu_timer();
  curr->accum.v[REPTEST_VALUE_PAGE_FAULTS] += read_os_page_faults();
  curr->close_block_count += 1;
}

static
void repetition_tester_count_bytes(Repetition_Tester *tester, u64 bytes)
{
  Repetition_Test *curr = &tester->current_test;
  curr->accum.v[REPTEST_VALUE_BYTE_COUNT] += bytes;
}

static
void repetition_tester_count_flops(Repetition_Tester *tester, u64 count)
{
  Repetition_Test *curr = &tester->current_test;
  curr->accum.v[REPTEST_VALUE_FLOP_COUNT] += count;
}

static
void repetition_tester_count_memops(Repetition_Tester *tester, u64 count)
{
  Repetition_Test *curr = &tester->current_test;
  curr->accum.v[REPTEST_VALUE_MEMOP_COUNT] += count;
}

static
void repetition_tester_error(Repetition_Tester *tester, const char *message)
{
  LOG_ERROR("%s", message);
  tester->mode = REPTEST_MODE_ERROR;
}

static
void print_repetition_test_values(const char *label, Repetition_Test_Values values, u64 cpu_timer_frequency, u64 test_count)
{
  u64 divisor = test_count ? test_count : 1;

  u64 time = values.v[REPTEST_VALUE_TIME] / divisor;
  printf("%s: %lu", label, time);
  if (cpu_timer_frequency)
  {
    f64 seconds = cpu_time_in_seconds(time, cpu_timer_frequency);
    printf(" (%.4fms)", 1000.0 * seconds);

    u64 byte_count = values.v[REPTEST_VALUE_BYTE_COUNT] / divisor;
    if (byte_count)
    {
      f64 gb_per_s = (f64)byte_count / (f64)GB(1) / seconds;

      printf(" %.4f GB/s", gb_per_s);
    }

    u64 page_faults = values.v[REPTEST_VALUE_PAGE_FAULTS] / divisor;
    if (page_faults)
    {
      f64 kb_per_fault = ((f64)byte_count / KB(1)) / (f64)page_faults;

      printf(", %lu faults (%.4f kb/fault)", page_faults, kb_per_fault);
    }

    u64 flops = values.v[REPTEST_VALUE_FLOP_COUNT] / divisor;
    if (flops)
    {
      printf(", %lu flops", flops);
    }

    u64 memops = values.v[REPTEST_VALUE_MEMOP_COUNT] / divisor;
    if (memops)
    {
      printf(", %lu memops", memops);
    }
  }
}

static
void repetition_tester_new_wave(Repetition_Tester *tester, u64 target_processed_byte_count, u64 cpu_timer_frequency, u32 seconds_to_try_for_min)
{
  if (tester->mode == REPTEST_MODE_UNINITIALIZED)
  {
    tester->mode = REPTEST_MODE_TESTING;
    tester->target_processed_byte_count = target_processed_byte_count;
    tester->cpu_timer_frequency = cpu_timer_frequency;
    tester->results.min.v[REPTEST_VALUE_TIME] = (u64)-1;
  }
  else if (tester->mode == REPTEST_MODE_COMPLETE)
  {
    tester->mode = REPTEST_MODE_TESTING;

    if (cpu_timer_frequency != tester->cpu_timer_frequency)
    {
      repetition_tester_error(tester, "Repetition Tester CPU frequency has changed from previous wave");
    }

    if (target_processed_byte_count != tester->target_processed_byte_count)
    {
      repetition_tester_error(tester, "Repetition Tester target byte count has changed from previous wave");
    }
  }

  tester->try_for_min_time = seconds_to_try_for_min * cpu_timer_frequency;
  tester->tests_start_time = read_cpu_timer();
}

static
b32 repetition_tester_is_testing(Repetition_Tester *tester)
{
  if (tester->mode == REPTEST_MODE_TESTING)
  {
    u64 current_time = read_cpu_timer();

    Repetition_Test curr = tester->current_test;

    // Only tests that have timing blocks get counted
    if (curr.begin_block_count)
    {
      if (curr.close_block_count != curr.begin_block_count)
      {
        repetition_tester_error(tester, "Tester has uneven timing blocks");
      }
      // FIXME:
      // if (curr.accum.v[REPTEST_VALUE_BYTE_COUNT] != tester->target_processed_byte_count)
      // {
      //   repetition_tester_error(tester, "Tester has mismatched target and actual bytes processed");
      // }

      if (tester->mode == REPTEST_MODE_TESTING) // We are all good no errors from previous checks
      {
        Repetition_Tester_Results *results = &tester->results;
        results->test_count += 1;

        for (usize i = 0; i < STATIC_ARRAY_COUNT(results->total.v); i++)
        {
          results->total.v[i] += curr.accum.v[i];
        }

        if (curr.accum.v[REPTEST_VALUE_TIME] > results->max.v[REPTEST_VALUE_TIME])
        {
          results->max = curr.accum;
        }

        // Have some special stuff to do if we find a new min
        if (curr.accum.v[REPTEST_VALUE_TIME] < results->min.v[REPTEST_VALUE_TIME])
        {
          results->min = curr.accum;

          // Restart time to find new min
          tester->tests_start_time = current_time;

          printf("                                                                                        \r");
          print_repetition_test_values("MIN", results->min, tester->cpu_timer_frequency, 1);
          printf("\r");
          fflush(stdout);
        }

        // Reset
        tester->current_test = (Repetition_Test){0};

        // Now check if long enough time has passed without finding a new min
        if ((current_time - tester->tests_start_time) > tester->try_for_min_time)
        {
          tester->mode = REPTEST_MODE_COMPLETE;

          print_repetition_test_values("MIN", results->min, tester->cpu_timer_frequency, 1);
          printf("\n");

          print_repetition_test_values("MAX", results->max, tester->cpu_timer_frequency, 1);
          printf("\n");

          if (results->test_count)
          {
            printf("                                                          \r");
            fflush(stdout);
            print_repetition_test_values("AVG", results->total, tester->cpu_timer_frequency, results->test_count);
            printf("\n");
          }
        }
      }
    }
  }

  return tester->mode == REPTEST_MODE_TESTING;
}
