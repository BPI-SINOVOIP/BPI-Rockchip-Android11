/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CRAS_HFP_ALSA_IODEV_H_
#define CRAS_HFP_ALSA_IODEV_H_

#include "cras_bt_device.h"
#include "cras_hfp_info.h"
#include "cras_types.h"

struct hfp_slc_handle;

/*
 * Creates a hfp alsa iodev.
 *
 * hfp_alsa_iodev is a special HFP iodev which would be managed by bt_io but
 * playback/capture via an inner ALSA iodev.
 *
 * The usage of hfp_alsa_iodev is only for SCO connection over PCM/I2S.
 */
struct cras_iodev *hfp_alsa_iodev_create(struct cras_iodev *aio,
					 struct cras_bt_device *device,
					 struct hfp_slc_handle *slc,
					 enum cras_bt_device_profile profile);

void hfp_alsa_iodev_destroy(struct cras_iodev *iodev);

#endif /* CRAS_HFP_ALSA_IODEV_H_ */
