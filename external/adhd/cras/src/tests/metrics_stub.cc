/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

extern "C" {
#include "cras_server_metrics.h"

int cras_server_metrics_device_runtime(struct cras_iodev* iodev) {
  return 0;
}

int cras_server_metrics_highest_device_delay(
    unsigned int hw_level,
    unsigned int largest_cb_level,
    enum CRAS_STREAM_DIRECTION direction) {
  return 0;
}

int cras_server_metrics_highest_hw_level(unsigned hw_level,
                                         enum CRAS_STREAM_DIRECTION direction) {
  return 0;
}

int cras_server_metrics_longest_fetch_delay(unsigned delay_msec) {
  return 0;
}

int cras_server_metrics_missed_cb_event(const struct cras_rstream* stream) {
  return 0;
}

int cras_server_metrics_num_underruns(unsigned num_underruns) {
  return 0;
}

int cras_server_metrics_hfp_wideband_support(bool supported) {
  return 0;
}

int cras_server_metrics_hfp_packet_loss(float packet_loss_ratio) {
  return 0;
}

int cras_server_metrics_busyloop(struct timespec* ts, unsigned count) {
  return 0;
}

}  // extern "C"
