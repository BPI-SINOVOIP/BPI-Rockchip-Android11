/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <inttypes.h>
#include <string.h>
#include <syslog.h>

#include <webrtc-apm/webrtc_apm.h>

#include "byte_buffer.h"
#include "cras_apm_list.h"
#include "cras_audio_area.h"
#include "cras_audio_format.h"
#include "cras_dsp_pipeline.h"
#include "cras_iodev.h"
#include "cras_iodev_list.h"
#include "dsp_util.h"
#include "dumper.h"
#include "float_buffer.h"
#include "iniparser_wrapper.h"
#include "utlist.h"

static const unsigned int MAX_INI_NAME_LEN = 63;

#define AEC_CONFIG_NAME "aec.ini"
#define APM_CONFIG_NAME "apm.ini"

/*
 * Structure holding a WebRTC audio processing module and necessary
 * info to process and transfer input buffer from device to stream.
 *
 * Below chart describes the buffer structure inside APM and how an input buffer
 * flows from a device through the APM to stream. APM processes audio buffers in
 * fixed 10ms width, and that's the main reason we need two copies of the
 * buffer:
 * (1) to cache input buffer from device until 10ms size is filled.
 * (2) to store the interleaved buffer, of 10ms size also, after APM processing.
 *
 *  ________   _______     _______________________________
 *  |      |   |     |     |_____________APM ____________|
 *  |input |-> | DSP |---> ||           |    |          || -> stream 1
 *  |device|   |     | |   || float buf | -> | byte buf ||
 *  |______|   |_____| |   ||___________|    |__________||
 *                     |   |_____________________________|
 *                     |   _______________________________
 *                     |-> |             APM 2           | -> stream 2
 *                     |   |_____________________________|
 *                     |                                       ...
 *                     |
 *                     |------------------------------------> stream N
 *
 * Members:
 *    apm_ptr - An APM instance from libwebrtc_audio_processing
 *    dev_ptr - Pointer to the device this APM is associated with.
 *    buffer - Stores the processed/interleaved data ready for stream to read.
 *    fbuffer - Stores the floating pointer buffer from input device waiting
 *        for APM to process.
 *    dev_fmt - The format used by the iodev this APM attaches to.
 *    fmt - The audio data format configured for this APM.
 *    area - The cras_audio_area used for copying processed data to client
 *        stream.
 *    work_queue - A task queue instance created and destroyed by
 *        libwebrtc_apm.
 */
struct cras_apm {
	webrtc_apm apm_ptr;
	void *dev_ptr;
	struct byte_buffer *buffer;
	struct float_buffer *fbuffer;
	struct cras_audio_format dev_fmt;
	struct cras_audio_format fmt;
	struct cras_audio_area *area;
	void *work_queue;
	struct cras_apm *prev, *next;
};

/*
 * Lists of cras_apm instances created for a stream. A stream may
 * have more than one cras_apm when multiple input devices are
 * enabled. The most common scenario is the silent input iodev be
 * enabled when CRAS switches active input device.
 */
struct cras_apm_list {
	void *stream_ptr;
	uint64_t effects;
	struct cras_apm *apms;
	struct cras_apm_list *prev, *next;
};

/*
 * Object used to analyze playback audio from output iodev. It is responsible
 * to get buffer containing latest output data and provide it to the APM
 * instances which want to analyze reverse stream.
 * Member:
 *    ext - The interface implemented to process reverse(output) stream
 *        data in various formats.
 *    fbuf - Middle buffer holding reverse data for APMs to analyze.
 *    odev - Pointer to the output iodev playing audio as the reverse
 *        stream. NULL if there's no playback stream.
 *    dev_rate - The sample rate odev is opened for.
 *    process_reverse - Flag to indicate if there's APM has effect that
 *        needs to process reverse stream.
 */
struct cras_apm_reverse_module {
	struct ext_dsp_module ext;
	struct float_buffer *fbuf;
	struct cras_iodev *odev;
	unsigned int dev_rate;
	unsigned process_reverse;
};

static struct cras_apm_reverse_module *rmodule = NULL;
static struct cras_apm_list *apm_list = NULL;
static const char *aec_config_dir = NULL;
static char ini_name[MAX_INI_NAME_LEN + 1];
static dictionary *aec_ini = NULL;
static dictionary *apm_ini = NULL;

