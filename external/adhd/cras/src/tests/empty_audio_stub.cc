/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

extern "C" {
#include "polled_interval_checker.h"

struct polled_interval* pic_polled_interval_create(int interval_sec) {
  return NULL;
}

int pic_interval_elapsed(const struct polled_interval* interval) {
  return 0;
}

void pic_interval_reset(struct polled_interval* interval) {}

void pic_polled_interval_destroy(struct polled_interval** interval) {}

void pic_update_current_time() {}

void cras_non_empty_audio_send_msg(int non_empty) {}
}
