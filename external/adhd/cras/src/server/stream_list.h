/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>

#include "cras_types.h"
#include "utlist.h"

struct cras_rclient;
struct cras_rstream;
struct cras_rstream_config;
struct cras_audio_format;
struct stream_list;

typedef int(stream_callback)(struct cras_rstream *rstream);
/* This function will mutably borrow stream_config. */
typedef int(stream_create_func)(struct cras_rstream_config *stream_config,
				struct cras_rstream **rstream);
typedef void(stream_destroy_func)(struct cras_rstream *rstream);

struct stream_list *stream_list_create(stream_callback *add_cb,
				       stream_callback *rm_cb,
				       stream_create_func *create_cb,
				       stream_destroy_func *destroy_cb,
				       struct cras_tm *timer_manager);

void stream_list_destroy(struct stream_list *list);

struct cras_rstream *stream_list_get(struct stream_list *list);

/* Creates a cras_rstream from cras_rstreaem_config and adds the cras_rstream
 * to stream_list.
 *
 * Args:
 *   list - stream_list to add streams.
 *   stream_config - A mutable borrow of cras_rstream_config.
 *   stream - A pointer to place created cras_rstream.
 *
 * Returns:
 *   0 on success. Negative error code on failure.
 */
int stream_list_add(struct stream_list *list,
		    struct cras_rstream_config *stream_config,
		    struct cras_rstream **stream);

int stream_list_rm(struct stream_list *list, cras_stream_id_t id);

int stream_list_rm_all_client_streams(struct stream_list *list,
				      struct cras_rclient *rclient);

/*
 * Checks if there is a stream pinned to the given device.
 */
bool stream_list_has_pinned_stream(struct stream_list *list,
				   unsigned int dev_idx);