/* Update the global process reverse flag. Should be called when apms are added
 * or removed. */
static void update_process_reverse_flag()
{
	struct cras_apm_list *list;

	if (!rmodule)
		return;
	rmodule->process_reverse = 0;
	DL_FOREACH (apm_list, list) {
		rmodule->process_reverse |=
			!!(list->effects & APM_ECHO_CANCELLATION);
	}
}

static void apm_destroy(struct cras_apm **apm)
{
	if (*apm == NULL)
		return;
	byte_buffer_destroy(&(*apm)->buffer);
	float_buffer_destroy(&(*apm)->fbuffer);
	cras_audio_area_destroy((*apm)->area);

	/* Any unfinished AEC dump handle will be closed. */
	webrtc_apm_destroy((*apm)->apm_ptr);
	free(*apm);
	*apm = NULL;
}

struct cras_apm_list *cras_apm_list_create(void *stream_ptr, uint64_t effects)
{
	struct cras_apm_list *list;

	if (effects == 0)
		return NULL;

	DL_SEARCH_SCALAR(apm_list, list, stream_ptr, stream_ptr);
	if (list)
		return list;

	list = (struct cras_apm_list *)calloc(1, sizeof(*list));
	list->stream_ptr = stream_ptr;
	list->effects = effects;
	list->apms = NULL;
	DL_APPEND(apm_list, list);

	return list;
}

struct cras_apm *cras_apm_list_get(struct cras_apm_list *list, void *dev_ptr)
{
	struct cras_apm *apm;

	if (list == NULL)
		return NULL;

	DL_FOREACH (list->apms, apm) {
		if (apm->dev_ptr == dev_ptr)
			return apm;
	}
	return NULL;
}

uint64_t cras_apm_list_get_effects(struct cras_apm_list *list)
{
	if (list == NULL)
		return 0;
	else
		return list->effects;
}

void cras_apm_list_remove(struct cras_apm_list *list, void *dev_ptr)
{
	struct cras_apm *apm;

	DL_FOREACH (list->apms, apm) {
		if (apm->dev_ptr == dev_ptr) {
			DL_DELETE(list->apms, apm);
			apm_destroy(&apm);
		}
	}
}

/*
 * WebRTC APM handles no more than stereo + keyboard mic channels.
 * Ignore keyboard mic feature for now because that requires processing on
 * mixed buffer from two input devices. Based on that we should modify the best
 * channel layout for APM use.
 * Args:
 *    apm_fmt - Pointer to a format struct already filled with the value of
 *        the open device format. Its content may be modified for APM use.
 */
static void get_best_channels(struct cras_audio_format *apm_fmt)
{
	int ch;
	int8_t layout[CRAS_CH_MAX];

	/* Assume device format has correct channel layout populated. */
	if (apm_fmt->num_channels <= 2)
		return;

	/* If the device provides recording from more channels than we care
	 * about, construct a new channel layout containing subset of original
	 * channels that matches either FL, FR, or FC.
	 * TODO(hychao): extend the logic when we have a stream that wants
	 * to record channels like RR(rear right).
	 */
	for (ch = 0; ch < CRAS_CH_MAX; ch++)
		layout[ch] = -1;

	apm_fmt->num_channels = 0;
	if (apm_fmt->channel_layout[CRAS_CH_FL] != -1)
		layout[CRAS_CH_FL] = apm_fmt->num_channels++;
	if (apm_fmt->channel_layout[CRAS_CH_FR] != -1)
		layout[CRAS_CH_FR] = apm_fmt->num_channels++;
	if (apm_fmt->channel_layout[CRAS_CH_FC] != -1)
		layout[CRAS_CH_FC] = apm_fmt->num_channels++;

	for (ch = 0; ch < CRAS_CH_MAX; ch++)
		apm_fmt->channel_layout[ch] = layout[ch];
}

