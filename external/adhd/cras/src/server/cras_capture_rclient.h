/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CRAS_CAPTURE_RCLIENT_H_
#define CRAS_CAPTURE_RCLIENT_H_

struct cras_rclient;

/* Creates a capture rclient structure.
 * Args:
 *    fd - The file descriptor used for communication with the client.
 *    id - Unique identifier for this client.
 * Returns:
 *    A pointer to the newly created rclient on success, NULL on failure.
 */
struct cras_rclient *cras_capture_rclient_create(int fd, size_t id);

#endif /* CRAS_CAPTURE_RCLIENT_H_ */
