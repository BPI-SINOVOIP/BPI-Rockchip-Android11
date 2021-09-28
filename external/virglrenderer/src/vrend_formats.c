/**************************************************************************
 *
 * Copyright (C) 2014 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
#include <epoxy/gl.h>

#include "vrend_renderer.h"
#include "util/u_memory.h"
#include "util/u_format.h"

#define SWIZZLE_INVALID 0xff
#define NO_SWIZZLE { SWIZZLE_INVALID, SWIZZLE_INVALID, SWIZZLE_INVALID, SWIZZLE_INVALID }
#define RRR1_SWIZZLE { PIPE_SWIZZLE_RED, PIPE_SWIZZLE_RED, PIPE_SWIZZLE_RED, PIPE_SWIZZLE_ONE }
#define RGB1_SWIZZLE { PIPE_SWIZZLE_RED, PIPE_SWIZZLE_GREEN, PIPE_SWIZZLE_BLUE, PIPE_SWIZZLE_ONE }

#ifdef __GNUC__
/* The warning missing-field-initializers is misleading: If at least one field
 * is initialized, then the un-initialized fields will be filled with zero.
 * Silencing the warning by manually adding the zeros that the compiler will add
 * anyway doesn't improve the code, and initializing the files by using a named
 * notation will make it worse, because then he remaining fields truely be
 * un-initialized.
 */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#else
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#endif

/* fill the format table */
static struct vrend_format_table base_rgba_formats[] =
  {
    { VIRGL_FORMAT_B8G8R8X8_UNORM, GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, RGB1_SWIZZLE },
    { VIRGL_FORMAT_B8G8R8A8_UNORM, GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, NO_SWIZZLE },

    { VIRGL_FORMAT_R8G8B8X8_UNORM, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, RGB1_SWIZZLE },
    { VIRGL_FORMAT_R8G8B8A8_UNORM, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },

    { VIRGL_FORMAT_A8R8G8B8_UNORM, GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, NO_SWIZZLE },
    { VIRGL_FORMAT_X8R8G8B8_UNORM, GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, NO_SWIZZLE },

    { VIRGL_FORMAT_A8B8G8R8_UNORM, GL_RGBA8, GL_ABGR_EXT, GL_UNSIGNED_BYTE, NO_SWIZZLE },

    { VIRGL_FORMAT_B4G4R4X4_UNORM, GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV, RGB1_SWIZZLE },
    { VIRGL_FORMAT_A4B4G4R4_UNORM, GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NO_SWIZZLE },
    { VIRGL_FORMAT_B5G5R5X1_UNORM, GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, RGB1_SWIZZLE },

    { VIRGL_FORMAT_B5G6R5_UNORM, GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NO_SWIZZLE },
    { VIRGL_FORMAT_B2G3R3_UNORM, GL_R3_G3_B2, GL_RGB, GL_UNSIGNED_BYTE_3_3_2, NO_SWIZZLE },

    { VIRGL_FORMAT_R16G16B16X16_UNORM, GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, RGB1_SWIZZLE },

    { VIRGL_FORMAT_R16G16B16A16_UNORM, GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, NO_SWIZZLE },
  };

static struct vrend_format_table gl_base_rgba_formats[] =
  {
    { VIRGL_FORMAT_B4G4R4A4_UNORM, GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV, NO_SWIZZLE },
    { VIRGL_FORMAT_B5G5R5A1_UNORM, GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NO_SWIZZLE },
  };

