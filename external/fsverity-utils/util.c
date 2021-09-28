// SPDX-License-Identifier: GPL-2.0+
/*
 * Utility functions for the 'fsverity' program
 *
 * Copyright (C) 2018 Google LLC
 *
 * Written by Eric Biggers.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

/* ========== Memory allocation ========== */

void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if (!p)
		fatal_error("out of memory");
	return p;
}

void *xzalloc(size_t size)
{
	return memset(xmalloc(size), 0, size);
}

void *xmemdup(const void *mem, size_t size)
{
	return memcpy(xmalloc(size), mem, size);
}

char *xstrdup(const char *s)
{
	return xmemdup(s, strlen(s) + 1);
}

/* ========== Error messages and assertions ========== */

void do_error_msg(const char *format, va_list va, int err)
{
	fputs("ERROR: ", stderr);
	vfprintf(stderr, format, va);
	if (err)
		fprintf(stderr, ": %s", strerror(err));
	putc('\n', stderr);
}

void error_msg(const char *format, ...)
{
	va_list va;

	va_start(va, format);
	do_error_msg(format, va, 0);
	va_end(va);
}

void error_msg_errno(const char *format, ...)
{
	va_list va;

	va_start(va, format);
	do_error_msg(format, va, errno);
	va_end(va);
}

__noreturn void fatal_error(const char *format, ...)
{
	va_list va;

	va_start(va, format);
	do_error_msg(format, va, 0);
	va_end(va);
	abort();
}

__noreturn void assertion_failed(const char *expr, const char *file, int line)
{
	fatal_error("Assertion failed: %s at %s:%d", expr, file, line);
}

/* ========== File utilities ========== */

bool open_file(struct filedes *file, const char *filename, int flags, int mode)
{
	file->fd = open(filename, flags, mode);
	if (file->fd < 0) {
		error_msg_errno("can't open '%s' for %s", filename,
				(flags & O_ACCMODE) == O_RDONLY ? "reading" :
				(flags & O_ACCMODE) == O_WRONLY ? "writing" :
				"reading and writing");
		return false;
	}
	file->name = xstrdup(filename);
	return true;
}

bool get_file_size(struct filedes *file, u64 *size_ret)
{
	struct stat stbuf;

	if (fstat(file->fd, &stbuf) != 0) {
		error_msg_errno("can't stat file '%s'", file->name);
		return false;
	}
	*size_ret = stbuf.st_size;
	return true;
}

bool full_read(struct filedes *file, void *buf, size_t count)
{
	while (count) {
		int n = read(file->fd, buf, min(count, INT_MAX));

		if (n < 0) {
			error_msg_errno("reading from '%s'", file->name);
			return false;
		}
		if (n == 0) {
			error_msg("unexpected end-of-file on '%s'", file->name);
			return false;
		}
		buf += n;
		count -= n;
	}
	return true;
}

bool full_write(struct filedes *file, const void *buf, size_t count)
{
	while (count) {
		int n = write(file->fd, buf, min(count, INT_MAX));

		if (n < 0) {
			error_msg_errno("writing to '%s'", file->name);
			return false;
		}
		buf += n;
		count -= n;
	}
	return true;
}

bool filedes_close(struct filedes *file)
{
	int res;

	if (file->fd < 0)
		return true;
	res = close(file->fd);
	if (res != 0)
		error_msg_errno("closing '%s'", file->name);
	file->fd = -1;
	free(file->name);
	file->name = NULL;
	return res == 0;
}

/* ========== String utilities ========== */

static int hex2bin_char(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return 10 + (c - 'a');
	if (c >= 'A' && c <= 'F')
		return 10 + (c - 'A');
	return -1;
}

bool hex2bin(const char *hex, u8 *bin, size_t bin_len)
{
	if (strlen(hex) != 2 * bin_len)
		return false;

	while (bin_len--) {
		int hi = hex2bin_char(*hex++);
		int lo = hex2bin_char(*hex++);

		if (hi < 0 || lo < 0)
			return false;
		*bin++ = (hi << 4) | lo;
	}
	return true;
}

static char bin2hex_char(u8 nibble)
{
	ASSERT(nibble <= 0xf);

	if (nibble < 10)
		return '0' + nibble;
	return 'a' + (nibble - 10);
}

void bin2hex(const u8 *bin, size_t bin_len, char *hex)
{
	while (bin_len--) {
		*hex++ = bin2hex_char(*bin >> 4);
		*hex++ = bin2hex_char(*bin & 0xf);
		bin++;
	}
	*hex = '\0';
}