struct cras_apm *cras_apm_list_add(struct cras_apm_list *list, void *dev_ptr,
				   const struct cras_audio_format *dev_fmt)
{
	struct cras_apm *apm;

	DL_FOREACH (list->apms, apm)
		if (apm->dev_ptr == dev_ptr)
			return apm;

	// TODO(hychao): Remove the check when we enable more effects.
	if (!(list->effects & APM_ECHO_CANCELLATION))
		return NULL;

	apm = (struct cras_apm *)calloc(1, sizeof(*apm));

	/* Configures APM to the format used by input device. If the channel
	 * count is larger than stereo, use the standard channel count/layout
	 * in APM. */
	apm->dev_fmt = *dev_fmt;
	apm->fmt = *dev_fmt;
	get_best_channels(&apm->fmt);

	apm->apm_ptr = webrtc_apm_create(apm->fmt.num_channels,
					 apm->fmt.frame_rate, aec_ini, apm_ini);
	if (apm->apm_ptr == NULL) {
		syslog(LOG_ERR,
		       "Fail to create webrtc apm for ch %zu"
		       " rate %zu effect %" PRIu64,
		       dev_fmt->num_channels, dev_fmt->frame_rate,
		       list->effects);
		free(apm);
		return NULL;
	}

	apm->dev_ptr = dev_ptr;
	apm->work_queue = NULL;

	/* WebRTC APM wants 10 ms equivalence of data to process. */
	apm->buffer = byte_buffer_create(10 * apm->fmt.frame_rate / 1000 *
					 cras_get_format_bytes(&apm->fmt));
	apm->fbuffer = float_buffer_create(10 * apm->fmt.frame_rate / 1000,
					   apm->fmt.num_channels);
	apm->area = cras_audio_area_create(apm->fmt.num_channels);
	cras_audio_area_config_channels(apm->area, &apm->fmt);

	DL_APPEND(list->apms, apm);
	update_process_reverse_flag();

	return apm;
}

int cras_apm_list_destroy(struct cras_apm_list *list)
{
	struct cras_apm_list *tmp;
	struct cras_apm *apm;

	DL_FOREACH (apm_list, tmp) {
		if (tmp == list) {
			DL_DELETE(apm_list, tmp);
			break;
		}
	}

	if (tmp == NULL)
		return 0;

	DL_FOREACH (list->apms, apm) {
		DL_DELETE(list->apms, apm);
		apm_destroy(&apm);
	}
	free(list);

	update_process_reverse_flag();

	return 0;
}

/*
 * Determines the iodev to be used as the echo reference for APM reverse
 * analysis. If there exists the special purpose "echo reference dev" then
 * use it. Otherwise just use this output iodev.
 */
static struct cras_iodev *get_echo_reference_target(struct cras_iodev *iodev)
{
	return iodev->echo_reference_dev ? iodev->echo_reference_dev : iodev;
}

/*
 * Updates the first enabled output iodev in the list, determine the echo
 * reference target base on this output iodev, and register rmodule as ext dsp
 * module to this echo reference target.
 * When this echo reference iodev is opened and audio data flows through its
 * dsp pipeline, APMs will anaylize the reverse stream. This is expected to be
 * called in main thread when output devices enable/dsiable state changes.
 */
static void update_first_output_dev_to_process()
{
	struct cras_iodev *echo_ref;
	struct cras_iodev *iodev =
		cras_iodev_list_get_first_enabled_iodev(CRAS_STREAM_OUTPUT);

	if (iodev == NULL)
		return;

	echo_ref = get_echo_reference_target(iodev);

	/* If rmodule is already tracking echo_ref, do nothing. */
	if (rmodule->odev == echo_ref)
		return;

	/* Detach from the old iodev that rmodule was tracking. */
	if (rmodule->odev) {
		cras_iodev_set_ext_dsp_module(rmodule->odev, NULL);
		rmodule->odev = NULL;
	}

	rmodule->odev = echo_ref;
	cras_iodev_set_ext_dsp_module(echo_ref, &rmodule->ext);
}

static void handle_device_enabled(struct cras_iodev *iodev, void *cb_data)
{
	if (iodev->direction != CRAS_STREAM_OUTPUT)
		return;

	/* Register to the first enabled output device. */
	update_first_output_dev_to_process();
}

static void handle_device_disabled(struct cras_iodev *iodev, void *cb_data)
{
	struct cras_iodev *echo_ref;

	if (iodev->direction != CRAS_STREAM_OUTPUT)
		return;

	echo_ref = get_echo_reference_target(iodev);

	if (rmodule->odev == echo_ref) {
		cras_iodev_set_ext_dsp_module(echo_ref, NULL);
		rmodule->odev = NULL;
	}

	/* Register to the first enabled output device. */
	update_first_output_dev_to_process();
}

