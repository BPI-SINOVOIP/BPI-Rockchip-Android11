/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Utility functions and macros for the 'fsverity' program
 *
 * Copyright (C) 2018 Google LLC
 */
#ifndef UTIL_H
#define UTIL_H

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#ifndef __force
#  ifdef __CHECKER__
#    define __force	__attribute__((force))
#  else
#    define __force
#  endif
#endif

#ifndef __printf
#  define __printf(fmt_idx, vargs_idx) \
	__attribute__((format(printf, fmt_idx, vargs_idx)))
#endif

#ifndef __noreturn
#  define __noreturn	__attribute__((noreturn))
#endif

#ifndef __cold
#  define __cold	__attribute__((cold))
#endif

#define min(a, b) ({			\
	__typeof__(a) _a = (a);		\
	__typeof__(b) _b = (b);		\
	_a < _b ? _a : _b;		\
})
#define max(a, b) ({			\
	__typeof__(a) _a = (a);		\
	__typeof__(b) _b = (b);		\
	_a > _b ? _a : _b;		\
})

#define roundup(x, y) ({		\
	__typeof__(y) _y = (y);		\
	(((x) + _y - 1) / _y) * _y;	\
})

#define ARRAY_SIZE(A)		(sizeof(A) / sizeof((A)[0]))

#define DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))

static inline bool is_power_of_2(unsigned long n)
{
	return n != 0 && ((n & (n - 1)) == 0);
}

static inline int ilog2(unsigned long n)
{
	return (8 * sizeof(n) - 1) - __builtin_clzl(n);
}

/* ========== Endianness conversion ========== */

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define cpu_to_le16(v)	((__force __le16)(u16)(v))
#  define le16_to_cpu(v)	((__force u16)(__le16)(v))
#  define cpu_to_le32(v)	((__force __le32)(u32)(v))
#  define le32_to_cpu(v)	((__force u32)(__le32)(v))
#  define cpu_to_le64(v)	((__force __le64)(u64)(v))
#  define le64_to_cpu(v)	((__force u64)(__le64)(v))
#else
#  define cpu_to_le16(v)	((__force __le16)__builtin_bswap16(v))
#  define le16_to_cpu(v)	(__builtin_bswap16((__force u16)(v)))
#  define cpu_to_le32(v)	((__force __le32)__builtin_bswap32(v))
#  define le32_to_cpu(v)	(__builtin_bswap32((__force u32)(v)))
#  define cpu_to_le64(v)	((__force __le64)__builtin_bswap64(v))
#  define le64_to_cpu(v)	(__builtin_bswap64((__force u64)(v)))
#endif

/* ========== Memory allocation ========== */

void *xmalloc(size_t size);
void *xzalloc(size_t size);
void *xmemdup(const void *mem, size_t size);
char *xstrdup(const char *s);

/* ========== Error messages and assertions ========== */

__cold void do_error_msg(const char *format, va_list va, int err);
__printf(1, 2) __cold void error_msg(const char *format, ...);
__printf(1, 2) __cold void error_msg_errno(const char *format, ...);
__printf(1, 2) __cold __noreturn void fatal_error(const char *format, ...);
__cold __noreturn void assertion_failed(const char *expr,
					const char *file, int line);

#define ASSERT(e) ({ if (!(e)) assertion_failed(#e, __FILE__, __LINE__); })

/* ========== File utilities ========== */

struct filedes {
	int fd;
	char *name;		/* filename, for logging or error messages */
};

bool open_file(struct filedes *file, const char *filename, int flags, int mode);
bool get_file_size(struct filedes *file, u64 *size_ret);
bool full_read(struct filedes *file, void *buf, size_t count);
bool full_write(struct filedes *file, const void *buf, size_t count);
bool filedes_close(struct filedes *file);

/* ========== String utilities ========== */

bool hex2bin(const char *hex, u8 *bin, size_t bin_len);
void bin2hex(const u8 *bin, size_t bin_len, char *hex);

#endif /* UTIL_H */
