/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

extern "C" {
#include "cras_iodev_list.h"
#include "cras_mix.h"
#include "cras_observer.h"
#include "cras_rclient.h"
#include "cras_shm.h"
#include "cras_system_state.h"
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  cras_rclient* client = cras_rclient_create(0, 0, CRAS_CONTROL);
  cras_rclient_buffer_from_client(client, data, size, NULL, 0);
  cras_rclient_destroy(client);

  return 0;
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  char* shm_name;
  if (asprintf(&shm_name, "/cras-%d", getpid()) < 0)
    exit(-ENOMEM);
  struct cras_server_state* exp_state =
      (struct cras_server_state*)calloc(1, sizeof(*exp_state));
  if (!exp_state)
    exit(-1);
  int rw_shm_fd = open("/dev/null", O_RDWR);
  int ro_shm_fd = open("/dev/null", O_RDONLY);
  cras_system_state_init("/tmp", shm_name, rw_shm_fd, ro_shm_fd, exp_state,
                         sizeof(*exp_state));
  free(shm_name);

  cras_observer_server_init();
  cras_mix_init(0);
  cras_iodev_list_init();

  return 0;
}