static int process_reverse(struct float_buffer *fbuf, unsigned int frame_rate)
{
	struct cras_apm_list *list;
	struct cras_apm *apm;
	int ret;
	float *const *wp;

	if (float_buffer_writable(fbuf))
		return 0;

	wp = float_buffer_write_pointer(fbuf);

	DL_FOREACH (apm_list, list) {
		if (!(list->effects & APM_ECHO_CANCELLATION))
			continue;

		DL_FOREACH (list->apms, apm) {
			ret = webrtc_apm_process_reverse_stream_f(
				apm->apm_ptr, fbuf->num_channels, frame_rate,
				wp);
			if (ret) {
				syslog(LOG_ERR, "APM process reverse err");
				return ret;
			}
		}
	}
	float_buffer_reset(fbuf);
	return 0;
}

void reverse_data_run(struct ext_dsp_module *ext, unsigned int nframes)
{
	struct cras_apm_reverse_module *rmod =
		(struct cras_apm_reverse_module *)ext;
	unsigned int writable;
	int i, offset = 0;
	float *const *wp;

	if (!rmod->process_reverse)
		return;

	while (nframes) {
		process_reverse(rmod->fbuf, rmod->dev_rate);
		writable = float_buffer_writable(rmod->fbuf);
		writable = MIN(nframes, writable);
		wp = float_buffer_write_pointer(rmod->fbuf);
		for (i = 0; i < rmod->fbuf->num_channels; i++)
			memcpy(wp[i], ext->ports[i] + offset,
			       writable * sizeof(float));

		offset += writable;
		float_buffer_written(rmod->fbuf, writable);
		nframes -= writable;
	}
}

void reverse_data_configure(struct ext_dsp_module *ext,
			    unsigned int buffer_size, unsigned int num_channels,
			    unsigned int rate)
{
	struct cras_apm_reverse_module *rmod =
		(struct cras_apm_reverse_module *)ext;
	if (rmod->fbuf)
		float_buffer_destroy(&rmod->fbuf);
	rmod->fbuf = float_buffer_create(rate / 100, num_channels);
	rmod->dev_rate = rate;
}

static void get_aec_ini(const char *config_dir)
{
	snprintf(ini_name, MAX_INI_NAME_LEN, "%s/%s", config_dir,
		 AEC_CONFIG_NAME);
	ini_name[MAX_INI_NAME_LEN] = '\0';

	if (aec_ini) {
		iniparser_freedict(aec_ini);
		aec_ini = NULL;
	}
	aec_ini = iniparser_load_wrapper(ini_name);
	if (aec_ini == NULL)
		syslog(LOG_INFO, "No aec ini file %s", ini_name);
}

static void get_apm_ini(const char *config_dir)
{
	snprintf(ini_name, MAX_INI_NAME_LEN, "%s/%s", config_dir,
		 APM_CONFIG_NAME);
	ini_name[MAX_INI_NAME_LEN] = '\0';

	if (apm_ini) {
		iniparser_freedict(apm_ini);
		apm_ini = NULL;
	}
	apm_ini = iniparser_load_wrapper(ini_name);
	if (apm_ini == NULL)
		syslog(LOG_INFO, "No apm ini file %s", ini_name);
}

int cras_apm_list_init(const char *device_config_dir)
{
	if (rmodule == NULL) {
		rmodule = (struct cras_apm_reverse_module *)calloc(
			1, sizeof(*rmodule));
		rmodule->ext.run = reverse_data_run;
		rmodule->ext.configure = reverse_data_configure;
	}

	aec_config_dir = device_config_dir;
	get_aec_ini(aec_config_dir);
	get_apm_ini(aec_config_dir);

	update_first_output_dev_to_process();
	cras_iodev_list_set_device_enabled_callback(
		handle_device_enabled, handle_device_disabled, rmodule);

	return 0;
}

void cras_apm_list_reload_aec_config()
{
	if (NULL == aec_config_dir)
		return;

	get_aec_ini(aec_config_dir);
	get_apm_ini(aec_config_dir);

	/* Dump the config content at reload only, for debug. */
	webrtc_apm_dump_configs(apm_ini, aec_ini);
}