static struct vrend_format_table base_depth_formats[] =
  {
    { VIRGL_FORMAT_Z16_UNORM, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NO_SWIZZLE },
    { VIRGL_FORMAT_Z32_UNORM, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NO_SWIZZLE },
    { VIRGL_FORMAT_S8_UINT_Z24_UNORM, GL_DEPTH24_STENCIL8_EXT, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NO_SWIZZLE },
    { VIRGL_FORMAT_Z24X8_UNORM, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NO_SWIZZLE },
    { VIRGL_FORMAT_Z32_FLOAT, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, NO_SWIZZLE },
    /* this is probably a separate format */
    { VIRGL_FORMAT_Z32_FLOAT_S8X24_UINT, GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NO_SWIZZLE },
    { VIRGL_FORMAT_X24S8_UINT, GL_STENCIL_INDEX8, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  };

static struct vrend_format_table base_la_formats[] = {
  { VIRGL_FORMAT_A8_UNORM, GL_ALPHA8, GL_ALPHA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_L8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L8A8_UNORM, GL_LUMINANCE8_ALPHA8, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_A16_UNORM, GL_ALPHA16, GL_ALPHA, GL_UNSIGNED_SHORT, NO_SWIZZLE },
  { VIRGL_FORMAT_L16_UNORM, GL_R16, GL_RED, GL_UNSIGNED_SHORT, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L16A16_UNORM, GL_LUMINANCE16_ALPHA16, GL_LUMINANCE_ALPHA, GL_UNSIGNED_SHORT, NO_SWIZZLE },
};

static struct vrend_format_table rg_base_formats[] = {
  { VIRGL_FORMAT_R8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_R8G8_UNORM, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_R16_UNORM, GL_R16, GL_RED, GL_UNSIGNED_SHORT, NO_SWIZZLE },
  { VIRGL_FORMAT_R16G16_UNORM, GL_RG16, GL_RG, GL_UNSIGNED_SHORT, NO_SWIZZLE },
};

static struct vrend_format_table integer_base_formats[] = {
  { VIRGL_FORMAT_R8G8B8A8_UINT, GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_R8G8B8A8_SINT, GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, NO_SWIZZLE },

  { VIRGL_FORMAT_R16G16B16A16_UINT, GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, NO_SWIZZLE },
  { VIRGL_FORMAT_R16G16B16A16_SINT, GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, NO_SWIZZLE },

  { VIRGL_FORMAT_R32G32B32A32_UINT, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, NO_SWIZZLE },
  { VIRGL_FORMAT_R32G32B32A32_SINT, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT, NO_SWIZZLE },
};

static struct vrend_format_table integer_3comp_formats[] = {
  { VIRGL_FORMAT_R8G8B8X8_UINT, GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, RGB1_SWIZZLE },
  { VIRGL_FORMAT_R8G8B8X8_SINT, GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, RGB1_SWIZZLE },
  { VIRGL_FORMAT_R16G16B16X16_UINT, GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, RGB1_SWIZZLE },
  { VIRGL_FORMAT_R16G16B16X16_SINT, GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, RGB1_SWIZZLE },
  { VIRGL_FORMAT_R32G32B32_UINT, GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, NO_SWIZZLE },
  { VIRGL_FORMAT_R32G32B32_SINT, GL_RGB32I, GL_RGB_INTEGER, GL_INT, NO_SWIZZLE },
};

static struct vrend_format_table float_base_formats[] = {
  { VIRGL_FORMAT_R16G16B16A16_FLOAT, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, NO_SWIZZLE },
  { VIRGL_FORMAT_R32G32B32A32_FLOAT, GL_RGBA32F, GL_RGBA, GL_FLOAT, NO_SWIZZLE },
};

static struct vrend_format_table float_la_formats[] = {
  { VIRGL_FORMAT_A16_FLOAT, GL_ALPHA16F_ARB, GL_ALPHA, GL_HALF_FLOAT, NO_SWIZZLE },
  { VIRGL_FORMAT_L16_FLOAT, GL_R16F, GL_RED, GL_HALF_FLOAT, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L16A16_FLOAT, GL_LUMINANCE_ALPHA16F_ARB, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT, NO_SWIZZLE },

  { VIRGL_FORMAT_A32_FLOAT, GL_ALPHA32F_ARB, GL_ALPHA, GL_FLOAT, NO_SWIZZLE },
  { VIRGL_FORMAT_L32_FLOAT, GL_R32F, GL_RED, GL_FLOAT, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L32A32_FLOAT, GL_LUMINANCE_ALPHA32F_ARB, GL_LUMINANCE_ALPHA, GL_FLOAT, NO_SWIZZLE },
};

static struct vrend_format_table integer_rg_formats[] = {
  { VIRGL_FORMAT_R8_UINT, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_R8G8_UINT, GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_R8_SINT, GL_R8I, GL_RED_INTEGER, GL_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_R8G8_SINT, GL_RG8I, GL_RG_INTEGER, GL_BYTE, NO_SWIZZLE },

  { VIRGL_FORMAT_R16_UINT, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, NO_SWIZZLE },
  { VIRGL_FORMAT_R16G16_UINT, GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, NO_SWIZZLE },
  { VIRGL_FORMAT_R16_SINT, GL_R16I, GL_RED_INTEGER, GL_SHORT, NO_SWIZZLE },
  { VIRGL_FORMAT_R16G16_SINT, GL_RG16I, GL_RG_INTEGER, GL_SHORT, NO_SWIZZLE },

  { VIRGL_FORMAT_R32_UINT, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, NO_SWIZZLE },
  { VIRGL_FORMAT_R32G32_UINT, GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, NO_SWIZZLE },
  { VIRGL_FORMAT_R32_SINT, GL_R32I, GL_RED_INTEGER, GL_INT, NO_SWIZZLE },
  { VIRGL_FORMAT_R32G32_SINT, GL_RG32I, GL_RG_INTEGER, GL_INT, NO_SWIZZLE },
};

static struct vrend_format_table float_rg_formats[] = {
  { VIRGL_FORMAT_R16_FLOAT, GL_R16F, GL_RED, GL_HALF_FLOAT, NO_SWIZZLE },
  { VIRGL_FORMAT_R16G16_FLOAT, GL_RG16F, GL_RG, GL_HALF_FLOAT, NO_SWIZZLE },
  { VIRGL_FORMAT_R32_FLOAT, GL_R32F, GL_RED, GL_FLOAT, NO_SWIZZLE },
  { VIRGL_FORMAT_R32G32_FLOAT, GL_RG32F, GL_RG, GL_FLOAT, NO_SWIZZLE },
};

static struct vrend_format_table float_3comp_formats[] = {
  { VIRGL_FORMAT_R16G16B16X16_FLOAT, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, RGB1_SWIZZLE },
  { VIRGL_FORMAT_R32G32B32_FLOAT, GL_RGB32F, GL_RGB, GL_FLOAT, NO_SWIZZLE },
};


static struct vrend_format_table integer_la_formats[] = {
  { VIRGL_FORMAT_A8_UINT, GL_ALPHA8UI_EXT, GL_ALPHA_INTEGER, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_L8_UINT, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L8A8_UINT, GL_LUMINANCE_ALPHA8UI_EXT, GL_LUMINANCE_ALPHA_INTEGER_EXT, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_A8_SINT, GL_ALPHA8I_EXT, GL_ALPHA_INTEGER, GL_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_L8_SINT, GL_R8I, GL_RED_INTEGER, GL_BYTE, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L8A8_SINT, GL_LUMINANCE_ALPHA8I_EXT, GL_LUMINANCE_ALPHA_INTEGER_EXT, GL_BYTE, NO_SWIZZLE },

  { VIRGL_FORMAT_A16_UINT, GL_ALPHA16UI_EXT, GL_ALPHA_INTEGER, GL_UNSIGNED_SHORT, NO_SWIZZLE },
  { VIRGL_FORMAT_L16_UINT, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L16A16_UINT, GL_LUMINANCE_ALPHA16UI_EXT, GL_LUMINANCE_ALPHA_INTEGER_EXT, GL_UNSIGNED_SHORT, NO_SWIZZLE },

  { VIRGL_FORMAT_A16_SINT, GL_ALPHA16I_EXT, GL_ALPHA_INTEGER, GL_SHORT, NO_SWIZZLE },
  { VIRGL_FORMAT_L16_SINT, GL_R16I, GL_RED_INTEGER, GL_SHORT, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L16A16_SINT, GL_LUMINANCE_ALPHA16I_EXT, GL_LUMINANCE_ALPHA_INTEGER_EXT, GL_SHORT, NO_SWIZZLE },

  { VIRGL_FORMAT_A32_UINT, GL_ALPHA32UI_EXT, GL_ALPHA_INTEGER, GL_UNSIGNED_INT, NO_SWIZZLE },
  { VIRGL_FORMAT_L32_UINT, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L32A32_UINT, GL_LUMINANCE_ALPHA32UI_EXT, GL_LUMINANCE_ALPHA_INTEGER_EXT, GL_UNSIGNED_INT, NO_SWIZZLE },

  { VIRGL_FORMAT_A32_SINT, GL_ALPHA32I_EXT, GL_ALPHA_INTEGER, GL_INT, NO_SWIZZLE },
  { VIRGL_FORMAT_L32_SINT, GL_R32I, GL_RED_INTEGER, GL_INT, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L32A32_SINT, GL_LUMINANCE_ALPHA32I_EXT, GL_LUMINANCE_ALPHA_INTEGER_EXT, GL_INT, NO_SWIZZLE },


};

static struct vrend_format_table snorm_formats[] = {
  { VIRGL_FORMAT_R8_SNORM, GL_R8_SNORM, GL_RED, GL_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_R8G8_SNORM, GL_RG8_SNORM, GL_RG, GL_BYTE, NO_SWIZZLE },

  { VIRGL_FORMAT_R8G8B8A8_SNORM, GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_R8G8B8X8_SNORM, GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, RGB1_SWIZZLE },

  { VIRGL_FORMAT_R16_SNORM, GL_R16_SNORM, GL_RED, GL_SHORT, NO_SWIZZLE },
  { VIRGL_FORMAT_R16G16_SNORM, GL_RG16_SNORM, GL_RG, GL_SHORT, NO_SWIZZLE },
  { VIRGL_FORMAT_R16G16B16A16_SNORM, GL_RGBA16_SNORM, GL_RGBA, GL_SHORT, NO_SWIZZLE },

  { VIRGL_FORMAT_R16G16B16X16_SNORM, GL_RGBA16_SNORM, GL_RGBA, GL_SHORT, RGB1_SWIZZLE },
};

static struct vrend_format_table snorm_la_formats[] = {
  { VIRGL_FORMAT_A8_SNORM, GL_ALPHA8_SNORM, GL_ALPHA, GL_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_L8_SNORM, GL_R8_SNORM, GL_RED, GL_BYTE, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L8A8_SNORM, GL_LUMINANCE8_ALPHA8_SNORM, GL_LUMINANCE_ALPHA, GL_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_A16_SNORM, GL_ALPHA16_SNORM, GL_ALPHA, GL_SHORT, NO_SWIZZLE },
  { VIRGL_FORMAT_L16_SNORM, GL_R16_SNORM, GL_RED, GL_SHORT, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L16A16_SNORM, GL_LUMINANCE16_ALPHA16_SNORM, GL_LUMINANCE_ALPHA, GL_SHORT, NO_SWIZZLE },
};

static struct vrend_format_table dxtn_formats[] = {
  { VIRGL_FORMAT_DXT1_RGB, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_RGB, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_DXT1_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_DXT3_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_DXT5_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
};

static struct vrend_format_table dxtn_srgb_formats[] = {
  { VIRGL_FORMAT_DXT1_SRGB, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, GL_RGB, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_DXT1_SRGBA, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_DXT3_SRGBA, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_DXT5_SRGBA, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
};

static struct vrend_format_table rgtc_formats[] = {
  { VIRGL_FORMAT_RGTC1_UNORM, GL_COMPRESSED_RED_RGTC1, GL_RED, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_RGTC1_SNORM, GL_COMPRESSED_SIGNED_RED_RGTC1, GL_RED, GL_BYTE, NO_SWIZZLE },

  { VIRGL_FORMAT_RGTC2_UNORM, GL_COMPRESSED_RG_RGTC2, GL_RG, GL_UNSIGNED_BYTE, NO_SWIZZLE },
  { VIRGL_FORMAT_RGTC2_SNORM, GL_COMPRESSED_SIGNED_RG_RGTC2, GL_RG, GL_BYTE, NO_SWIZZLE },
};

static struct vrend_format_table srgb_formats[] = {
  { VIRGL_FORMAT_R8G8B8X8_SRGB, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, RGB1_SWIZZLE },
  { VIRGL_FORMAT_R8G8B8A8_SRGB, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },

  { VIRGL_FORMAT_L8_SRGB, GL_SR8_EXT, GL_RED, GL_UNSIGNED_BYTE, RRR1_SWIZZLE },
  { VIRGL_FORMAT_L8A8_SRGB, GL_SLUMINANCE8_ALPHA8_EXT, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
};

static struct vrend_format_table gl_srgb_formats[] =
{
  { VIRGL_FORMAT_B8G8R8X8_SRGB, GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE, RGB1_SWIZZLE },
  { VIRGL_FORMAT_B8G8R8A8_SRGB, GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
};

static struct vrend_format_table bit10_formats[] = {
  { VIRGL_FORMAT_B10G10R10X2_UNORM, GL_RGB10_A2, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV, RGB1_SWIZZLE },
  { VIRGL_FORMAT_B10G10R10A2_UNORM, GL_RGB10_A2, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV, NO_SWIZZLE },
  { VIRGL_FORMAT_B10G10R10A2_UINT, GL_RGB10_A2UI, GL_BGRA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, NO_SWIZZLE },
  { VIRGL_FORMAT_R10G10B10X2_UNORM, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, RGB1_SWIZZLE },
  { VIRGL_FORMAT_R10G10B10A2_UNORM, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, NO_SWIZZLE },
  { VIRGL_FORMAT_R10G10B10A2_UINT, GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, NO_SWIZZLE },
};

static struct vrend_format_table packed_float_formats[] = {
  { VIRGL_FORMAT_R11G11B10_FLOAT, GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, NO_SWIZZLE },
};

static struct vrend_format_table exponent_float_formats[] = {
  { VIRGL_FORMAT_R9G9B9E5_FLOAT, GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, NO_SWIZZLE },
};

static struct vrend_format_table bptc_formats[] = {
   { VIRGL_FORMAT_BPTC_RGBA_UNORM, GL_COMPRESSED_RGBA_BPTC_UNORM, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_BPTC_SRGBA, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_BPTC_RGB_FLOAT, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT, GL_RGB, GL_UNSIGNED_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_BPTC_RGB_UFLOAT, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, GL_RGB, GL_UNSIGNED_BYTE, NO_SWIZZLE },
};

static struct vrend_format_table gles_bgra_formats[] = {
  { VIRGL_FORMAT_B8G8R8X8_UNORM, GL_BGRA_EXT, GL_BGRA_EXT, GL_UNSIGNED_BYTE, RGB1_SWIZZLE },
  { VIRGL_FORMAT_B8G8R8A8_UNORM, GL_BGRA_EXT, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NO_SWIZZLE },
};

static struct vrend_format_table gles_z32_format[] = {
  { VIRGL_FORMAT_Z32_UNORM, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NO_SWIZZLE },
};

static void vrend_add_formats(struct vrend_format_table *table, int num_entries)
{
  int i;
  uint32_t binding = 0;
  GLuint buffers;
  GLuint tex_id, fb_id;

  for (i = 0; i < num_entries; i++) {
    GLenum status;
    bool is_depth = false;
    /**/
    glGenTextures(1, &tex_id);
    glGenFramebuffers(1, &fb_id);

    glBindTexture(GL_TEXTURE_2D, tex_id);
    glBindFramebuffer(GL_FRAMEBUFFER, fb_id);

    glTexImage2D(GL_TEXTURE_2D, 0, table[i].internalformat, 32, 32, 0, table[i].glformat, table[i].gltype, NULL);
    status = glGetError();
    if (status == GL_INVALID_VALUE || status == GL_INVALID_ENUM || status == GL_INVALID_OPERATION) {
      struct vrend_format_table *entry = NULL;
      uint8_t swizzle[4];
      binding = VIRGL_BIND_SAMPLER_VIEW | VIRGL_BIND_RENDER_TARGET | VIRGL_BIND_NEED_SWIZZLE;

      switch (table[i].format) {
      case PIPE_FORMAT_A8_UNORM:
        entry = &rg_base_formats[0];
        swizzle[0] = swizzle[1] = swizzle[2] = PIPE_SWIZZLE_ZERO;
        swizzle[3] = PIPE_SWIZZLE_RED;
        break;
      case PIPE_FORMAT_A16_UNORM:
        entry = &rg_base_formats[2];
        swizzle[0] = swizzle[1] = swizzle[2] = PIPE_SWIZZLE_ZERO;
        swizzle[3] = PIPE_SWIZZLE_RED;
        break;
      default:
        break;
      }

      if (entry) {
        vrend_insert_format_swizzle(table[i].format, entry, binding, swizzle);
      }
      glDeleteTextures(1, &tex_id);
      glDeleteFramebuffers(1, &fb_id);
      continue;
    }

    if (util_format_is_depth_or_stencil(table[i].format)) {
      GLenum attachment;

      if (table[i].format == VIRGL_FORMAT_Z24X8_UNORM || table[i].format == VIRGL_FORMAT_Z32_UNORM || table[i].format == VIRGL_FORMAT_Z16_UNORM || table[i].format == VIRGL_FORMAT_Z32_FLOAT)
        attachment = GL_DEPTH_ATTACHMENT;
      else
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;
      glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, attachment, GL_TEXTURE_2D, tex_id, 0);

      is_depth = true;

      buffers = GL_NONE;
      glDrawBuffers(1, &buffers);
    } else {
      glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, tex_id, 0);

      buffers = GL_COLOR_ATTACHMENT0_EXT;
      glDrawBuffers(1, &buffers);
    }

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    binding = VIRGL_BIND_SAMPLER_VIEW;
    if (status == GL_FRAMEBUFFER_COMPLETE)
      binding |= (is_depth ? VIRGL_BIND_DEPTH_STENCIL : VIRGL_BIND_RENDER_TARGET);

    glDeleteTextures(1, &tex_id);
    glDeleteFramebuffers(1, &fb_id);

    if (table[i].swizzle[0] != SWIZZLE_INVALID)
        vrend_insert_format_swizzle(table[i].format, &table[i], binding, table[i].swizzle);
    else
        vrend_insert_format(&table[i], binding);
  }
}

#define add_formats(x) vrend_add_formats((x), ARRAY_SIZE((x)))

void vrend_build_format_list_common(void)
{
  add_formats(base_rgba_formats);
  add_formats(base_depth_formats);
  add_formats(base_la_formats);

  /* float support */
  add_formats(float_base_formats);
  add_formats(float_la_formats);
  add_formats(float_3comp_formats);

  /* texture integer support ? */
  add_formats(integer_base_formats);
  add_formats(integer_la_formats);
  add_formats(integer_3comp_formats);

  /* RG support? */
  add_formats(rg_base_formats);
  /* integer + rg */
  add_formats(integer_rg_formats);
  /* float + rg */
  add_formats(float_rg_formats);

  /* snorm */
  add_formats(snorm_formats);
  add_formats(snorm_la_formats);

  /* compressed */
  add_formats(rgtc_formats);
  add_formats(dxtn_formats);
  add_formats(dxtn_srgb_formats);

  add_formats(srgb_formats);

  add_formats(bit10_formats);

  add_formats(packed_float_formats);
  add_formats(exponent_float_formats);

  add_formats(bptc_formats);
}


void vrend_build_format_list_gl(void)
{
  /* GL_BGRA formats aren't as well supported in GLES as in GL, specially in
   * transfer operations. So we only register support for it in GL.
   */
  add_formats(gl_base_rgba_formats);
  add_formats(gl_srgb_formats);
}

void vrend_build_format_list_gles(void)
{
  /* The BGR[A|X] formats is required but OpenGL ES does not
   * support rendering to it. Try to use GL_BGRA_EXT from the
   * GL_EXT_texture_format_BGRA8888 extension. But the
   * GL_BGRA_EXT format is not supported by OpenGL Desktop.
   */
  add_formats(gles_bgra_formats);


  /* The Z32 format is required, but OpenGL ES does not support
   * using it as a depth buffer. We just fake support with Z24
   * and hope nobody notices.
   */
  add_formats(gles_z32_format);
}

/* glTexStorage may not support all that is supported by glTexImage,
 * so add a flag to indicate when it can be used.
 */
void vrend_check_texture_storage(struct vrend_format_table *table)
{
   int i;
   GLuint tex_id;
   for (i = 0; i < VIRGL_FORMAT_MAX; i++) {

      if (table[i].internalformat != 0) {
         glGenTextures(1, &tex_id);
         glBindTexture(GL_TEXTURE_2D, tex_id);
         glTexStorage2D(GL_TEXTURE_2D, 1, table[i].internalformat, 32, 32);
         if (glGetError() == GL_NO_ERROR)
            table[i].bindings |= VIRGL_BIND_CAN_TEXTURE_STORAGE;
         glDeleteTextures(1, &tex_id);
      }
   }
}

unsigned vrend_renderer_query_multisample_caps(unsigned max_samples, struct virgl_caps_v2 *caps)
{
   GLuint tex;
   GLuint fbo;
   GLenum status;

   uint max_samples_confirmed = 1;
   uint test_num_samples[4] = {2,4,8,16};
   int out_buf_offsets[4] = {0,1,2,4};
   int lowest_working_ms_count_idx = -1;

   glGenFramebuffers( 1, &fbo );
   memset(caps->sample_locations, 0, 8 * sizeof(uint32_t));

   for (int i = 3; i >= 0; i--) {
      if (test_num_samples[i] > max_samples)
         continue;
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex);
      glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, test_num_samples[i], GL_RGBA32F, 64, 64, GL_TRUE);
      status = glGetError();
      if (status == GL_NO_ERROR) {
         glBindFramebuffer(GL_FRAMEBUFFER, fbo);
         glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, tex, 0);
         status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
         if (status == GL_FRAMEBUFFER_COMPLETE) {
            if (max_samples_confirmed < test_num_samples[i])
               max_samples_confirmed = test_num_samples[i];

            for (uint k = 0; k < test_num_samples[i]; ++k) {
               float msp[2];
               uint32_t compressed;
               glGetMultisamplefv(GL_SAMPLE_POSITION, k, msp);
               compressed = ((unsigned)(floor(msp[0] * 16.0f)) & 0xf) << 4;
               compressed |= ((unsigned)(floor(msp[1] * 16.0f)) & 0xf);
               caps->sample_locations[out_buf_offsets[i] + (k >> 2)] |= compressed  << (8 * (k & 3));
            }
            lowest_working_ms_count_idx = i;
         } else {
            /* If a framebuffer doesn't support low sample counts,
             * use the sample position from the last working larger count. */
            if (lowest_working_ms_count_idx > 0) {
               for (uint k = 0; k < test_num_samples[i]; ++k) {
                  caps->sample_locations[out_buf_offsets[i] + (k >> 2)] =
                        caps->sample_locations[out_buf_offsets[lowest_working_ms_count_idx]  + (k >> 2)];
               }
            }
         }
         glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }
      glDeleteTextures(1, &tex);
   }
   glDeleteFramebuffers(1, &fbo);
   return max_samples_confirmed;
}

