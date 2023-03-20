/*
 * mleak - library for catching memory leaks, double-frees, invalid pointers
 * passed to free & realloc, couting allocations and used heap memory.
 *
 * Copyright (c) 2022 mini-rose
 */
#ifndef MLEAK_H
#define MLEAK_H

#include <features.h>
#include <stddef.h>

#if defined __GNUC__ || defined __clang__
/* If we're compiling under GNU GCC or Clang, we can use the __PRETTY_FUNCTION__
   macro to get some nice names in the errors. This also may help with C++ for
   overloaded functions. */
# define MLEAK_FUNC   __PRETTY_FUNCTION__
#else
# define MLEAK_FUNC   __func__
#endif

void mleak_free(void *ptr, char *file, int line);
void *mleak_malloc(size_t size, char *file, int line, const char *func);
void *mleak_calloc(size_t size, size_t elems, char *file, int line,
		const char *func);
void *mleak_realloc(void *ptr, size_t size, char *file, int line,
		const char *func);

#if _XOPEN_SOURCE >= 500
/* Duplicate a string */
char *mleak_strdup(const char *str, char *file, int line, const char *func);
#endif

/* Free a value that has not been registered by mleak. Note that because this
   is an unchecked free, passing a registered pointer will result in a "memory
   leak" message. */
void mleak_unchecked_free(void *ptr);

/* Store current mleak statistics. You may retrieve all this information from
   the library using mleak_getstat(). This can be used to profile to memory
   allocations and used up heap memory.

   Note that when calculating realloc() calls, when called with a NULL pointer
   as the first argument, a realloc() call gets moved and registered as a call
   to malloc(). */
struct mleak_stat
{
	size_t ml_total;
	size_t ml_frees;
	size_t ml_mallocs;
	size_t ml_callocs;
	size_t ml_reallocs;
	size_t ml_strdups;
};

/* Get the current statistics from the library. This will copy the data into
   the passed structure, so you may read and write to it with no problem. */
void mleak_getstat(struct mleak_stat *mlstat);

/* Print the passed mleak_stat to stdout in 2 lines: the total allocated bytes
   and the amount of calls. */
void mleak_printstat(struct mleak_stat *mlstat);

#ifndef MLEAK_NO_MACROS
/* If MLEAK_NO_MACROS is not defined, the standard library factories will not
   get overwritten. */
#define free(PTR) mleak_free(PTR, __FILE__, __LINE__)
#define malloc(SIZE) mleak_malloc(SIZE, __FILE__, __LINE__, MLEAK_FUNC)
#define calloc(SIZE, N) mleak_calloc(SIZE, N, __FILE__, __LINE__, MLEAK_FUNC)
#define realloc(PTR, SIZE) mleak_realloc(PTR, SIZE, __FILE__, __LINE__, \
		MLEAK_FUNC)
#define unchecked_free(PTR) mleak_unchecked_free(PTR)

#if _XOPEN_SOURCE >= 500
# define strdup(STR) mleak_strdup(STR, __FILE__, __LINE__, MLEAK_FUNC)
#endif
#endif /* !MLEAK_NO_MACROS */

#endif /* !MLEAK_H */
