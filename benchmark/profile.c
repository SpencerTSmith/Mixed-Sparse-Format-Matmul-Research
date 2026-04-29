#include "profile.h"

#include "../common.h"
#include "platform_timing.h"

static Profiler g_profiler;

static
void begin_profiling()
{
  g_profiler = (Profiler)
  {
    .start = read_cpu_timer(),
  };
}

static
void end_profiling()
{
  u64 total_delta = read_cpu_timer() - g_profiler.start;

  if (total_delta)
  {
    u64 freq = estimate_cpu_timer_freq();
    printf("[PROFILE] Total duration: %lu (%f ms @ %lu Hz)\n", total_delta, (f64)total_delta / (f64)freq * 1000.0, freq);

    f64 exclusive_percent = 0.0;

    for (usize i = 0; i < STATIC_ARRAY_COUNT(g_profiler.zones); i++)
    {
      Profile_Zone *zone = &g_profiler.zones[i];

      if (zone->elapsed_inclusive)
      {
        f64 percent = ((f64)zone->elapsed_exclusive / (f64)total_delta) * 100.0;

        printf("[PROFILE] Zone '%.*s':\n"
               "  Hit Count: %lu\n"
               "  Exclusive Timestamp Cycles: %lu (%.4f%%)\n"
               , String_Format(zone->name), zone->hit_count, zone->elapsed_exclusive, percent);

        if (zone->elapsed_exclusive != zone->elapsed_inclusive)
        {
          f64 with_children_percent = ((f64)zone->elapsed_inclusive / (f64)total_delta) * 100.0;
          printf("  Inclusive Timestamp Cycles: %lu (%.4f%%)\n", zone->elapsed_inclusive, with_children_percent);
        }

        exclusive_percent += percent;

        if (zone->bytes_processed)
        {
          f64 megabytes = (f64)zone->bytes_processed / MB(1);

          f64 gb_per_s = (f64)zone->bytes_processed / ((f64)zone->elapsed_inclusive / (f64)freq) / (f64)GB(1.0);

          printf("  Megabytes Processed: %fMB @ %f GB/s\n", megabytes, gb_per_s);
        }
      }
    }
  }
}

static
Profile_Pass __profile_begin_pass(String name, usize zone_index, u64 bytes_processed)
{
  Profile_Pass pass =
  {
    .parent_index = g_profiler.current_parent_zone,
    .name         = name,
    .zone_index   = zone_index,
    .old_elapsed_inclusive = g_profiler.zones[zone_index].elapsed_inclusive, // Save the original so it get overwritten in the case of children
    .bytes_processed = bytes_processed,
  };

  // Push parent
  g_profiler.current_parent_zone = zone_index;

  // Last!
  pass.start = read_cpu_timer();

  return pass;
}

static
void __profile_close_pass(Profile_Pass pass)
{
  // First!
  u64 elapsed = read_cpu_timer() - pass.start;

  // Pop parent
  g_profiler.current_parent_zone = pass.parent_index;

  Profile_Zone *current = &g_profiler.zones[pass.zone_index];
  current->elapsed_exclusive += elapsed;
  current->hit_count += 1;
  current->name = pass.name; // Stupid...
  current->elapsed_inclusive = pass.old_elapsed_inclusive + elapsed; // So that only the final out of potential recursive calls writes inclusive time
  current->bytes_processed += pass.bytes_processed;

  // Accumulate to parent time
  Profile_Zone *parent = &g_profiler.zones[pass.parent_index];
  parent->elapsed_exclusive -= elapsed;
}
