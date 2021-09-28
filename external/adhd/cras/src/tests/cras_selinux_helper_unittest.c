// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cras_shm.h"

/* Define a stub cras_selinux_restorecon() which doesn't do anything */
int cras_selinux_restorecon(const char* pathname) {
  return 0;
}
