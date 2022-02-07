/*
 * mleak - library for catching memory leaks, double-frees, invalid pointers
 * passed to free & realloc, couting allocations and used heap memory.
 *
 * If you're profiling allocations, you may want to use `-DMLEAK_TRACK` to print
 * the amount of calls to the allocation function, and the total amount of bytes
 * allocated.
 *
 * Copyright (c) 2022 mini-rose
 */
#ifndef MLEAK_H
#define MLEAK_H
#ifndef NO_MLEAK

#include <features.h>
#include <stddef.h>

#if defined __GNUC__ || defined __clang__
/* If we're compiling under GNU GCC or Clang, we can use the __PRETTY_FUNCTION__
   macro to get some nice names in the errors. This also may help with C++ for
   overloaded functions. */
# define __mleak_func   __PRETTY_FUNCTION__
#else
# define __mleak_func   __func__
#endif

void _mleak_free(void *ptr, char *file, int line);
void *_mleak_malloc(size_t size, char *file, int line, const char *func);
void *_mleak_calloc(size_t size, size_t elems, char *file, int line,
        const char *func);

#if _XOPEN_SOURCE >= 500
/* Duplicate a string */
char *_mleak_strdup(const char *str, char *file, int line, const char *func);
#endif

/* If `ptr` is NULL, this call will be redirected and registered as a malloc,
   not a realloc. Keep that in mind when calculating calls with MLEAK_TRACK. */
void *_mleak_realloc(void *ptr, size_t size, char *file, int line,
        const char *func);

/* Free a value that has not been registered by mleak. Note that because this
   is an unchecked free, passing a registered pointer will result in a "memory
   leak" message. */
void unchecked_free(void *ptr);


#define free(PTR) _mleak_free(PTR, __FILE__, __LINE__)
#define malloc(SIZE) _mleak_malloc(SIZE, __FILE__, __LINE__, __mleak_func)
#define calloc(SIZE, N) _mleak_calloc(SIZE, N, __FILE__, __LINE__, __mleak_func)
#define realloc(PTR, SIZE) _mleak_realloc(PTR, SIZE, __FILE__, __LINE__, \
        __mleak_func)

#if _XOPEN_SOURCE >= 500
# define strdup(STR) _mleak_strdup(STR, __FILE__, __LINE__, MLEAK_FUNC)
#endif

#endif /* !NO_MLEAK */
#endif /* !MLEAK_H */
