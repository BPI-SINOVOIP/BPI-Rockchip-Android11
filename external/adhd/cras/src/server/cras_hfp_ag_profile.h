/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CRAS_HFP_AG_PROFILE_H_
#define CRAS_HFP_AG_PROFILE_H_

#include <dbus/dbus.h>

#include "cras_bt_device.h"
#include "cras_hfp_slc.h"

/* The bitmap of HFP AG feature supported by CRAS */
#define CRAS_AG_SUPPORTED_FEATURES (AG_ENHANCED_CALL_STATUS)

struct hfp_slc_handle;

/* Adds a profile instance for HFP AG (Hands-Free Profile Audio Gateway). */
int cras_hfp_ag_profile_create(DBusConnection *conn);

/* Adds a profile instance for HSP AG (Headset Profile Audio Gateway). */
int cras_hsp_ag_profile_create(DBusConnection *conn);

/* Starts the HFP audio gateway for audio input/output. */
int cras_hfp_ag_start(struct cras_bt_device *device);

/*
 * Suspends all connected audio gateways except the one associated to device.
 * Used to stop previously running HFP/HSP audio when a new device is connected.
 * Args:
 *    device - The device that we want to keep connection while others should
 *        be removed.
 */
int cras_hfp_ag_remove_conflict(struct cras_bt_device *device);

/* Suspends audio gateway associated with given bt device. */
void cras_hfp_ag_suspend_connected_device(struct cras_bt_device *device);

/* Gets the active SLC handle. Used for HFP qualification. */
struct hfp_slc_handle *cras_hfp_ag_get_active_handle();

/* Gets the SLC handle for given cras_bt_device. */
struct hfp_slc_handle *cras_hfp_ag_get_slc(struct cras_bt_device *device);

#endif /* CRAS_HFP_AG_PROFILE_H_ */
