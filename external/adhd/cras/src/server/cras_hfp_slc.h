/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CRAS_HFP_SLC_H_
#define CRAS_HFP_SLC_H_

struct hfp_slc_handle;
struct cras_bt_device;

/*
 * Hands-free HFP and AG supported features bits definition.
 * Per HFP 1.7.1 specification section 4.34.1, command
 * AT+BRSF (Bluetooth Retrieve Supported Features)
 */
#define HF_EC_ANDOR_NR 0x0001
#define HF_THREE_WAY_CALLING 0x0002
#define HF_CLI_PRESENTATION_CAP 0x0004
#define HF_VOICE_RECOGNITION 0x0008
#define HF_REMOVE_VOLUME_CTONTROL 0x0010
#define HF_ENHANCED_CALL_STATUS 0x0020
#define HF_ENHANCED_CALL_CONTROL 0x0040
#define HF_CODEC_NEGOTIATION 0x0080
#define HF_HF_INDICATORS 0x0100
#define HF_ESCO_S4_T2_SETTINGS 0x0200

#define AG_THREE_WAY_CALLING 0x0001
#define AG_EC_ANDOR_NR 0x0002
#define AG_VOICE_RECOGNITION 0x0004
#define AG_INBAND_RINGTONE 0x0008
#define AG_ATTACH_NUMBER_TO_VOICETAG 0x0010
#define AG_REJECT_A_CALL 0x0020
#define AG_ENHANCED_CALL_STATUS 0x0040
#define AG_ENHANCED_CALL_CONTROL 0x0080
#define AG_EXTENDED_ERROR_RESULT_CODES 0x0100
#define AG_CODEC_NEGOTIATION 0x0200
#define AG_HF_INDICATORS 0x0400
#define AG_ESCO_S4_T2_SETTINGS 0x0800

/* Codec ids for codec negotiation, per HFP 1.7.1 spec appendix B. */
#define HFP_CODEC_UNUSED 0
#define HFP_CODEC_ID_CVSD 1
#define HFP_CODEC_ID_MSBC 2
#define HFP_MAX_CODECS 3

/* Callback to call when service level connection initialized. */
typedef int (*hfp_slc_init_cb)(struct hfp_slc_handle *handle);

/* Callback to call when service level connection disconnected. */
typedef int (*hfp_slc_disconnect_cb)(struct hfp_slc_handle *handle);

/* Creates an hfp_slc_handle to poll the RFCOMM file descriptor
 * to read and handle received AT commands.
 * Args:
 *    fd - the rfcomm fd used to initialize service level connection
 *    is_hsp - if the slc handle is created for headset profile
 *    ag_supported_features - Supported AG features bitmap.
 *    device - The bt device associated with the created slc object
 *    init_cb - the callback function to be triggered when a service level
 *        connection is initialized.
 *    disconnect_cb - the callback function to be triggered when the service
 *        level connection is disconnected.
 */
struct hfp_slc_handle *hfp_slc_create(int fd, int is_hsp,
				      int ag_supported_features,
				      struct cras_bt_device *device,
				      hfp_slc_init_cb init_cb,
				      hfp_slc_disconnect_cb disconnect_cb);

/* Destroys an hfp_slc_handle. */
void hfp_slc_destroy(struct hfp_slc_handle *handle);

/* Sets the call status to notify handsfree device. */
int hfp_set_call_status(struct hfp_slc_handle *handle, int call);

/* Fakes the incoming call event for qualification test. */
int hfp_event_incoming_call(struct hfp_slc_handle *handle, const char *number,
			    int type);

/* Handles the call status changed event.
 * AG will send notification to HF accordingly. */
int hfp_event_update_call(struct hfp_slc_handle *handle);

/* Handles the call setup status changed event.
 * AG will send notification to HF accordingly. */
int hfp_event_update_callsetup(struct hfp_slc_handle *handle);

/* Handles the call held status changed event.
 * AG will send notification to HF accordingly. */
int hfp_event_update_callheld(struct hfp_slc_handle *handle);

/* Sets battery level which is required for qualification test. */
int hfp_event_set_battery(struct hfp_slc_handle *handle, int value);

/* Sets signal strength which is required for qualification test. */
int hfp_event_set_signal(struct hfp_slc_handle *handle, int value);

/* Sets service availability which is required for qualification test. */
int hfp_event_set_service(struct hfp_slc_handle *handle, int value);

/* Sets speaker gain value to headsfree device. */
int hfp_event_speaker_gain(struct hfp_slc_handle *handle, int gain);

/* Gets the selected codec for HFP, mSBC or CVSD. */
int hfp_slc_get_selected_codec(struct hfp_slc_handle *handle);

/* Gets if the remote HF supports codec negotiation. */
int hfp_slc_get_hf_codec_negotiation_supported(struct hfp_slc_handle *handle);

#endif /* CRAS_HFP_SLC_H_ */
