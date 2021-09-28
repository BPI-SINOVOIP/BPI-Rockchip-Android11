// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <selinux/restorecon.h>

#include "cras_shm.h"

int cras_selinux_restorecon(const char *pathname)
{
	return selinux_restorecon(pathname, 0);
}
