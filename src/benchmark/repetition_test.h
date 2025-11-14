#ifndef REPETITION_TEST_H
#define REPETITION_TEST_H

#include "platform_timing.h"

typedef enum Repetition_Tester_Mode
{
  REPTEST_MODE_UNINITIALIZED,

  REPTEST_MODE_TESTING,
  REPTEST_MODE_COMPLETE,
  REPTEST_MODE_ERROR,

  REPTEST_MODE_COUNT,
} Repetition_Tester_Mode;

typedef enum Repetition_Test_Value
{
  REPTEST_VALUE_NONE,

  REPTEST_VALUE_TIME,
  REPTEST_VALUE_PAGE_FAULTS,
  REPTEST_VALUE_BYTE_COUNT,
  REPTEST_VALUE_FLOP_COUNT,
  REPTEST_VALUE_MEMOP_COUNT,

  REPTEST_VALUE_COUNT,
} Repetition_Test_Value;

typedef struct Repetition_Test_Values Repetition_Test_Values;
struct Repetition_Test_Values
{
  u64 v[REPTEST_VALUE_COUNT];
};

typedef struct Repetition_Test Repetition_Test;
struct Repetition_Test
{
  Repetition_Test_Values accum;
  u32 begin_block_count;
  u32 close_block_count;
};

typedef struct Repetition_Tester_Results Repetition_Tester_Results;
struct Repetition_Tester_Results
{
  u64 test_count;
  Repetition_Test_Values min;
  Repetition_Test_Values max;
  Repetition_Test_Values total;
};

typedef struct Repetition_Tester Repetition_Tester;
struct Repetition_Tester
{
  u64 target_processed_byte_count;
  u64 cpu_timer_frequency;
  u64 try_for_min_time;
  u64 tests_start_time;

  Repetition_Tester_Mode mode;

  Repetition_Test current_test;
  Repetition_Tester_Results results;
};

static
void repetition_tester_begin_time(Repetition_Tester *tester);

static
void repetition_tester_close_time(Repetition_Tester *tester);

static
void repetition_tester_count_bytes(Repetition_Tester *tester, u64 bytes);

static
void repetition_tester_count_flops(Repetition_Tester *tester, u64 count);
static
void repetition_tester_count_memops(Repetition_Tester *tester, u64 count);

static
void repetition_tester_error(Repetition_Tester *tester, const char *message);

static
void print_repetition_test_values(const char *label, Repetition_Test_Values values,
                                  u64 cpu_timer_frequency, u64 test_count);

static
void repetition_tester_new_wave(Repetition_Tester *tester, u64 target_processed_byte_count,
                                u64 cpu_timer_frequency, u32 seconds_to_try_for_min);

static
b32 repetition_tester_is_testing(Repetition_Tester *tester);

// You provide the definitions for these
typedef struct Operation_Parameters Operation_Parameters;
typedef void Operation_Function(Repetition_Tester *tester, Operation_Parameters *params);

typedef struct Operation_Entry Operation_Entry;
struct Operation_Entry
{
  String name;
  Operation_Function *function;
};

#endif // REPETITION_TEST_H