/* returns: 1 = compatible, -1 = not compatible, 0 = undecided */
static int format_uncompressed_compressed_copy_compatible(enum pipe_format src,
                                                          enum pipe_format dst)
{
   switch (src) {
   case PIPE_FORMAT_R32G32B32A32_UINT:
   case PIPE_FORMAT_R32G32B32A32_SINT:
   case PIPE_FORMAT_R32G32B32A32_FLOAT:
   case PIPE_FORMAT_R32G32B32A32_SNORM:
   case PIPE_FORMAT_R32G32B32A32_UNORM:
      switch (dst) {
      case PIPE_FORMAT_DXT3_RGBA:
      case PIPE_FORMAT_DXT3_SRGBA:
      case PIPE_FORMAT_DXT5_RGBA:
      case PIPE_FORMAT_DXT5_SRGBA:
      case PIPE_FORMAT_RGTC2_UNORM:
      case PIPE_FORMAT_RGTC2_SNORM:
      case PIPE_FORMAT_BPTC_RGBA_UNORM:
      case PIPE_FORMAT_BPTC_SRGBA:
      case PIPE_FORMAT_BPTC_RGB_FLOAT:
      case PIPE_FORMAT_BPTC_RGB_UFLOAT:
         return 1;
      default:
         return -1;
      }
   case PIPE_FORMAT_R16G16B16A16_UINT:
   case PIPE_FORMAT_R16G16B16A16_SINT:
   case PIPE_FORMAT_R16G16B16A16_FLOAT:
   case PIPE_FORMAT_R16G16B16A16_SNORM:
   case PIPE_FORMAT_R16G16B16A16_UNORM:
   case PIPE_FORMAT_R32G32_UINT:
   case PIPE_FORMAT_R32G32_SINT:
   case PIPE_FORMAT_R32G32_FLOAT:
   case PIPE_FORMAT_R32G32_UNORM:
   case PIPE_FORMAT_R32G32_SNORM:
      switch (dst) {
      case PIPE_FORMAT_DXT1_RGBA:
      case PIPE_FORMAT_DXT1_SRGBA:
      case PIPE_FORMAT_DXT1_RGB:
      case PIPE_FORMAT_DXT1_SRGB:
      case PIPE_FORMAT_RGTC1_UNORM:
      case PIPE_FORMAT_RGTC1_SNORM:
         return 1;
      default:
         return -1;
      }
   default:
      return 0;
   }
}

