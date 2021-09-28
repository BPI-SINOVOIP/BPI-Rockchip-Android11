/*
 * Copyright 2018 Collabora Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ZINK_RENDERPASS_H
#define ZINK_RENDERPASS_H

#include <vulkan/vulkan.h>

#include "pipe/p_state.h"
#include "util/u_inlines.h"

struct zink_screen;

struct zink_rt_attrib {
  VkFormat format;
  VkSampleCountFlagBits samples;
};

struct zink_render_pass_state {
   uint8_t num_cbufs : 4; /* PIPE_MAX_COLOR_BUFS = 8 */
   uint8_t have_zsbuf : 1;
   struct zink_rt_attrib rts[PIPE_MAX_COLOR_BUFS + 1];
};

struct zink_render_pass {
   struct pipe_reference reference;

   VkRenderPass render_pass;
};

struct zink_render_pass *
zink_create_render_pass(struct zink_screen *screen,
                        struct zink_render_pass_state *state);

void
zink_destroy_render_pass(struct zink_screen *screen,
                         struct zink_render_pass *rp);

void
debug_describe_zink_render_pass(char* buf, const struct zink_render_pass *ptr);

static inline void
zink_render_pass_reference(struct zink_screen *screen,
                           struct zink_render_pass **dst,
                           struct zink_render_pass *src)
{
   struct zink_render_pass *old_dst = *dst;

   if (pipe_reference_described(&old_dst->reference, &src->reference,
                                (debug_reference_descriptor)debug_describe_zink_render_pass))
      zink_destroy_render_pass(screen, old_dst);
   *dst = src;
}

#endif
