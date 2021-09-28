/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INPUT_DATA_H_
#define INPUT_DATA_H_

#include "cras_dsp_pipeline.h"
#include "float_buffer.h"

/*
 * Structure holding the information used when a chunk of input buffer
 * is accessed by multiple streams with different properties and
 * processing requirements.
 * Member:
 *    ext - Provides interface to read and process buffer in dsp pipeline.
 *    dev_ptr - Pointer to the associated input iodev.
 *    area - The audio area used for deinterleaved data copy.
 *    fbuffer - Floating point buffer from input device.
 */
struct input_data {
	struct ext_dsp_module ext;
	void *dev_ptr;
	struct cras_audio_area *area;
	struct float_buffer *fbuffer;
};

/*
 * Creates an input_data instance for input iodev.
 * Args:
 *    dev_ptr - Pointer to the associated input device.
 */
struct input_data *input_data_create(void *dev_ptr);

/* Destroys an input_data instance. */
void input_data_destroy(struct input_data **data);

/* Sets how many frames in buffer has been read by all input streams. */
void input_data_set_all_streams_read(struct input_data *data,
				     unsigned int nframes);

/*
 * Gets an audio area for |stream| to read data from. An input_data may be
 * accessed by multiple streams while some requires processing, the
 * |offsets| arguments helps track the offset value each stream has read
 * into |data|.
 * Args:
 *    data - The input data to get audio area from.
 *    stream - The stream that reads data.
 *    offsets - Structure holding the mapping from stream to the offset value
 *        of how many frames each stream has read into input buffer.
 *    area - To be filled with a pointer to an audio area struct for stream to
 *        read data.
 *    offset - To be filled with the samples offset in |area| that |stream|
 *        should start reading.
 */
int input_data_get_for_stream(struct input_data *data,
			      struct cras_rstream *stream,
			      struct buffer_share *offsets,
			      struct cras_audio_area **area,
			      unsigned int *offset);

/*
 * Marks |frames| of audio data as read by |stream|.
 * Args:
 *    data - The input_data to mark frames has been read by |stream|.
 *    stream - The stream that has read audio data.
 *    offsets - Structure holding the mapping from stream to the offset value
 *        of how many frames each stream has read into input buffer.
 *    frames - Number of frames |stream| has read.
 */
int input_data_put_for_stream(struct input_data *data,
			      struct cras_rstream *stream,
			      struct buffer_share *offsets,
			      unsigned int frames);

#endif /* INPUT_DATA_H_ */
