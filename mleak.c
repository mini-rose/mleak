/*
 * mleak.c - mleak implementation
 * Copyright (c) 2022 mini-rose
 */
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include "mleak.h"

typedef void (*free_ft) (void *);
typedef void *(*malloc_ft) (size_t);
typedef void *(*calloc_ft) (size_t, size_t);
typedef void *(*realloc_ft) (void *, size_t);
typedef char *(*strdup_ft) (const char *);

#define ALLOC_FREE      1
#define ALLOC_MALLOC    2
#define ALLOC_CALLOC    3
#define ALLOC_REALLOC   4
#define ALLOC_STRDUP    5

#define LINESIZE        256

struct allocation
{
    void *ptr;
    size_t size;
    int type;
    int line;
    char *func;
    char *file;
};

struct slist
{
    char **strings;
    size_t size;
    size_t space;
};

struct allocation_list
{
    struct allocation **allocs;
    struct mleak_stat mlstat;
    size_t size;
    size_t space;
};

static bool is_initialized = false;
static free_ft sys_free = NULL;
static malloc_ft sys_malloc = NULL;
static calloc_ft sys_calloc = NULL;
static realloc_ft sys_realloc = NULL;
static strdup_ft sys_strdup = NULL;

/* All strings are stored in this single array. This contains all file and
   function names that are stored when allocating something. */
static struct slist strings = {0};
static pthread_mutex_t mut_strings = PTHREAD_MUTEX_INITIALIZER;

/* List of all allocations. This is verified after the program exits, meaning
   when deconstruct() gets called. */
static struct allocation_list allocs = {0};
static pthread_mutex_t mut_allocs = PTHREAD_MUTEX_INITIALIZER;

static char *strings_add(const char *str);
static struct allocation *allocation_new();
static struct allocation *allocation_find_by_ptr(void *ptr);
static void notify_about_leak(struct allocation *alloc);
static void print_source_code(char *file, int line, void *ptr);
static void initialize();
static void deconstruct();


void mleak_free(void *ptr, char *file, int line)
{
    struct allocation *alloc;

    if (!is_initialized)
        initialize();

    if (!ptr)
        return;

    alloc = allocation_find_by_ptr(ptr);
    if (!alloc) {
        fprintf(stderr, "\033[91mfree() called with unregistered pointer:"
                "\033[0m\n");
        print_source_code(file, line, ptr);
        exit(1);
    }

    if (alloc->type == ALLOC_FREE) {
        fprintf(stderr, "\033[91mfree() called with already free'd pointer:"
                "\033[0m\n");
        print_source_code(file, line, ptr);

        fprintf(stderr, "Previously free'd here:\n");
        print_source_code(alloc->file, alloc->line, alloc->ptr);

        exit(1);
    }

    alloc->line = line;
    alloc->func = strings_add(alloc->func);
    alloc->file = strings_add(alloc->file);
    alloc->type = ALLOC_FREE;
    sys_free(ptr);

    allocs.mlstat.ml_frees++;
}

void *mleak_malloc(size_t size, char *file, int line, const char *func)
{
    if (!is_initialized)
        initialize();

    void *ptr = sys_malloc(size);

    struct allocation *alloc = allocation_new();
    alloc->ptr = ptr;
    alloc->size = size;
    alloc->line = line;
    alloc->func = strings_add(func);
    alloc->file = strings_add(file);
    alloc->type = ALLOC_MALLOC;

    allocs.mlstat.ml_total += size;
    allocs.mlstat.ml_mallocs++;
    return ptr;
}

void *mleak_calloc(size_t size, size_t elems, char *file, int line,
        const char *func)
{
    if (!is_initialized)
        initialize();

    void *ptr = sys_calloc(size, elems);

    struct allocation *alloc = allocation_new();
    alloc->ptr = ptr;
    alloc->size = size * elems;
    alloc->line = line;
    alloc->func = strings_add(func);
    alloc->file = strings_add(file);
    alloc->type = ALLOC_CALLOC;

    allocs.mlstat.ml_total += size;
    allocs.mlstat.ml_callocs++;
    return ptr;
}

void *mleak_realloc(void *ptr, size_t size, char *file, int line,
        const char *func)
{
    struct allocation *alloc, *previous_alloc;
    void *new_ptr;

    if (!is_initialized)
        initialize();

    if (!ptr)
        return mleak_malloc(size, file, line, func);

    previous_alloc = allocation_find_by_ptr(ptr);
    if (!previous_alloc) {
        fprintf(stderr, "\033[91mrealloc() called with unregistered pointer:"
                "\033[0m\n");
        print_source_code(file, line, ptr);
        exit(1);
    }

    new_ptr = sys_realloc(ptr, size);

    if (new_ptr == ptr) {
        /* If realloc() returns the same pointer as the old one, only the size
           is changed so we can just update the previous allocation. */
        if (size > previous_alloc->size)
            allocs.mlstat.ml_total += size - previous_alloc->size;

        previous_alloc->size = size;
        previous_alloc->line = line;
        previous_alloc->func = strings_add(func);
        previous_alloc->file = strings_add(file);
        previous_alloc->type = ALLOC_REALLOC;
        return new_ptr;
    }

    /* If we get here, it means that realloc() returned a brand new pointer, so
       we have to mark the previous allocation as ALLOC_FREE. */
    previous_alloc->type = ALLOC_FREE;

    alloc = allocation_new();
    alloc->ptr = new_ptr;
    alloc->size = size;
    alloc->line = line;
    alloc->func = strings_add(func);
    alloc->file = strings_add(file);
    alloc->type = ALLOC_REALLOC;

    allocs.mlstat.ml_reallocs++;
    allocs.mlstat.ml_total += size;
    return new_ptr;
}

