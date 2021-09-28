/*
 * Copyright (c) 2013 Intel Corporation
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

/* Small wrapper around compiler specific implementation details of cpuid */

#ifndef IGT_X86_H
#define IGT_X86_H

#define MMX	0x1
#define SSE	0x2
#define SSE2	0x4
#define SSE3	0x8
#define SSSE3	0x10
#define SSE4_1	0x20
#define SSE4_2	0x40
#define AVX	0x80
#define AVX2	0x100
#define F16C	0x200

#if defined(__x86_64__) || defined(__i386__)
unsigned igt_x86_features(void);
char *igt_x86_features_to_string(unsigned features, char *line);
#else
static inline unsigned igt_x86_features(void)
{
	return 0;
}
static inline char *igt_x86_features_to_string(unsigned features, char *line)
{
	line[0] = 0;
	return line;
}
#endif

void igt_memcpy_from_wc(void *dst, const void *src, unsigned long len);

#endif /* IGT_X86_H */
