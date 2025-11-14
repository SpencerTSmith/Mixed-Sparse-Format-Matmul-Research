#ifndef PROFILE_H
#define PROFILE_H
#include "../common.h"

// Just for storing actual timing info and where we need to save that to
typedef struct Profile_Pass Profile_Pass;
struct Profile_Pass
{
  String name;
  u64    start;
  u64    old_elapsed_inclusive;
  usize  zone_index;
  usize  parent_index;
  u64    bytes_processed;
};

// Here we collect info on 'zones' which is all the times a 'pass' hits it
typedef struct Profile_Zone Profile_Zone;
struct Profile_Zone
{
  String name;
  u64    elapsed_exclusive; // Not including child zones
  u64    elapsed_inclusive; // Incuding child zones
  u64    hit_count;
  u64    bytes_processed;
};

typedef struct Profiler Profiler;
struct Profiler
{
  // For the whole profiler duration
  u64 start;

  // DANGER! Needs to be a per thread profiler
  usize current_parent_zone;

  Profile_Zone zones[4096];
};

static
void begin_profiling();

static
void end_profiling();

static
Profile_Pass __profile_begin_pass(String name, usize zone_index, u64 bytes_processed);

static
void __profile_close_pass(Profile_Pass pass);

#ifdef PROFILE
  // Only works for unity builds, but I do those anyways
  #define profile_begin_pass(name) __profile_begin_pass(String(name), __COUNTER__ + 1, 0) // First zone is never used, so the default parent 0 doesn't get junk info
  #define profile_close_pass(block)  __profile_close_pass(block)

  // Helpful, and ok to hardcode name since should only use these once per function scope
  #define profile_begin_func() Profile_Pass __func_pass__ = profile_begin_pass(__func__)
  #define profile_close_func()   profile_close_pass(__func_pass__)

  #define PROFILE_SCOPE(name) \
    Profile_Pass CONCAT(__pass, __LINE__) = profile_begin_pass(name); DEFER_SCOPE(VOID_PROC, profile_close_pass(CONCAT(__pass, __LINE__)))

  #define PROFILE_SCOPE_BANDWIDTH(name, bytes) \
    Profile_Pass CONCAT(__pass, __LINE__) = __profile_begin_pass(String(name), __COUNTER__ + 1, bytes); DEFER_SCOPE(VOID_PROC, profile_close_pass(CONCAT(__pass, __LINE__)))

#else
  #define profile_begin_pass(name)  VOID_PROC
  #define profile_close_pass(block) VOID_PROC
  #define profile_begin_func()      VOID_PROC
  #define profile_close_func()      VOID_PROC
  #define PROFILE_SCOPE(name)
  #define PROFILE_SCOPE_BANDWIDTH(name, bytes)
#endif

#endif // PROFILE_H