char *mleak_strdup(const char *str, char *file, int line, const char *func)
{
    struct allocation *alloc;
    char *new_str;
    size_t size;

    if (!is_initialized)
        initialize();

    new_str = sys_strdup(str);
    size = strlen(new_str) + 1;

    alloc = allocation_new();
    alloc->ptr = new_str;
    alloc->size = size;
    alloc->line = line;
    alloc->func = strings_add(func);
    alloc->file = strings_add(file);
    alloc->type = ALLOC_STRDUP;

    allocs.mlstat.ml_total += size;
    allocs.mlstat.ml_strdups++;
    return new_str;
}

void mleak_unchecked_free(void *ptr)
{
    if (!is_initialized)
        initialize();
    sys_free(ptr);
}

void mleak_getstat(struct mleak_stat *mlstat)
{
    memcpy(mlstat, &allocs.mlstat, sizeof(*mlstat));
}

void mleak_printstat(struct mleak_stat *mlstat)
{
    printf("mleak: allocated %zu bytes\n", mlstat->ml_total);
    printf("mleak: mallocs=%zu callocs=%zu reallocs=%zu frees=%zu strdups=%zu"
            "\n", mlstat->ml_mallocs, mlstat->ml_callocs, mlstat->ml_reallocs,
            mlstat->ml_frees, mlstat->ml_strdups);
}

/* Private functions */

/* Add a new string into the strings list. Returns a pointer to string. */
static char *strings_add(const char *str)
{
    pthread_mutex_lock(&mut_strings);

    /* If the string already exists in the string list, return a pointer to the
       already allocated string. */
    for (size_t i = 0; i < strings.size; i++) {
        if (!strcmp(strings.strings[i], str)) {
            pthread_mutex_unlock(&mut_strings);
            return strings.strings[i];
        }
    }

    if (strings.size >= strings.space) {
        strings.space += 64;
        strings.strings = sys_realloc(strings.strings, strings.space
                * sizeof(char *));
    }

    strings.strings[strings.size++] = sys_strdup(str);

    pthread_mutex_unlock(&mut_strings);
    return strings.strings[strings.size - 1];
}

/* Create a new allocation slot. */
static struct allocation *allocation_new()
{
    pthread_mutex_lock(&mut_allocs);

    if (allocs.size >= allocs.space) {
        allocs.space += 64;
        allocs.allocs = sys_realloc(allocs.allocs, allocs.space
                * sizeof(struct allocation *));
    }

    allocs.allocs[allocs.size++] = sys_calloc(sizeof(struct allocation), 1);
    pthread_mutex_unlock(&mut_allocs);
    return allocs.allocs[allocs.size - 1];
}

static struct allocation *allocation_find_by_ptr(void *ptr)
{
    if (!allocs.size)
        return NULL;

    size_t i = allocs.size;
    while (i--) {
        if (allocs.allocs[i]->ptr == ptr)
            return allocs.allocs[i];
    }

    return NULL;
}

/* Notify the user about the memory leak, printing all information stored in the
   allocation struct. */
static void notify_about_leak(struct allocation *alloc)
{
    fprintf(stderr, "\033[91mMemory leaked, \033[1;91m%zu bytes\033[0m "
            "allocated in \033[1;97m%s:\033[0m\n", alloc->size, alloc->func);

    print_source_code(alloc->file, alloc->line, alloc->ptr);
}

/* If we find the given file, we may use it to print the source code. */
static void print_source_code(char *file, int linenum, void *ptr)
{
    char line[LINESIZE];
    FILE *f;

    if (!(f = fopen(file, "r")))
        return;

    /* Fetch until the start line. */
    for (int i = 0; i < linenum - 2; i++)
        fgets(line, LINESIZE, f);

    fgets(line, LINESIZE, f);
    fprintf(stderr, "%s\n%4d | %s", file, linenum - 1, line);

    fgets(line, LINESIZE, f);
    line[strlen(line) - 1] = 0;
    fprintf(stderr, "\033[96m%4d | %s\033[90m // => %p\033[0m\n", linenum, line,
            ptr);

    fgets(line, LINESIZE, f);
    fprintf(stderr, "%4d | %s\n", linenum + 1, line);

    fclose(f);
}

/* Because we can't use the "malloc", ... names for the function calls, we need
   to load them ourselfs from libc. */
static void initialize()
{
    sys_free    = dlsym(NULL, "free");
    sys_malloc  = dlsym(NULL, "malloc");
    sys_calloc  = dlsym(NULL, "calloc");
    sys_realloc = dlsym(NULL, "realloc");
    sys_strdup  = dlsym(NULL, "strdup");

    if (!sys_free || !sys_malloc || !sys_calloc || !sys_realloc
        || !sys_strdup) {
        fprintf(stderr, "mleak: %s\n", dlerror());
        exit(1);
    }

    atexit(deconstruct);
    is_initialized = true;
}

/* Free all allocated memory by this library, and summarize all allocations. */
static void deconstruct()
{
    for (size_t i = 0; i < allocs.size; i++) {
        if (allocs.allocs[i]->type != ALLOC_FREE)
            notify_about_leak(allocs.allocs[i]);
        sys_free(allocs.allocs[i]);
    }
    sys_free(allocs.allocs);

    for (size_t i = 0; i < strings.size; i++)
        sys_free(strings.strings[i]);
    sys_free(strings.strings);
}
