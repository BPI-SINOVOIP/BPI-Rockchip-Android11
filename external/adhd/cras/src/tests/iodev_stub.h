/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef IODEV_STUB_H_
#define IODEV_STUB_H_

#include <time.h>

void iodev_stub_reset();

void iodev_stub_frames_queued(cras_iodev* iodev, int ret, timespec ts);

void iodev_stub_valid_frames(cras_iodev* iodev, int ret, timespec ts);

bool iodev_stub_get_drop_time(cras_iodev* iodev, timespec* ts);

#endif  // IODEV_STUB_H_
