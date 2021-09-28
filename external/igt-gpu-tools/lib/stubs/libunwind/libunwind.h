/*
 * Copyright © 2018 Intel Corporation
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
 * Authors:
 *    Daniel Vetter <daniel.vetter@ffwll.ch>
 */

/* dummy libunwind header with stubs, only the minimal amount we need to get by */


typedef int unw_cursor_t;
typedef int unw_context_t;
typedef int unw_word_t;

#define UNW_REG_IP 0

static inline void unw_getcontext(unw_context_t *uc) {}
static inline void unw_init_local(unw_cursor_t *cursor, unw_context_t *uc) {}
static inline int unw_step(unw_cursor_t *cursor) { return 0; }
static inline int unw_get_reg (unw_cursor_t *cursor, int i, unw_word_t * word)
{
	return 0;
}
static inline int unw_get_proc_name (unw_cursor_t *cursor, char *c, size_t s, unw_word_t *word)
{
	return 0;
}
