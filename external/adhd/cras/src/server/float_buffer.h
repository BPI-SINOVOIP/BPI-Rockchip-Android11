/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef FLOAT_BUFFER_H_
#define FLOAT_BUFFER_H_

#include "byte_buffer.h"

/*
 * Circular buffer storing deinterleaved floating point data.
 * Members:
 *    fp - Pointer to be filled wtih read/write position of the buffer.
 *    num_channels - Number of channels.
 */
struct float_buffer {
	struct byte_buffer *buf;
	float **fp;
	unsigned int num_channels;
};

/*
 * Creates an float_buffer.
 * Args:
 *    max_size - The max number of frames this buffer may store.
 *    num_channels - Number of channels of the deinterleaved data.
 */
static inline struct float_buffer *
float_buffer_create(unsigned int max_size, unsigned int num_channels)
{
	struct float_buffer *b;

	b = (struct float_buffer *)calloc(1, sizeof(*b));

	b->num_channels = num_channels;
	b->fp = (float **)malloc(num_channels * sizeof(float *));
	b->buf = (struct byte_buffer *)calloc(
		1, sizeof(struct byte_buffer) +
			   max_size * num_channels * sizeof(float));
	b->buf->max_size = max_size;
	b->buf->used_size = max_size;
	return b;
}

/* Destroys the float buffer. */
static inline void float_buffer_destroy(struct float_buffer **b)
{
	if (*b == NULL)
		return;

	byte_buffer_destroy(&(*b)->buf);
	free((*b)->fp);
	free(*b);
	*b = NULL;
}

/* Gets the write pointer of given float_buffer. */
static inline float *const *float_buffer_write_pointer(struct float_buffer *b)
{
	unsigned int i;
	float *data = (float *)b->buf->bytes;

	for (i = 0; i < b->num_channels; i++, data += b->buf->max_size)
		b->fp[i] = data + b->buf->write_idx;
	return b->fp;
}

/* Gets the number of frames can write to the float_buffer. */
static inline unsigned int float_buffer_writable(struct float_buffer *b)
{
	return buf_writable(b->buf);
}

/* Marks |nwritten| of frames as written to float_buffer. */
static inline void float_buffer_written(struct float_buffer *b,
					unsigned int nwritten)
{
	buf_increment_write(b->buf, nwritten);
}

/* Gets the read pointer of given float_buffer. */
static inline float *const *float_buffer_read_pointer(struct float_buffer *b,
						      unsigned int offset,
						      unsigned int *readable)
{
	unsigned int i;
	float *data = (float *)b->buf->bytes;
	unsigned int nread = buf_readable(b->buf);

	if (offset >= buf_queued(b->buf)) {
		*readable = 0;
		offset = 0;
	} else if (offset >= nread) {
		/* wraps */
		offset = offset + b->buf->read_idx - b->buf->max_size;
		*readable = MIN(*readable, b->buf->write_idx - offset);
	} else {
		*readable = MIN(*readable, nread - offset);
		offset += b->buf->read_idx;
	}

	for (i = 0; i < b->num_channels; i++, data += b->buf->max_size)
		b->fp[i] = data + offset;
	return b->fp;
}

/* Gets the buffer level in frames queued in float_buffer. */
static inline unsigned int float_buffer_level(struct float_buffer *b)
{
	return buf_queued(b->buf);
}

/* Resets float_buffer to initial state. */
static inline void float_buffer_reset(struct float_buffer *b)
{
	buf_reset(b->buf);
}

/* Marks |nread| frames as read in float_buffer. */
static inline void float_buffer_read(struct float_buffer *b, unsigned int nread)
{
	buf_increment_read(b->buf, nread);
}

#endif /* FLOAT_BUFFER_H_ */
