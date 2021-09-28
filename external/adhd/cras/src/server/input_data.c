/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <syslog.h>

#include "buffer_share.h"
#include "cras_audio_area.h"
#include "cras_dsp_pipeline.h"
#include "cras_mix.h"
#include "cras_rstream.h"
#include "cras_system_state.h"
#include "dsp_util.h"
#include "input_data.h"
#include "utlist.h"

void input_data_run(struct ext_dsp_module *ext, unsigned int nframes)
{
	struct input_data *data = (struct input_data *)ext;
	float *const *wp;
	int i;
	unsigned int writable;
	unsigned int offset = 0;

	while (nframes) {
		writable = float_buffer_writable(data->fbuffer);
		writable = MIN(nframes, writable);
		if (!writable) {
			syslog(LOG_ERR,
			       "Not enough space to process input data");
			break;
		}
		wp = float_buffer_write_pointer(data->fbuffer);
		for (i = 0; i < data->fbuffer->num_channels; i++)
			memcpy(wp[i], ext->ports[i] + offset,
			       writable * sizeof(float));

		float_buffer_written(data->fbuffer, writable);
		nframes -= writable;
		offset += writable;
	}
}

void input_data_configure(struct ext_dsp_module *ext, unsigned int buffer_size,
			  unsigned int num_channels, unsigned int rate)
{
	struct input_data *data = (struct input_data *)ext;

	if (data->fbuffer)
		float_buffer_destroy(&data->fbuffer);
	data->fbuffer = float_buffer_create(buffer_size, num_channels);
}

struct input_data *input_data_create(void *dev_ptr)
{
	struct input_data *data = (struct input_data *)calloc(1, sizeof(*data));

	data->dev_ptr = dev_ptr;

	data->ext.run = input_data_run;
	data->ext.configure = input_data_configure;

	return data;
}

void input_data_destroy(struct input_data **data)
{
	if ((*data)->fbuffer)
		float_buffer_destroy(&(*data)->fbuffer);
	free(*data);
	*data = NULL;
}

void input_data_set_all_streams_read(struct input_data *data,
				     unsigned int nframes)
{
	if (!data->fbuffer)
		return;

	if (float_buffer_level(data->fbuffer) < nframes) {
		syslog(LOG_ERR,
		       "All streams read %u frames exceeds %u"
		       " in input_data's buffer",
		       nframes, float_buffer_level(data->fbuffer));
		float_buffer_reset(data->fbuffer);
		return;
	}
	float_buffer_read(data->fbuffer, nframes);
}

/*
 * The logic is not trivial to return the cras_audio_area and offset for
 * a input stream to read. The buffer position and length of a bunch of
 * input member variables are described below.
 *
 *                          hw_ptr                 appl_ptr
 * a. buffer of input device: |------------------------|
 * b. fbuffer of input data:         |<--------------->|
 * c. stream offset of input data:         |<--------->|
 *    stream offset of input data:                |<-->|
 *    stream offset of input data:     |<------------->|
 * d. audio area of input data:          |<----------->|
 *
 * One thing to keep in mind is, the offset could exceed the size of
 * buffer to read. It's not intuitive though why the stream offset would
 * exceed buffer size. Check this example:
 *
 * Idev gets input buffer 500 frames. One stream read 400, while the other
 * stream read 100. We track stream offset [0, 300] after both stream
 * consumes 100 frames. In the next wake up, audio thread asks idev to
 * get 250 frames. Now the input data holds audio area containing 250 frames
 * of audio as queried, while its float buffer contains 400 frames of audio
 * deinterleaved from last wake up.
 *
 * Wake up at T0:
 *                        hw_ptr                        appl_ptr
 * Input audio area         |-------------------------------|
 * deinterleave float       |-------------------------------|
 * Stream 1 read                                     |------|
 * Stream 2 read                    |-----------------------|
 *
 * Wake up at T1:
                          hw_ptr                 appl_ptr
 * Input audio area                     |------------|
 * deinterleave float       |------------------------|
 * Stream 1 offset                                   |
 * Stream 2 offset                  |----------------|
 *
 * Case 1:
 * A normal input stream, of read offset 0, about to read from device.
 * We shall return the exact audio area from idev, and set read offset to 0.
 *
 * Case 2:
 * A normal input stream, of read offset 300, about to read from device.
 * We shall return the exact audio area from idev but clip read offset to 250.
 *
 * Case 3:
 * An APM Stream of read offset 300, would like to read the deinterleaved
 * float buffer. We shall let APM process the float buffer from offset 300.
 * Don't bother clip read offset in this case, because fbuffer contains
 * the deepest deinterleaved audio data ever read from idev.
 */
int input_data_get_for_stream(struct input_data *data,
			      struct cras_rstream *stream,
			      struct buffer_share *offsets,
			      struct cras_audio_area **area,
			      unsigned int *offset)
{
	int apm_processed;
	struct cras_apm *apm;
	int stream_offset = buffer_share_id_offset(offsets, stream->stream_id);

	apm = cras_apm_list_get(stream->apm_list, data->dev_ptr);
	if (apm == NULL) {
		/*
		 * Case 1 and 2 from above example.
		 */
		*area = data->area;
		*offset = MIN(stream_offset, data->area->frames);
	} else {
		/*
		 * Case 3 from above example.
		 */
		apm_processed = cras_apm_list_process(apm, data->fbuffer,
						      stream_offset);
		if (apm_processed < 0) {
			cras_apm_list_remove(stream->apm_list, apm);
			return 0;
		}
		buffer_share_offset_update(offsets, stream->stream_id,
					   apm_processed);
		*area = cras_apm_list_get_processed(apm);
		*offset = 0;
	}

	return 0;
}

int input_data_put_for_stream(struct input_data *data,
			      struct cras_rstream *stream,
			      struct buffer_share *offsets, unsigned int frames)
{
	struct cras_apm *apm =
		cras_apm_list_get(stream->apm_list, data->dev_ptr);

	if (apm)
		cras_apm_list_put_processed(apm, frames);
	else
		buffer_share_offset_update(offsets, stream->stream_id, frames);

	return 0;
}
