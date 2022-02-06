/*
 * mleak.h - Single header/source library for catching memory leaks from using
 * malloc, calloc, realloc & free. In order to use this library you have to
 * compile with -ldl for dlsym().
 *
 * Copyright (c) 2022 mini-rose
 */
#if !defined(MLEAK_H) && !defined(NO_MLEAK)
#define MLEAK_H

#include <stddef.h>

#if !defined(__GNUC__) && !defined(__clang__) && !defined(__chibicc__)
# error "unsupported compiler, remove this check if you know what you're doing"
#endif

#if defined(__GNUC__)
# define MLEAK_FUNC     __PRETTY_FUNCTION__
#else
# define MLEAK_FUNC     __func__
#endif

#ifndef _mleak_exit
/* Usually, if some error occurs in the mleak library it calls exit(1). You may
   change this by defining _mleak_exit to be anything you want. */
# define _mleak_exit exit(1)
#endif

#define free(PTR)           _mleak_free(PTR, __FILE__, __LINE__, MLEAK_FUNC)
#define malloc(SIZE)        _mleak_malloc(SIZE, __FILE__, __LINE__, MLEAK_FUNC)
#define calloc(SIZE, N)     _mleak_calloc(SIZE, N, __FILE__, __LINE__, \
        MLEAK_FUNC)
#define realloc(PTR, SIZE)  _mleak_realloc(PTR, SIZE, __FILE__, __LINE__, \
        MLEAK_FUNC)

void _mleak_free(void *ptr, char *file, int line, const char *func);
void *_mleak_malloc(size_t size, char *file, int line, const char *func);
void *_mleak_calloc(size_t size, size_t elems, char *file, int line,
        const char *func);
void *_mleak_realloc(void *ptr, size_t size, char *file, int line,
        const char *func);

/* Free a value that has not been registered by mleak. This, for example, could
   be string allocated by strdup(). Note that because this is an unchecked free,
   passing a registered pointer will result in a "memory leak" message. */
void unchecked_free(void *ptr);


#endif /* MLEAK_H */
