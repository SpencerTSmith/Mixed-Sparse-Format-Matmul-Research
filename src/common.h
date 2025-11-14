#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C"
{
#endif

/*

  Standard stb style thing, yadda yadda
  put:

      #define COMMON_IMPLEMENTATION
      #include "common.h"

  in exactly one file, though personally I recommend unity builds

  Also, to title your log messages use:

     #define LOG_TITLE "TITLE"

  before defining the implementation

*/


////////////////////////////////////////////////////////////////////////////////////////////////////
// QOL/UTILITY
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stddef.h>

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t  i8;

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t b64;
typedef int32_t b32;
typedef int16_t b16;
typedef int8_t  b8;

typedef double f64;
typedef float  f32;

typedef size_t    usize;
typedef ptrdiff_t isize;

#define true  1
#define false 0

#define _CONCAT(a, b) a##b
#define CONCAT(a, b) _CONCAT(a, b)

#define _STRINGIFY(a) #a
#define STRINGIFY(a) _STRINGIFY(a)

#define STATEMENT(s) do { s } while (0)

#define CLAMP(value, min, max) (((value) < (min)) ? (min) : ((value) > (max)) ? (max) : (value))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

#define SWAP(a, b, T) STATEMENT( T __t = (a); (a) = (b); (b) = __t; )

#define IS_POW2(a) ((((a) - 1) & (a)) == 0)
#define ALIGN_POW2_UP(x, b) (((x) + (b) - 1) & (~((b) - 1)))

#define PI 3.14159265358979323846
#define RADIANS(degrees) ((degrees) * (PI / 180.0))

#define STATIC_ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
 // Slowly replacing the above with this, shorthand
#define STATIC_COUNT(arr) STATIC_ARRAY_COUNT(arr)

#define VOID_PROC ((void)0)

#define KB(n) (1024 * (n))
#define MB(n) (1024 * KB(n))
#define GB(n) (1024L * MB(n)) // L immediate to promote the expression if over 4GB

#define THOUSAND(n) (1000 * (n))
#define MILLION(n) (1000 * THOUSAND(n))
#define BILLION(n) (1000 * MILLION(n))

#define NSEC_PER_SEC BILLION(1)
#define MSEC_PER_SEC THOUSAND(1)

// Only bright colors, please
#define ANSI_RESET   "\x1b[0m"
#define ANSI_BLACK   "\x1b[90m"
#define ANSI_RED     "\x1b[91m"
#define ANSI_GREEN   "\x1b[92m"
#define ANSI_YELLOW  "\x1b[93m"
#define ANSI_BLUE    "\x1b[94m"
#define ANSI_MAGENTA "\x1b[95m"
#define ANSI_CYAN    "\x1b[96m"
#define ANSI_WHITE   "\x1b[97m"

#include <string.h>

#define MEM_SET(ptr, size, value) (memset((ptr), (value), (size)))
#define MEM_COPY(dst, src, size)  (memcpy((dst), (src), (size)))
#define MEM_MOVE(dst, src, size)  (memmove((dst), (src), (size)))
#define MEM_MATCH(a, b, size)     (memcmp((a), (b), (size)) == 0)

#define ZERO_STRUCT(ptr)     (MEM_SET((ptr), sizeof(*(ptr)), 0))
#define ZERO_SIZE(ptr, size) (MEM_SET((ptr), (size), 0))

#define EACH_INDEX(it, count) (usize it = 0; it < (count); it += 1)

#define DEFER_SCOPE(begin, close) \
  for (usize __once__ = (begin, 0); !__once__; __once__++, (close))

#define ENUM_MEMBER(name) name,
#define ENUM_STRING(name) # name,

// This macro makes it very simple to do enum -> string tables
// Keep in mind that this string table is defined static const so it will
// create a copy in every file that includes a file that uses this macro
// You may prefer to do it in the traditional using the above ENUM_* macros
//
// NOTE: Idea from https://philliptrudeau.com/blog/x-macro
#define ENUM_TABLE(Enum_Name)                        \
  typedef enum Enum_Name                             \
  { Enum_Name(ENUM_MEMBER) } Enum_Name;              \
  static const char *CONCAT(Enum_Name, _strings)[] = \
  { Enum_Name(ENUM_STRING) };

// UGLY! For quick regression tests
#define PRINT_EVAL(label, expr)                   \
  printf("[%s]: %s\n", (label), (expr) ?          \
         ANSI_GREEN "PASS :)" ANSI_RESET        : \
         ANSI_RED "FAIL :( @" __FILE__":" STRINGIFY(__LINE__)"\n" "  Expression: " #expr ANSI_RESET)

////////////////////////////////////////////////////////////////////////////////////////////////////
// ARRAY MACRO
////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEFINE_ARRAY(Type)                \
typedef struct Type##_Array Type##_Array; \
struct Type##_Array                       \
{                                         \
  Type  *v;                               \
  usize count;                            \
}

DEFINE_ARRAY(i64);
DEFINE_ARRAY(i32);
DEFINE_ARRAY(i16);
DEFINE_ARRAY(i8);

DEFINE_ARRAY(u64);
DEFINE_ARRAY(u32);
DEFINE_ARRAY(u16);
DEFINE_ARRAY(u8);

DEFINE_ARRAY(b64);
DEFINE_ARRAY(b32);
DEFINE_ARRAY(b16);
DEFINE_ARRAY(b8);

DEFINE_ARRAY(f64);
DEFINE_ARRAY(f32);

DEFINE_ARRAY(usize);
DEFINE_ARRAY(isize);

// No null terminated strings, please
typedef u8_Array String;
DEFINE_ARRAY(String);

////////////////////////////////////////////////////////////////////////////////////////////////////
// LIST MACRO
////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEFINE_LIST(Type)               \
typedef struct Type##_Node Type##_Node; \
struct Type##_Node                      \
{                                       \
  Type##_Node *link_next;               \
  Type        value;                    \
};                                      \
typedef struct Type##_List Type##_List; \
struct Type##_List                      \
{                                       \
  Type##_Node *first;                   \
  Type##_Node *last;                    \
  usize count;                          \
}

DEFINE_LIST(i64);
DEFINE_LIST(i32);
DEFINE_LIST(i16);
DEFINE_LIST(i8);

DEFINE_LIST(u64);
DEFINE_LIST(u32);
DEFINE_LIST(u16);
DEFINE_LIST(u8);

DEFINE_LIST(b64);
DEFINE_LIST(b32);
DEFINE_LIST(b16);
DEFINE_LIST(b8);

DEFINE_LIST(f64);
DEFINE_LIST(f32);

DEFINE_LIST(usize);
DEFINE_LIST(isize);

DEFINE_LIST(String);

////////////////////////////////////////////////////////////////////////////////////////////////////
// LOGGING
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>

#define LOG_ENUM(X) \
  X(LOG_ASSERT)     \
  X(LOG_FATAL)      \
  X(LOG_ERROR)      \
  X(LOG_DEBUG)      \
  X(LOG_INFO)
typedef enum Log_Level
{
  LOG_ENUM(ENUM_MEMBER)
} Log_Level;

// Intended for internal use... probably want to use the macros
void log_message(Log_Level level, const char *file, usize line, const char *message, ...);

#define LOG_FATAL(message, exit_code, ...)                              \
  STATEMENT                                                             \
  (                                                                     \
    log_message(LOG_FATAL, __FILE__, __LINE__, message, ##__VA_ARGS__); \
    exit(exit_code);                                                    \
  )
#define LOG_ERROR(message, ...) log_message(LOG_ERROR, __FILE__, __LINE__, message, ##__VA_ARGS__)

#ifdef DEBUG
  #define LOG_DEBUG(message, ...) log_message(LOG_DEBUG, __FILE__, __LINE__, message, ##__VA_ARGS__)
#else
  #define LOG_DEBUG(message, ...) VOID_PROC
#endif // DEBUG
       //
#define LOG_INFO(message, ...) log_message(LOG_INFO, __FILE__, __LINE__, message, ##__VA_ARGS__)

// Just a little wrapper, don't have to && your message, and complains if you don't
// give it a message, which is good practice and probably ought to force myself to do it
#ifdef DEBUG
  #define ASSERT(expr, message, ...)                                 \
  STATEMENT                                                          \
  (                                                                  \
    if (!(expr))                                                     \
    {                                                                \
      log_message(LOG_ASSERT, __FILE__, __LINE__,                    \
                  "Assertion: (" STRINGIFY(expr) ") :: " message, ##__VA_ARGS__); \
      (*(volatile i32 *) 0 = 0);                                     \
    }                                                                \
  )
#else
  #define ASSERT(expr, message, ...) VOID_PROC
#endif // DEBUG

////////////////////////////////////////////////////////////////////////////////////////////////////
// OS
////////////////////////////////////////////////////////////////////////////////////////////////////

// Basically stolen from Rad Debugger, see what we are compiled for
#if defined(_WIN32)
  #define OS_WINDOWS 1
#elif defined(__gnu_linux__) || defined(__linux__)
  #define OS_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
  #define OS_MAC 1
#else
  #error This OS is not supported.
#endif

typedef enum OS_Allocation_Flags
{
  OS_ALLOCATION_COMMIT    = (1 << 0),
  OS_ALLOCATION_2MB_PAGES = (1 << 1),
  OS_ALLOCATION_1GB_PAGES = (1 << 2),
  OS_ALLOCATION_PREFAULT  = (1 << 3), // Need to see if Windows even has an equivalent?
} OS_Allocation_Flags;

// TODO: Mac and Windows
#ifdef OS_LINUX
 #include <sys/mman.h>
 #include <sys/stat.h>
 #include <sys/random.h>
#elif OS_WINDOWS
 #include <windows.h>
#elif OS_MAC
#endif

void *os_allocate(usize size, OS_Allocation_Flags flags);
b32 os_commit(void *start, usize size);
void os_deallocate(void *start, usize size);

b32 os_fill_buffer_random(String buffer);

////////////////////////////////////////////////////////////////////////////////////////////////////
// MEMORY
////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum Arena_Flags
{
  ARENA_FLAG_NONE = 0,
}
Arena_Flags;

typedef struct Arena Arena;
struct Arena
{
  u8    *base;
  usize reserve_size;
  usize commit_size;
  usize next_offset;

  Arena_Flags flags;
};

typedef struct Arena_Args Arena_Args;
struct Arena_Args
{
  usize reserve_size;
  usize commit_size;
  Arena_Flags flags;

  String make_call_file;
  usize  make_call_line;
};

#define ARENA_DEFAULT_RESERVE_SIZE MB(256)
#define ARENA_DEFAULT_COMMIT_SIZE  KB(64)

// Allocates it's own memory
Arena __arena_make(Arena_Args *args);

#define arena_make(...) __arena_make(&(Arena_Args){                              \
                                     .reserve_size = ARENA_DEFAULT_RESERVE_SIZE, \
                                     .commit_size  = ARENA_DEFAULT_COMMIT_SIZE,  \
                                     .flags        = ARENA_FLAG_NONE,            \
                                     .make_call_file = String(__FILE__),         \
                                     .make_call_line = __LINE__,                 \
                                     __VA_ARGS__})

void arena_free(Arena *arena);
void arena_print_stats(Arena *arena);

void *arena_alloc(Arena *arena, usize size, usize alignment);
void arena_pop_to(Arena *arena, usize offset);
void arena_pop(Arena *arena, usize size);
void arena_clear(Arena *arena);

#include <stdalign.h>

// Arena Helpers ---

// specify the arena, the number of elements, and the type... c(ounted)alloc
#define arena_calloc(a, count, T) (T *)arena_alloc((a), sizeof(T) * (count), alignof(T))
// Useful for structs, much like new in other languages
#define arena_new(a, T) arena_calloc((a), 1, T)

// Array Helpers ---

#define arena_array(a, _count, T) (T##_Array) {.v = arena_calloc((a), (_count), T), .count = (_count)}

// NOTE: EVIL! Macro VOODOO... too much? We will see...
// Only works when building contiguously, IE use a linked list (Type_List), or rethink, if can't guarantee that
// May add relocation later... but maybe not
// Probably also slower than needs to be as we need to go through alloc path for individual elements
#define array_add(a, array, new)                                                                 \
  !((array).v) ?                                                                                 \
    ((array).v = arena_alloc((a), sizeof((array).v[0]), alignof((array).v[0])),                  \
     (array).v[(array).count++] = (new),                                                         \
     (array).v + (array).count - 1)                                                              \
  : arena_alloc((a), sizeof((array).v[0]), alignof((array).v[0])) == (array).v + (array).count ? \
    ((array).v[(array).count++] = (new), (array).v + (array).count - 1)                          \
  : (LOG_ERROR("Tried to add to array in arena noncontiguously!"), arena_pop(a, sizeof((array).v[0])), NULL)

// Linked list Helpers ---

// More generic helpers: first, last, new are all pointers, while next is the name of
// the next link member variable
// NOTE: These are expressions and therefore they will evaluate to a pointer to the node
// that has been pushed
#define SLL_push_first(first, last, new_node, next)                  \
  !(first) ? ((new_node)->next = 0, (first) = (last) = (new_node)) : \
   ((new_node)->next = (first), (first) = (new_node))
#define SLL_push_last(first, last, new_node, next)                   \
  !(first) ? ((new_node)->next = 0, (first) = (last) = (new_node)) : \
   ((last)->next = (new_node), (new_node)->next = 0, (last) = (new_node))

// Helpers specific to the DEFINE_LIST() structures, that is, they assume the naming
// scheme and also increment the count
#define list_push_first(list, new_node) \
  ((list).count++, SLL_push_first((list).first, (list).last, new_node, link_next))
#define list_push_last(list, new_node) \
  ((list).count++, SLL_push_last((list).first, (list).last, new_node, link_next))

// We just want some temporary memory
// ie we save the offset we wish to return to after using this arena as a scratch pad
typedef struct Scratch Scratch;
struct Scratch
{
  Arena *arena;
  usize offset_save;
};

Scratch scratch_begin(Arena *arena);
void scratch_close(Scratch *scratch);

////////////////////////////////////////////////////////////////////////////////////////////////////
// STRINGS
////////////////////////////////////////////////////////////////////////////////////////////////////

#define String(s) (String){(u8 *)(s), STATIC_COUNT(s) - 1}
#define String_Format(s) (int)(s).count, (s).v
#define STR(s) String((s))
#define STRF(s) String_Format((s))

b32 char_is_whitespace(u8 c);
b32 char_is_digit(u8 c);
b32 char_is_alphabetic(u8 c);

u32 string_hash_u32(String string);
b32 string_match(String a, String b);
b32 string_starts_with(String string, String prefix);

String string_skip(String string, usize count);
String string_chop(String string, usize count);
String string_trim_whitespace(String string);

String string_substring(String string, usize start, usize close);
// Returns string.count when not found
usize string_find_substring(String string, usize start, String substring);

String string_from_c_string(char *pointer);
char *string_to_c_string(Arena *arena, String string);

String_Array string_split(Arena *arena, String string, String delimiter);
String_Array string_split_whitepace(Arena *arena, String string);

String string_join_array(Arena *arena, String_Array array, String separator);
String string_join_list(Arena *arena, String_List list, String separator);

// Only useful if you know exactly how big the file is ahead of time, otherwise probably put on an arena if don't know...
// or use file_size()
usize read_file_to_memory(const char *name, u8 *buffer, usize buffer_size);
usize file_size(const char *name);

// Reads the entire thing and returns a String (just a byte slice)
String read_file_to_arena(Arena *arena, String name);

////////////////////////////////////////////////////////////////////////////////////////////////////
// ARGUMENTS
////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: Arguments must be either positionals (a plain string) or options (-string or --string)
// options may have values passed as well (--option=val1,val2,val3)
typedef struct Arg_Option Arg_Option;
struct Arg_Option
{
  Arg_Option   *hash_next;
  u32          hash;
  String       name;
  String_Array values;
};

typedef struct Args Args;
struct Args
{
  String program_name;

  Arg_Option *option_table;
  usize      option_table_count;

  usize  positionals_count;
  String positionals[32];
};

Arg_Option *get_arg_option_bucket(Args *args, String name);
Arg_Option *get_arg_option_from_bucket(Arg_Option *bucket, String name);
Arg_Option *find_arg_option(Args *args, String name);
Arg_Option *insert_arg_option(Arena *arena, Args *args, String name, String_Array values);
Args parse_args(Arena *arena, usize count, char **arguments);

b32 args_has_flag(Args *table, String flag);
String_Array args_get_option_values(Args *table, String option);


#ifdef __cplusplus
} // extern "C"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// IMPLEMENT
////////////////////////////////////////////////////////////////////////////////////////////////////

#define COMMON_IMPLEMENTATION
#ifdef COMMON_IMPLEMENTATION
// Returns size of file, or 0 if it can't open the file
usize read_file_to_memory(const char *name, u8 *buffer, usize buffer_size)
{
  usize byte_count = 0;

  FILE *file = fopen(name, "rb");
  if (file)
  {
    byte_count = fread(buffer, sizeof(u8), buffer_size, file);
    fclose(file);
  }
  else
  {
    LOG_ERROR("Unable to open file: %s", name);
  }

  return byte_count;
}

usize file_size(const char *name)
{
  usize size = 0;
  b32 success = 0;

  // Seriously???
#if OS_WINDOWS
  struct __stat64 stats;
  success = _stat64(name, &stats) == 0;
#else
  struct stat stats;
  success = stat(name, &stats) == 0;
#endif

  if (success)
  {
    size = stats.st_size;
  }
  else
  {
    LOG_ERROR("Unable to determine file size of '%s'", name);
  }

  return size;
}

String read_file_to_arena(Arena *arena, String name)
{
  // Just in case we fail reading we won't commit any allocations
  Arena save = *arena;

  char *_name = string_to_c_string(arena, name); // Ugh

  usize buffer_size = file_size(_name);

  u8 *buffer = arena_calloc(arena, buffer_size, u8);

  if (read_file_to_memory(_name, buffer, buffer_size) != buffer_size)
  {
    LOG_ERROR("Unable to read file: %s", name);
    *arena = save; // Rollback allocation
  }

  String result =
  {
    .v = buffer,
    .count = buffer_size,
  };

  return result;
}

b32 char_is_whitespace(u8 c)
{
  return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

b32 char_is_digit(u8 c)
{
  return c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9';
}

// TODO: Steal a better hash function
// currently using https://en.wikipedia.org/wiki/Jenkins_hash_function
u32 string_hash_u32(String string)
{
  u32 hash = 0;

  for (usize i = 0; i < string.count; i++)
  {
    hash += string.v[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}

b32 string_match(String a, String b)
{
  return a.count == b.count && MEM_MATCH(a.v, b.v, a.count);
}

b32 string_starts_with(String string, String prefix)
{
  b32 result = false;

  // Check string has to be longer or equal to the prefix
  if (string.count >= prefix.count)
  {
    String substring =
    {
      .v = string.v,
      .count = prefix.count,
    };

    result = string_match(prefix, substring);
  }

  return result;
}

String string_from_c_string(char *pointer)
{
  String result =
  {
    .v = (u8 *)pointer,
    .count = 0,
  };

  for (char *cursor = pointer; *cursor; cursor++)
  {
    result.count += 1;
  }

  return result;
}

char *string_to_c_string(Arena *arena, String string)
{
  char *result = arena_calloc(arena, string.count + 1, char);
  MEM_COPY(result, string.v, string.count);

  return result;
}

String string_skip(String string, usize count)
{
  String result = string;

  usize clamp = MIN(result.count, count);
  result.v += clamp;
  result.count -= clamp;

  return result;
}

String string_chop(String string, usize count)
{
  String result = string;

  usize clamp = MIN(result.count, count);
  result.count -= clamp;

  return result;
}

String string_trim_whitespace(String string)
{
  usize start = 0;
  usize stop  = string.count - 1;
  // Eat leading whitespace
  for (; start < string.count; start++)
  {
    u8 c = string.v[start];
    if (!char_is_whitespace(c))
    {
      break;
    }
  }

  for (; stop > start; stop--)
  {
    u8 c = string.v[stop];
    if (!char_is_whitespace(c))
    {
      break;
    }
  }

  return string_substring(string, start, stop + 1);
}

// Start inclusive, stop exclusive
String string_substring(String string, usize start, usize stop)
{
  ASSERT(start <= stop, "Invalid substring range [%lu:%lu]", start, stop);

  String result = string;

  usize clamp_start = MIN(start, string.count);
  usize clamp_stop  = MIN(stop, string.count);
  result.v += clamp_start;
  result.count = (clamp_stop - clamp_start);

  return result;
}

usize string_find_substring(String string, usize start, String substring)
{
  ASSERT(substring.count, "Substring shouldn't be empty");

  isize result = string.count;
  usize comparison_count = string.count - substring.count + 1;

  for (usize i = start; i < comparison_count; i++)
  {
    // Only do full check if first char matches
    if (string.v[i] == substring.v[0])
    {
      String to_compare = string_substring(string, i, i + substring.count);

      if (string_match(to_compare, substring))
      {
        result = i;
        break;
      }
    }
  }

  return result;
}

String_Array string_split(Arena *arena, String string, String delimiter)
{
  String_Array result = {0};

  usize start = 0;
  for (usize delimiter_idx = string_find_substring(string, 0, delimiter);
       delimiter_idx <= string.count && start <= delimiter_idx;
       delimiter_idx = string_find_substring(string, start, delimiter))
  {
    String substring = string_substring(string, start, delimiter_idx);
    array_add(arena, result, substring);

    start = delimiter_idx + delimiter.count;
  }

  return result;
}

String_Array string_split_whitepace(Arena *arena, String string)
{
  String_Array result = {0};

  for (usize i = 0; i < string.count;)
  {
    usize start = i;
    for (; start < string.count; start++)
    {
      if (!char_is_whitespace(string.v[start]))
      {
        break;
      }
    }

    usize stop = start;
    for (; stop < string.count; stop++)
    {
      if (char_is_whitespace(string.v[stop]))
      {
        break;
      }
    }

    if (start < stop) // No empties
    {
      String substring = string_substring(string, start, stop);
      array_add(arena, result, substring);
    }

    i = stop + 1;
  }

  return result;
}

String string_join_array(Arena *arena, String_Array array, String separator)
{
  String result = {0};

  // Perhaps the string data structures ought to just carry around this info so don't need to do a count pass
  usize array_total_char_count = 0;
  for (usize i = 0; i < array.count; i++)
  {
    array_total_char_count += array.v[i].count;
  }

  usize join_total_char_count = 0;
  if (array.count > 0)
  {
    join_total_char_count = (array.count - 1) * separator.count;
  }

  result.count = array_total_char_count + join_total_char_count;
  result.v = arena_calloc(arena, result.count, u8);

  // Over all but the last
  u8 *cursor = result.v;
  for (usize i = 0; i < array.count; i++)
  {
    String to_copy = array.v[i];
    MEM_COPY(cursor, to_copy.v, to_copy.count);
    cursor += to_copy.count;

    // Last string doesn't get a separator
    if (i != array.count - 1)
    {
      MEM_COPY(cursor, separator.v, separator.count);
      cursor += separator.count;
    }
  }

  return result;
}

String string_join_list(Arena *arena, String_List list, String separator)
{
  String result = {0};

  // Perhaps the string data structures ought to just carry around this info so don't need to do a count pass
  usize list_total_char_count = 0;
  for (String_Node *node = list.first; node; node = node->link_next)
  {
    list_total_char_count += node->value.count;
  }

  usize join_total_char_count = (list.count - 1) * separator.count;

  result.count = list_total_char_count + join_total_char_count;
  result.v = arena_calloc(arena, result.count, u8);

  // Over all but the last
  u8 *cursor = result.v;
  for (String_Node *node = list.first; node; node = node->link_next)
  {
    String to_copy = node->value;
    MEM_COPY(cursor, to_copy.v, to_copy.count);
    cursor += to_copy.count;

    // Last string doesn't get a separator
    if (node->link_next)
    {
      MEM_COPY(cursor, separator.v, separator.count);
      cursor += separator.count;
    }
  }

  return result;
}

#ifndef LOG_TITLE
  #define LOG_TITLE "COMMON"
#endif

void log_message(Log_Level level, const char *file, usize line, const char *message, ...)
{
  const char *level_strings[] =
  {
    LOG_ENUM(ENUM_STRING)
  };

  FILE *stream = stderr;
  if (level <= LOG_ERROR)
  {
    fprintf(stream, "[" LOG_TITLE " %s]: (%s:%lu) ", level_strings[level], file, line);
  }
  else
  {
    if (level == LOG_INFO)
    {
      stream = stdout;
    }
    fprintf(stream, "[" LOG_TITLE " %s]: ", level_strings[level]);
  }

  va_list args;
  va_start(args, message);
  vfprintf(stream, message, args);
  va_end(args);

  fprintf(stream, "\n");
}

#ifdef OS_LINUX
void *os_allocate(usize size, OS_Allocation_Flags flags)
{
  u32 prot_flags = PROT_NONE; // By default only reserve
  if (flags & OS_ALLOCATION_COMMIT)
  {
    prot_flags |= (PROT_READ|PROT_WRITE);
  }

  u32 map_flags = MAP_PRIVATE|MAP_ANONYMOUS;
  if (flags & OS_ALLOCATION_2MB_PAGES)
  {
    map_flags |= (MAP_HUGETLB|MAP_HUGE_2MB);
  }
  else if (flags & OS_ALLOCATION_1GB_PAGES) // Can't have both
  {
    map_flags |= (MAP_HUGETLB|MAP_HUGE_1GB);
  }

  if (flags & OS_ALLOCATION_PREFAULT)
  {
    map_flags |= MAP_POPULATE;
  }

  void *result = mmap(NULL, size, prot_flags, map_flags, -1, 0);

  if (result == MAP_FAILED)
  {
    result = NULL;
  }

  return result;
}

b32 os_commit(void *start, usize size)
{
  return mprotect(start, size, PROT_READ|PROT_WRITE) == 0;
}

void os_deallocate(void *start, usize size)
{
  munmap(start, size);
}

void os_decommit(void *start, usize size)
{
  mprotect(start, size, PROT_NONE);
}

b32 os_fill_buffer_random(String buffer)
{
  usize result = getrandom(buffer.v, buffer.count, GRND_NONBLOCK); // Probably don't want to block

  return result == buffer.count;
}
#elif OS_WINDOWS
// TODO
#elif OS_MAC
// TODO
#endif

Arena __arena_make(Arena_Args *args)
{
  // TODO: Large pages, verify that OS and CPU page size actually is 4kb, etc
  usize res = ALIGN_POW2_UP(args->reserve_size, KB(4));
  usize com = ALIGN_POW2_UP(args->commit_size,  KB(4));
  ASSERT(res >= com, "Reserve size must be greater than or equal to commit size.");

  Arena arena = {0};

  arena.base = (u8 *)os_allocate(res, (OS_Allocation_Flags)0);

  // Maybe we do something more gracefully, as this won't be compiled in when DEBUG not defined
  ASSERT(arena.base, "Failed to allocate arena memory (%.*s:%ld)",
         args->make_call_file, args->make_call_line);

  os_commit(arena.base, com);

  arena.reserve_size = res;
  arena.commit_size  = com;
  arena.next_offset  = 0;
  arena.flags        = args->flags;

  return arena;
}

void arena_free(Arena *arena)
{
  os_deallocate(arena->base, arena->reserve_size);

  ZERO_STRUCT(arena);
}

void arena_print_stats(Arena *arena)
{
  printf("Arena ---\n");
  printf("  Reserved:  %ld\n", arena->reserve_size);
  printf("  Committed: %ld\n", arena->commit_size);
}

void *arena_alloc(Arena *arena, usize size, usize alignment) {
  ASSERT(arena->base, "Arena memory is null");

  usize aligned_offset = ALIGN_POW2_UP(arena->next_offset, alignment);
  void *ptr = arena->base + aligned_offset;

  usize wish_capacity = aligned_offset + size;

  // Do we need to commit memory?
  usize wish_commit_size = ALIGN_POW2_UP(wish_capacity, KB(4));
  if (wish_commit_size > arena->commit_size)
  {
    usize commit_diff = wish_commit_size - arena->commit_size;
    usize commit_size = ALIGN_POW2_UP(commit_diff, KB(4)); // Commit only in pages

    // TODO: Probably do separate chaining
    ASSERT(commit_size < arena->reserve_size, "Not enough reserved memory in arena, wish: %ld bytes RESERVED: %ld bytes",
           wish_commit_size, arena->reserve_size);

    os_commit(arena->base + arena->commit_size, commit_size);
    arena->commit_size = wish_commit_size;
  }

  // If we either had the needed memory already, or could commit more
  if (ptr)
  {
    ZERO_SIZE(ptr, size);
    arena->next_offset = wish_capacity;
  }

  return ptr;
}

void arena_pop_to(Arena *arena, usize offset)
{
  ASSERT(offset < arena->next_offset,
         "Failed to pop arena allocation, more than currently allocated");

  // Should we zero out the memory?
  arena->next_offset = offset;
}

void arena_pop(Arena *arena, usize size)
{
  arena_pop_to(arena, arena->next_offset - size);
}

void arena_clear(Arena *arena)
{
  arena->next_offset = 0;
}

Scratch scratch_begin(Arena *arena)
{
  Scratch scratch = {.arena = arena, .offset_save = arena->next_offset};
  return scratch;
}

void scratch_close(Scratch *scratch)
{
  arena_pop_to(scratch->arena, scratch->offset_save);
  ZERO_STRUCT(scratch);
}

Arg_Option *get_arg_option_bucket(Args *args, String name)
{
  Arg_Option *bucket = NULL;

  if (args->option_table_count)
  {
    u32 hash = string_hash_u32(name);

    usize index = hash % args->option_table_count;

    bucket = args->option_table + index;
  }

  return bucket;
}

Arg_Option *get_arg_option_from_bucket(Arg_Option *bucket, String name)
{
  Arg_Option *result = NULL;

  for (Arg_Option *cursor = bucket; cursor; cursor = cursor->hash_next)
  {
    if (string_match(cursor->name, name))
    {
      result = cursor;
      break;
    }
  }

  return result;
}

Arg_Option *find_arg_option(Args *args, String name)
{
  return get_arg_option_from_bucket(get_arg_option_bucket(args, name), name);
}

Arg_Option *insert_arg_option(Arena *arena, Args *args, String name, String_Array values)
{
  Arg_Option *result = NULL;

  Arg_Option *bucket = get_arg_option_bucket(args, name);
  Arg_Option *exists = get_arg_option_from_bucket(bucket, name);

  // We already inserted it
  if (exists)
  {
    result = exists;
  }
  else
  {
    // Collision
    if (bucket->name.v)
    {
      result = arena_new(arena, Arg_Option);

      // Insert at head
      result->hash_next = bucket->hash_next;
      bucket->hash_next = result;
    }
    else
    {
      result = bucket;
    }

    result->hash = string_hash_u32(name);
    result->name = name;
    result->values = values;
  }

  return result;
}

Args parse_args(Arena *arena, usize count, char **arguments)
{
  Args result = {0};
  result.program_name = string_from_c_string(arguments[0]);

  result.option_table_count = 64;
  result.option_table = arena_calloc(arena, result.option_table_count, Arg_Option);

  for (usize i = 1; i < count; i++)
  {
    String string = string_from_c_string(arguments[i]);

    b32 is_option = true;

    // Option
    if (string_starts_with(string, String("--")))
    {
      string = string_skip(string, 2);
    }
    else if (string_starts_with(string, String("-")))
    {
      string = string_skip(string, 1);
    }
    // Positional
    else
    {
      is_option = false;
    }

    if (is_option)
    {
      usize values_delimeter_idx = string_find_substring(string, 0, String("="));

      String name = string_substring(string, 0, values_delimeter_idx);

      String values_substring = string_substring(string, values_delimeter_idx, string.count);
      values_substring = string_skip(values_substring, 1); // Skip the delimiter

      // Add any values
      String_Array values = string_split(arena, values_substring, String(","));

      insert_arg_option(arena, &result, name, values);
    }

    // Its a positional
    else
    {
      ASSERT(result.positionals_count < STATIC_COUNT(result.positionals), "Too many positional arguments for parsing");
      result.positionals[result.positionals_count] = string;
      result.positionals_count += 1;
    }
  }

  return result;
}

b32 args_has_flag(Args *table, String flag)
{
  return find_arg_option(table, flag) != NULL;
}

String_Array args_get_option_values(Args *table, String option)
{
  return find_arg_option(table, option)->values;
}

#endif // COMMON_IMPLEMENTATION
#endif // COMMON_H
