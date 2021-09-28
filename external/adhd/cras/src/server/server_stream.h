/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SERVER_STREAM_H_
#define SERVER_STREAM_H_

struct stream_list;

/*
 * Asynchronously creates a server stream pinned to device of given idx.
 * Args:
 *    stream_list - List of stream to add new server stream to.
 *    dev_idx - The id of the device that new server stream will pin to.
 */
void server_stream_create(struct stream_list *stream_list,
			  unsigned int dev_idx);

/*
 * Asynchronously destroys existing server stream pinned to device of given idx.
 * Args:
 *    stream_list - List of stream to look up server stream.
 *    dev_idx - The device id that target server stream is pinned to.
 **/
void server_stream_destroy(struct stream_list *stream_list,
			   unsigned int dev_idx);

#endif /* SERVER_STREAM_H_ */
