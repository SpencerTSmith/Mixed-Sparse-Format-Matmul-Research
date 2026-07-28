#include "platform_timing.h"

// TODO: clean this up
#include <x86intrin.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <linux/perf_event.h>
#include <unistd.h>
#include <sys/ioctl.h>

// NOTE(ss): Will need to be defined per ISA
static
u64 read_cpu_timer(void)
{
  return __rdtsc();
}

// Just an estimation, in microseconds
  static
u64 estimate_cpu_timer_freq(void)
{
  u64 wait_milliseconds = 1000;
  u64 os_frequency = get_os_timer_freq();

  u64 cpu_start = read_cpu_timer();
  u64 os_start  = read_os_timer();

  u64 os_end   = 0;
  u64 os_delta = 0;

  // In microseconds
  u64 os_wait_time = (os_frequency * wait_milliseconds) / 1000;
  while (os_delta < os_wait_time)
  {
    os_end   = read_os_timer();
    os_delta = os_end - os_start;
  }

  u64 cpu_end   = read_cpu_timer();
  u64 cpu_delta = cpu_end - cpu_start;

  u64 cpu_frequency = 0;

  ASSERT(os_delta, "OS Time delta for cpu frequency estimation was somehow 0!");

  // CPU time in OS ticks, divide by OS delta gives estimate of cpu frequency
  cpu_frequency = os_frequency * cpu_delta / os_delta;

  return cpu_frequency;
}

static
f64 cpu_time_in_seconds(u64 cpu_time, u64 cpu_timer_frequency)
{
  f64 result = 0.0;
  if (cpu_timer_frequency)
  {
    result = (f64)cpu_time / (f64)cpu_timer_frequency;
  }

  return result;
}


#ifdef OS_LINUX
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>

static
u64 read_os_page_faults(void)
{
  struct rusage usage = {0};
  getrusage(RUSAGE_SELF, &usage);

  u64 hard_faults = usage.ru_majflt;
  u64 soft_faults = usage.ru_minflt;

  // For now just count both, maybe we get more specific later
  return hard_faults + soft_faults;
}

// NOTE(ss): Will need to be defined per OS
static
u64 get_os_timer_freq(void)
{
  // Posix gettimeofday is in microseconds
  return 1000000;
}

// NOTE(ss): Will need to be defined per OS
static
u64 read_os_timer(void)
{
  struct timeval value;
  gettimeofday(&value, 0);
  u64 result = get_os_timer_freq() * value.tv_sec + value.tv_usec;

  return result;
}

static
long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                     int cpu, int group_fd, unsigned long flags)
{
  // NOTE: Sometimes a system's linux headers might not have '__NR_perf_event_open' ...
  return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

static
u64 make_cpu_pmc_event(CPU_PMC_Event event)
{
  struct perf_event_attr pe;
  memset(&pe, 0, sizeof(pe));
  pe.type = PERF_TYPE_HARDWARE;
  pe.size = sizeof(pe);
  pe.disabled = 1;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;

  switch (event)
  {
    case CPU_PMC_CACHE:
    {
      pe.config = PERF_COUNT_HW_CACHE_MISSES;
    } break;
    case ETC:
    {
    } break;
  }

  u64 handle = 0;

  int fd = perf_event_open(&pe, 0, -1, -1, 0);

  if (fd != -1)
  {
    handle = (u64)fd;
  }

  return handle;
}

static
void open_cpu_pmc_event_counting(u64 handle)
{
  int fd = (int)handle;
  ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
}

static
u64 close_cpu_pmc_event_counting(u64 handle)
{
  int fd = (int)handle;
  ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);

  long long count = 0;
  read(fd, &count, sizeof(count));

  return (u64)count;
}

#endif
