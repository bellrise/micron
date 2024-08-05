/* micron.h - main header
   Copyright (c) 2024 bellrise */

#ifndef MICRON_H
#define MICRON_H 1

#include <micron/version.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uintptr_t uptr;
typedef size_t usize;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef intptr_t iptr;

#ifndef __unused
# define __unused __attribute__((unused))
#endif

#ifndef __maybe_unused
# define __maybe_unused __attribute__((maybe_unused))
#endif

#ifndef __packed
# define __packed __attribute__((packed))
#endif

#ifndef __aligned
# define __aligned(x) __attribute__((__aligned__(x)))
#endif

static inline i32 imax(i32 a, i32 b)
{
    return a >= b ? a : b;
}

static inline i32 imin(i32 a, i32 b)
{
    return a <= b ? a : b;
}

#endif /* MICRON_H */
