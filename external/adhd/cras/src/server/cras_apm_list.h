/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CRAS_APM_LIST_H_
#define CRAS_APM_LIST_H_

#include "cras_types.h"

struct cras_audio_area;
struct cras_audio_format;
struct cras_apm;
struct cras_apm_list;
struct float_buffer;

#ifdef HAVE_WEBRTC_APM

/* Initialize the apm list for analyzing output data. */
int cras_apm_list_init(const char *device_config_dir);

/* Reloads the aec config. Used for debug and tuning. */
void cras_apm_list_reload_aec_config();

/* Deinitialize apm list to free all allocated resources. */
int cras_apm_list_deinit();

/*
 * Creates an list to hold all APM instances created when a stream
 * attaches to an iodev.
 * Args:
 *    stream_ptr - Pointer to the stream.
 *    effects - Bit map specifying the enabled effects on this stream.
 */
struct cras_apm_list *cras_apm_list_create(void *stream_ptr, uint64_t effects);

/*
 * Creates a cras_apm associated to given dev_ptr and adds it to the list.
 * If there already exists an APM instance linked to dev_ptr, we assume
 * the open format is unchanged so just return it.
 * Args:
 *    list - The list holding APM instances.
 *    dev_ptr - Pointer to the iodev to add new APM for.
 *    fmt - Format of the audio data used for this cras_apm.
 */
struct cras_apm *cras_apm_list_add(struct cras_apm_list *list, void *dev_ptr,
				   const struct cras_audio_format *fmt);

/*
 * Gets the cras_apm instance in the list that associates with given dev.
 * Args:
 *    list - The list holding APM instances.
 *    dev_ptr - The iodev as key to look up associated APM.
 */
struct cras_apm *cras_apm_list_get(struct cras_apm_list *list, void *dev_ptr);

/*
 * Gets the effects bit map of the APM list.
 * Args:
 *    list - The list holding APM instances.
 */
uint64_t cras_apm_list_get_effects(struct cras_apm_list *list);

/* Removes a cras_apm from list and destroys it. */
int cras_apm_list_destroy(struct cras_apm_list *list);

/*
 * Removes an APM from the list, expected to be used when an iodev is no
 * longer open for the client stream holding the APM list.
 * Args:
 *    list - The list holding APM instances.
 *    dev_ptr - Device pointer used to look up which apm to remove.
 */
void cras_apm_list_remove(struct cras_apm_list *list, void *dev_ptr);

/* Passes audio data from hardware for cras_apm to process.
 * Args:
 *    apm - The cras_apm instance.
 *    input - Float buffer from device for apm to process.
 *    offset - Offset in |input| to note the data position to start
 *        reading.
 */
int cras_apm_list_process(struct cras_apm *apm, struct float_buffer *input,
			  unsigned int offset);

/* Gets the APM processed data in the form of audio area.
 * Args:
 *    apm - The cras_apm instance that owns the audio area pointer and
 *        processed data.
 * Returns:
 *    The audio area used to read processed data. No need to free
 *    by caller.
 */
struct cras_audio_area *cras_apm_list_get_processed(struct cras_apm *apm);

/* Tells |apm| that |frames| of processed data has been used, so |apm|
 * can allocate space to read more from input device.
 * Args:
 *    apm - The cras_apm instance owns the processed data.
 *    frames - The number in frames of processed data to mark as used.
 */
void cras_apm_list_put_processed(struct cras_apm *apm, unsigned int frames);

/* Gets the format of the actual data processed by webrtc-apm library.
 * Args:
 *    apm - The cras_apm instance holding audio data and format info.
 */
struct cras_audio_format *cras_apm_list_get_format(struct cras_apm *apm);

/* Sets debug recording to start or stop.
 * Args:
 *    list - List contains the apm instance to start/stop debug recording.
 *    dev_ptr - Use as key to look up specific apm to do aec dump.
 *    start - True to set debug recording start, otherwise stop.
 *    fd - File descriptor to aec dump destination.
 */
void cras_apm_list_set_aec_dump(struct cras_apm_list *list, void *dev_ptr,
				int start, int fd);

#else

/*
 * If webrtc audio processing library is not available then define all
 * cras_apm_list functions as dummy. As long as cras_apm_list_add returns
 * NULL, non of the other functions should be called.
 */
static inline int cras_apm_list_init(const char *device_config_dir)
{
	return 0;
}
static inline void cras_apm_list_reload_aec_config()
{
}
static inline struct cras_apm_list *cras_apm_list_create(void *stream_ptr,
							 unsigned int effects)
{
	return NULL;
}
static inline struct cras_apm *
cras_apm_list_add(struct cras_apm_list *list, void *dev_ptr,
		  const struct cras_audio_format *fmt)
{
	return NULL;
}
static inline struct cras_apm *cras_apm_list_get(struct cras_apm_list *list,
						 void *dev_ptr)
{
	return NULL;
}
static inline uint64_t cras_apm_list_get_effects(struct cras_apm_list *list)
{
	return 0;
}
static inline int cras_apm_list_destroy(struct cras_apm_list *list)
{
	return 0;
}
static inline void cras_apm_list_remove(struct cras_apm_list *list,
					void *dev_ptr)
{
}

static inline int cras_apm_list_process(struct cras_apm *apm,
					struct float_buffer *input,
					unsigned int offset)
{
	return 0;
}

static inline struct cras_audio_area *
cras_apm_list_get_processed(struct cras_apm *apm)
{
	return NULL;
}

static inline void cras_apm_list_put_processed(struct cras_apm *apm,
					       unsigned int frames)
{
}

static inline struct cras_audio_format *
cras_apm_list_get_format(struct cras_apm *apm)
{
	return NULL;
}

static inline void cras_apm_list_set_aec_dump(struct cras_apm_list *list,
					      void *dev_ptr, int start, int fd)
{
}

#endif /* HAVE_WEBRTC_APM */

#endif /* CRAS_APM_LIST_H_ */
