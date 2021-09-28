// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <lib/syslog/global.h>
#include <stdio.h>
#include <stdlib.h>

void __assert_fail(const char *expr, const char *file, int line, const char *func)
{
  FX_LOGF(ERROR, "goldfish", "Assertion failed: %s (%s: %s: %d)", expr, file, func, line);
  abort();
}

int puts(const char *s)
{
  return fputs(s, stdout);
}

int printf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stdout, format, args);
  va_end(args);
  return 0;
}

int vprintf(const char *format, va_list ap)
{
  return vfprintf(stdout, format, ap);
}

int fprintf(FILE *stream, const char *format, ...)
{
  assert(stream == stdout || stream == stderr);
  if (stream == stdout || stream == stderr)
  {
    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
  }
  return 0;
}

static inline fx_log_severity_t severity(FILE *stream)
{
  return stream == stdout ? FX_LOG_INFO : FX_LOG_ERROR;
}

int fputs(const char *s, FILE *stream)
{
  assert(stream == stdout || stream == stderr);
  if (stream == stdout || stream == stderr)
  {
    _FX_LOG(severity(stream), "goldfish", s);
  }
  return 0;
}

int vfprintf(FILE *stream, const char *format, va_list ap)
{
  assert(stream == stdout || stream == stderr);
  if (stream == stdout || stream == stderr)
  {
    _FX_LOGVF(severity(stream), "goldfish", format, ap);
  }
  return 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream)
{
  assert(stream == stdout || stream == stderr);
  char buffer[512];
  size_t offset = 0;
  size_t count = 0;
  for (; count < nitems; count++)
  {
    snprintf(buffer + offset, sizeof(buffer) - offset, reinterpret_cast<const char *>(ptr) + offset, size);
    offset += size;
    if (offset > sizeof(buffer))
      break;
  }
  buffer[sizeof(buffer) - 1] = 0;
  fputs(buffer, stream);
  return count;
}
