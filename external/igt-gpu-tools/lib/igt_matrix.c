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
 */

#include "igt_core.h"
#include "igt_matrix.h"

/**
 * SECTION:igt_matrix
 * @short_description: Matrix math library
 * @title: Matrix
 * @include: igt.h
 *
 * This library contains helper functions for basic matrix math.
 * The library operates on #igt_mat4 and #igt_vec4 structures.
 */

/**
 * igt_matrix_print:
 * @m: the matrix
 *
 * Print out the matrix elements.
 */
void igt_matrix_print(const struct igt_mat4 *m)
{
	for (int row = 0; row < 4; row++) {
		igt_info("|");
		for (int col = 0; col < 4; col++) {
			igt_info("%4.4f,", m->d[m(row, col)]);
		}
		igt_info("|\n");
	}
}

/**
 * igt_matrix_identity:
 *
 * Returns:
 * An identity matrix.
 */
struct igt_mat4 igt_matrix_identity(void)
{
	static const struct igt_mat4 ret = {
		.d[m(0, 0)] = 1.0f,
		.d[m(1, 1)] = 1.0f,
		.d[m(2, 2)] = 1.0f,
		.d[m(3, 3)] = 1.0f,
	};

	return ret;
}

/**
 * igt_matrix_scale:
 * @x: x scaling amount
 * @y: y scaling amount
 * @z: z scaling amount
 *
 * Returns:
 * An scaling matrix.
 */
struct igt_mat4 igt_matrix_scale(float x, float y, float z)
{
	struct igt_mat4 ret = {
		.d[m(0, 0)] = x,
		.d[m(1, 1)] = y,
		.d[m(2, 2)] = z,
		.d[m(3, 3)] = 1.0f,
	};

	return ret;
}

/**
 * igt_matrix_translate:
 * @x: x translation amount
 * @y: y translation amount
 * @z: z translation amount
 *
 * Returns:
 * A translation matrix.
 */
struct igt_mat4 igt_matrix_translate(float x, float y, float z)
{
	struct igt_mat4 ret = {
		.d[m(0, 0)] = 1.0f,
		.d[m(1, 1)] = 1.0f,
		.d[m(2, 2)] = 1.0f,
		.d[m(0, 3)] = x,
		.d[m(1, 3)] = y,
		.d[m(2, 3)] = z,
		.d[m(3, 3)] = 1.0f,
	};

	return ret;
}

/**
 * igt_matrix_multiply:
 * @a: Left matrix
 * @b: Right matrix
 *
 * Multiply two matrices together. @a is on the left,
 * @b on the right.
 *
 * Returns:
 * The resulting matrix.
 */
struct igt_mat4 igt_matrix_multiply(const struct igt_mat4 *a,
				    const struct igt_mat4 *b)
{
	struct igt_mat4 ret = {};

	for (int col = 0; col < 4; col++) {
		for (int row = 0; row < 4; row++) {
			for (int i = 0; i < 4; i++)
				ret.d[m(row, col)] += a->d[m(row, i)] * b->d[m(i, col)];
		}
	}

	return ret;
}