int cras_apm_list_deinit()
{
	if (rmodule) {
		if (rmodule->fbuf)
			float_buffer_destroy(&rmodule->fbuf);
		free(rmodule);
		rmodule = NULL;
	}
	return 0;
}

int cras_apm_list_process(struct cras_apm *apm, struct float_buffer *input,
			  unsigned int offset)
{
	unsigned int writable, nframes, nread;
	int ch, i, j, ret;
	float *const *wp;
	float *const *rp;

	nread = float_buffer_level(input);
	if (nread < offset) {
		syslog(LOG_ERR, "Process offset exceeds read level");
		return -EINVAL;
	}

	writable = float_buffer_writable(apm->fbuffer);
	writable = MIN(nread - offset, writable);

	nframes = writable;
	while (nframes) {
		nread = nframes;
		wp = float_buffer_write_pointer(apm->fbuffer);
		rp = float_buffer_read_pointer(input, offset, &nread);

		for (i = 0; i < apm->fbuffer->num_channels; i++) {
			/* Look up the channel position and copy from
			 * the correct index of |input| buffer.
			 */
			for (ch = 0; ch < CRAS_CH_MAX; ch++)
				if (apm->fmt.channel_layout[ch] == i)
					break;
			if (ch == CRAS_CH_MAX)
				continue;

			j = apm->dev_fmt.channel_layout[ch];
			if (j == -1)
				continue;

			memcpy(wp[i], rp[j], nread * sizeof(float));
		}

		nframes -= nread;
		offset += nread;

		float_buffer_written(apm->fbuffer, nread);
	}

	/* process and move to int buffer */
	if ((float_buffer_writable(apm->fbuffer) == 0) &&
	    (buf_queued(apm->buffer) == 0)) {
		nread = float_buffer_level(apm->fbuffer);
		rp = float_buffer_read_pointer(apm->fbuffer, 0, &nread);
		ret = webrtc_apm_process_stream_f(apm->apm_ptr,
						  apm->fmt.num_channels,
						  apm->fmt.frame_rate, rp);
		if (ret) {
			syslog(LOG_ERR, "APM process stream f err");
			return ret;
		}

		dsp_util_interleave(rp, buf_write_pointer(apm->buffer),
				    apm->fbuffer->num_channels, apm->fmt.format,
				    nread);
		buf_increment_write(apm->buffer,
				    nread * cras_get_format_bytes(&apm->fmt));
		float_buffer_reset(apm->fbuffer);
	}

	return writable;
}

struct cras_audio_area *cras_apm_list_get_processed(struct cras_apm *apm)
{
	uint8_t *buf_ptr;

	buf_ptr = buf_read_pointer_size(apm->buffer, &apm->area->frames);
	apm->area->frames /= cras_get_format_bytes(&apm->fmt);
	cras_audio_area_config_buf_pointers(apm->area, &apm->fmt, buf_ptr);
	return apm->area;
}

void cras_apm_list_put_processed(struct cras_apm *apm, unsigned int frames)
{
	buf_increment_read(apm->buffer,
			   frames * cras_get_format_bytes(&apm->fmt));
}

struct cras_audio_format *cras_apm_list_get_format(struct cras_apm *apm)
{
	return &apm->fmt;
}

void cras_apm_list_set_aec_dump(struct cras_apm_list *list, void *dev_ptr,
				int start, int fd)
{
	struct cras_apm *apm;
	char file_name[256];
	int rc;
	FILE *handle;

	DL_SEARCH_SCALAR(list->apms, apm, dev_ptr, dev_ptr);
	if (apm == NULL)
		return;

	if (start) {
		handle = fdopen(fd, "w");
		if (handle == NULL) {
			syslog(LOG_ERR, "Create dump handle fail, errno %d",
			       errno);
			return;
		}
		/* webrtc apm will own the FILE handle and close it. */
		rc = webrtc_apm_aec_dump(apm->apm_ptr, &apm->work_queue, start,
					 handle);
		if (rc)
			syslog(LOG_ERR, "Fail to dump debug file %s, rc %d",
			       file_name, rc);
	} else {
		rc = webrtc_apm_aec_dump(apm->apm_ptr, &apm->work_queue, 0,
					 NULL);
		if (rc)
			syslog(LOG_ERR, "Failed to stop apm debug, rc %d", rc);
	}
}
