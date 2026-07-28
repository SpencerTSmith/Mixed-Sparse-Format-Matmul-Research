#ifndef PLATFORM_TIMING_H
#define PLATFORM_TIMING_H

#include "../common.h"

// Per OS...
static
u64 get_os_timer_freq(void);

// Per OS...
static
u64 read_os_timer(void);

static
u64 read_os_page_faults(void);

// Per ISA...
static
u64 read_cpu_timer(void);

static
u64 estimate_cpu_timer_freq(void);

static
f64 cpu_time_in_seconds(u64 cpu_time, u64 cpu_timer_frequency);

typedef enum
{
  CPU_PMC_CACHE,
  ETC,
} CPU_PMC_Event;

static
u64 make_cpu_pmc_event(CPU_PMC_Event event);

static
void open_cpu_pmc_event_counting(u64 handle);

static
u64 close_cpu_pmc_event_counting(u64 handle);

#endif // PLATFORM_TIMING_H
