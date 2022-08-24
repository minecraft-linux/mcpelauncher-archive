#pragma once
#ifndef __has_feature
#define __has_feature(...) 0
#endif
#ifndef __has_builtin
#define __has_builtin(...) 0
#define __builtin_umul_overflow
#endif
#ifndef __APPLE__
#include "../../bionic/libc/include/sys/cdefs.h"
#endif
#include_next <sys/cdefs.h>
#ifdef __BIONIC__
#undef __BIONIC__
#endif
