#include "common.h"

// NOTE(ss): They really suggest not to do this as these are black box memory barriers that mess with compiler optimization... But no simple way to let people instrument...
#define LLVM_MCA_BEGIN() \
  asm volatile("# LLVM-MCA-BEGIN" ::: "memory")
#define LLVM_MCA_END() \
  asm volatile("# LLVM-MCA-END":::"memory")

#define OSACA_BEGIN() \
  asm volatile("# OSACA-BEGIN " ::: "memory")
#define OSACA_END() \
  asm volatile("# OSACA-END " ::: "memory")

static u64 simulated_flops  = 0;
static u64 simulated_memops = 0;

static
void simulate_flop(u64 count)
{
  simulated_flops += count;
}

static
void simulate_memop(u64 count)
{
  simulated_memops += count;
}

#define SIMULATE_FLOPS
#define SIMULATE_MEMOPS

#ifndef SIMULATE_FLOPS
#define FMADD(dst, a, b) dst += (a * b);
#else
#define FMADD(dst, a, b) dst += (a * b); simulate_flop(2)
#endif // SIMULATE_FLOPS

// Will probably want to do something different for reads and writes in future
// may also want to do c++ operator overloading
#ifndef SIMULATE_MEMOPS
#define LOAD(src)       src;
#define STORE(dst, src) dst = src;
#else
#define LOAD(src)       src;       simulate_memop(1)
#define STORE(dst, src) dst = src; simulate_memop(1)
#endif // SIMULATE_MEMOPS
