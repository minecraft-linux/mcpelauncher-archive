#include <android/versioning.h>
#include <string.h>
#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE - 1))
#ifndef __APPLE__
#define DEF_WEAK(a)
#define __BIONIC_ALIGN(__value, __alignment) (((__value) + (__alignment)-1) & ~((__alignment)-1))
#define	__predict_true(exp)	__builtin_expect((exp) != 0, 1)
#define	__predict_false(exp)	__builtin_expect((exp) != 0, 0)
#include <features.h>
#if defined(__GLIBC__) && (__GLIBC__ < 2 || __GLIBC__ == 2 && __GLIBC_MINOR__ < 30)
#include <unistd.h>
#include <sys/syscall.h>
#ifdef SYS_gettid
#define gettid() syscall(SYS_gettid)
#else
#define gettid() 0
#endif
#endif
extern
#ifdef __cplusplus
"C"
#endif
size_t strlcpy(char *dst, const char *src, size_t dsize);
extern
#ifdef __cplusplus
"C"
#endif
size_t strlcat(char *dst, const char *src, size_t dsize);
#else
#define gettid() 0
#define __assert __loader_assert
extern 
#ifdef __cplusplus
"C"
#endif
void __loader_assert(const char* file, int line, const char* msg);
/* Used to retry syscalls that can return EINTR. */
#define TEMP_FAILURE_RETRY(exp) ({         \
    __typeof__(exp) _rc;                   \
    do {                                   \
        _rc = (exp);                       \
    } while (_rc == -1 && errno == EINTR); \
    _rc; })

#include <stdio.h>
#include <sys/types.h>
#include "../core/base/include/android-base/off64_t.h"
#  define fopen64 fopen
#  define mmap64 mmap
#  define pread64 pread
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#define __LIBC_HIDDEN__
#include <libgen.h>
#define basename(name) basename((char*)name)
// Returns the address of the page containing address 'x'.
#define PAGE_START(x) ((x) & PAGE_MASK)

// Returns the offset of address 'x' in its page.
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)

// Returns the address of the next page after address 'x', unless 'x' is
// itself at the start of a page.
#define PAGE_END(x) PAGE_START((x) + (PAGE_SIZE-1))
#endif
#ifndef __GLIBC__
#define _GNU_SOURCE
#include <pthread.h>
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP {{PTHREAD_MUTEX_RECURSIVE}}
#include <libgen.h>
#define basename(name) basename((char*)name)
#define getprogname() "unknown"
/* Used to retry syscalls that can return EINTR. */
#define TEMP_FAILURE_RETRY(exp) ({         \
    __typeof__(exp) _rc;                   \
    do {                                   \
        _rc = (exp);                       \
    } while (_rc == -1 && errno == EINTR); \
    _rc; })
#define __assert __loader_assert
extern
#ifdef __cplusplus
"C"
#endif
void __loader_assert(const char* file, int line, const char* msg);
#endif
