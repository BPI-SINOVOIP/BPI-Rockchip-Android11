/*
 * Copyright © 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef __IGT_SYSFS_H__
#define __IGT_SYSFS_H__

#include <stdbool.h>
#include <stdarg.h>

char *igt_sysfs_path(int device, char *path, int pathlen);
int igt_sysfs_open(int device);
int igt_sysfs_open_parameters(int device);
bool igt_sysfs_set_parameter(int device,
			     const char *parameter,
			     const char *fmt, ...)
	__attribute__((format(printf,3,4)));

int igt_sysfs_read(int dir, const char *attr, void *data, int len);
int igt_sysfs_write(int dir, const char *attr, const void *data, int len);

bool igt_sysfs_set(int dir, const char *attr, const char *value);
char *igt_sysfs_get(int dir, const char *attr);

int igt_sysfs_scanf(int dir, const char *attr, const char *fmt, ...)
	__attribute__((format(scanf,3,4)));
int igt_sysfs_vprintf(int dir, const char *attr, const char *fmt, va_list ap)
	__attribute__((format(printf,3,0)));
int igt_sysfs_printf(int dir, const char *attr, const char *fmt, ...)
	__attribute__((format(printf,3,4)));

uint32_t igt_sysfs_get_u32(int dir, const char *attr);
bool igt_sysfs_set_u32(int dir, const char *attr, uint32_t value);

bool igt_sysfs_get_boolean(int dir, const char *attr);
bool igt_sysfs_set_boolean(int dir, const char *attr, bool value);

void bind_fbcon(bool enable);
void kick_snd_hda_intel(void);
void fbcon_blink_enable(bool enable);

#endif /* __IGT_SYSFS_H__ */