static boolean format_compressed_compressed_copy_compatible(enum pipe_format src, enum pipe_format dst)
{
   if ((src == PIPE_FORMAT_RGTC1_UNORM && dst == PIPE_FORMAT_RGTC1_SNORM) ||
       (src == PIPE_FORMAT_RGTC2_UNORM && dst == PIPE_FORMAT_RGTC2_SNORM) ||
       (src == PIPE_FORMAT_BPTC_RGBA_UNORM && dst == PIPE_FORMAT_BPTC_SRGBA) ||
       (src == PIPE_FORMAT_BPTC_RGB_FLOAT && dst == PIPE_FORMAT_BPTC_RGB_UFLOAT))
      return true;
   return false;
}

boolean format_is_copy_compatible(enum pipe_format src, enum pipe_format dst,
                                  boolean allow_compressed)
{
   int r;

   if (src == dst)
      return true;

   if (util_format_is_plain(src) && util_format_is_plain(dst))  {
      const struct util_format_description *src_desc = util_format_description(src);
      const struct util_format_description *dst_desc = util_format_description(dst);
      return util_is_format_compatible(src_desc, dst_desc);
   }

   if (!allow_compressed)
      return false;

   /* compressed-uncompressed */
   r = format_uncompressed_compressed_copy_compatible(src, dst);
   if (r)
      return r > 0;

   r = format_uncompressed_compressed_copy_compatible(dst, src);
   if (r)
      return r > 0;

   return format_compressed_compressed_copy_compatible(dst, src);
}
