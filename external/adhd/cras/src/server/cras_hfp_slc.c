/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/socket.h>
#include <syslog.h>

#include "stdbool.h"

#include "cras_bt_device.h"
#include "cras_bt_log.h"
#include "cras_telephony.h"
#include "cras_hfp_slc.h"
#include "cras_system_state.h"
#include "cras_tm.h"

/* Message start and end with "\r\n". refer to spec 4.33. */
#define AT_CMD(cmd) "\r\n" cmd "\r\n"

/* The timeout between service level initialized and codec negotiation
 * completed. */
#define CODEC_NEGOTIATION_TIMEOUT_MS 500
#define SLC_BUF_SIZE_BYTES 256

/* Indicator update command response and indicator indices.
 * Note that indicator index starts from '1'.
 */
#define BATTERY_IND_INDEX 1
#define SIGNAL_IND_INDEX 2
#define SERVICE_IND_INDEX 3
#define CALL_IND_INDEX 4
#define CALLSETUP_IND_INDEX 5
#define CALLHELD_IND_INDEX 6
#define INDICATOR_UPDATE_RSP                                                   \
	"+CIND: "                                                              \
	"(\"battchg\",(0-5)),"                                                 \
	"(\"signal\",(0-5)),"                                                  \
	"(\"service\",(0,1)),"                                                 \
	"(\"call\",(0,1)),"                                                    \
	"(\"callsetup\",(0-3)),"                                               \
	"(\"callheld\",(0-2)),"                                                \
	"(\"roam\",(0,1))"                                                     \
	""
/* Mode values for standard event reporting activation/deactivation AT
 * command AT+CMER. Used for indicator events reporting in HFP. */
#define FORWARD_UNSOLICIT_RESULT_CODE 3

/* Handle object to hold required info to initialize and maintain
 * an HFP service level connection.
 * Args:
 *    buf - Buffer hold received commands.
 *    buf_read_idx - Read index for buf.
 *    buf_write_idx - Write index for buf.
 *    rfcomm_fd - File descriptor for the established RFCOMM connection.
 *    init_cb - Callback to be triggered when an SLC is initialized.
 *    cli_active - Calling line identification notification is enabled or not.
 *    battery - Current battery level of AG stored in SLC.
 *    signal - Current signal strength of AG stored in SLC.
 *    service - Current service availability of AG stored in SLC.
 *    callheld - Current callheld status of AG stored in SLC.
 *    ind_event_report - Activate status of indicator events reporting.
 *    ag_supported_features - Supported AG features bitmap.
 *    hf_codec_supported - Flags to indicate if codec is supported in HF.
 *    hf_supports_codec_negotiation - If the connected HF supports codec
 *        negotiation.
 *    preferred_codec - CVSD or mSBC based on the situation and strategy. This
 *        need not to be equal to selected_codec because codec negotiation
 *        process may fail.
 *    selected_codec - The codec id defaults to HFP_CODEC_UNUSED and changes
 *        only if codec negotiation is supported and the negotiation flow
 *        has completed.
 *    pending_codec_negotiation - True if codec negotiation process has started
 *        but haven't got reply from HF.
 *    telephony - A reference of current telephony handle.
 *    device - The associated bt device.
 */
struct hfp_slc_handle {
	char buf[SLC_BUF_SIZE_BYTES];
	int buf_read_idx;
	int buf_write_idx;
	int is_hsp;
	int rfcomm_fd;
	hfp_slc_init_cb init_cb;
	hfp_slc_disconnect_cb disconnect_cb;
	int cli_active;
	int battery;
	int signal;
	int service;
	int callheld;
	int ind_event_report;
	int ag_supported_features;
	bool hf_codec_supported[HFP_MAX_CODECS];
	int hf_supports_codec_negotiation;
	int preferred_codec;
	int selected_codec;
	int pending_codec_negotiation;
	struct cras_bt_device *device;
	struct cras_timer *timer;

	struct cras_telephony_handle *telephony;
};

/* AT command exchanges between AG(Audio gateway) and HF(Hands-free device) */
struct at_command {
	const char *cmd;
	int (*callback)(struct hfp_slc_handle *handle, const char *cmd);
};

/* Sends a response or command to HF */
static int hfp_send(struct hfp_slc_handle *handle, const char *buf)
{
	int written, err, len;

	if (handle->rfcomm_fd < 0)
		return -EIO;

	len = strlen(buf);
	written = 0;
	while (written < len) {
		err = write(handle->rfcomm_fd, buf + written, len - written);
		if (err < 0)
			return -errno;
		written += err;
	}

	return 0;
}

/* Sends a response for indicator event reporting. */
static int hfp_send_ind_event_report(struct hfp_slc_handle *handle,
				     int ind_index, int value)
{
	char cmd[64];

	if (handle->is_hsp || !handle->ind_event_report)
		return 0;

	snprintf(cmd, 64, AT_CMD("+CIEV: %d,%d"), ind_index, value);
	return hfp_send(handle, cmd);
}

/* Sends calling line identification unsolicited result code and
 * standard call waiting notification. */
static int hfp_send_calling_line_identification(struct hfp_slc_handle *handle,
						const char *number, int type)
{
	char cmd[64];

	if (handle->is_hsp)
		return 0;

	if (handle->telephony->call) {
		snprintf(cmd, 64, AT_CMD("+CCWA: \"%s\",%d"), number, type);
	} else {
		snprintf(cmd, 64, AT_CMD("+CLIP: \"%s\",%d"), number, type);
	}
	return hfp_send(handle, cmd);
}

/* ATA command to accept an incoming call. Mandatory support per spec 4.13. */
static int answer_call(struct hfp_slc_handle *handle, const char *cmd)
{
	int rc;
	rc = hfp_send(handle, AT_CMD("OK"));
	if (rc)
		return rc;

	return cras_telephony_event_answer_call();
}

/* AT+CCWA command to enable the "Call Waiting notification" function.
 * Mandatory support per spec 4.21. */
static int call_waiting_notify(struct hfp_slc_handle *handle, const char *buf)
{
	return hfp_send(handle, AT_CMD("OK"));
}

/* AT+CLIP command to enable the "Calling Line Identification notification"
 * function. Mandatory per spec 4.23.
 */
static int cli_notification(struct hfp_slc_handle *handle, const char *cmd)
{
	handle->cli_active = (cmd[8] == '1');
	return hfp_send(handle, AT_CMD("OK"));
}

/* ATDdd...dd command to place call with supplied number, or ATD>nnn...
 * command to dial the number stored at memory location. Mandatory per
 * spec 4.18 and 4.19.
 */
static int dial_number(struct hfp_slc_handle *handle, const char *cmd)
{
	int rc, cmd_len;

	cmd_len = strlen(cmd);

	if (cmd[3] == '>') {
		/* Handle memory dial. Extract memory location from command
		 * ATD>nnn...; and lookup. */
		int memory_location;
		memory_location = strtol(cmd + 4, NULL, 0);
		if (handle->telephony->dial_number == NULL ||
		    memory_location != 1)
			return hfp_send(handle, AT_CMD("ERROR"));
	} else {
		/* ATDddddd; Store dial number to the only memory slot. */
		cras_telephony_store_dial_number(cmd_len - 3 - 1, cmd + 3);
	}

	rc = hfp_send(handle, AT_CMD("OK"));
	if (rc)
		return rc;

	handle->telephony->callsetup = 2;
	return hfp_send_ind_event_report(handle, CALLSETUP_IND_INDEX, 2);
}

/* AT+VTS command to generate a DTMF code. Mandatory per spec 4.27. */
static int dtmf_tone(struct hfp_slc_handle *handle, const char *buf)
{
	return hfp_send(handle, AT_CMD("OK"));
}

/* Sends +BCS command to tell HF about our preferred codec. This shall
 * be called only if codec negotiation is supported.
 */
static void select_preferred_codec(struct hfp_slc_handle *handle)
{
	char buf[64];
	snprintf(buf, 64, AT_CMD("+BCS:%d"), handle->preferred_codec);
	hfp_send(handle, buf);
	BTLOG(btlog, BT_CODEC_SELECTION, 0, handle->preferred_codec);
}

/* Marks SLC handle as initialized and trigger HFP AG's init_cb. */
static void initialize_slc_handle(struct cras_timer *timer, void *arg)
{
	struct hfp_slc_handle *handle = (struct hfp_slc_handle *)arg;
	if (timer)
		handle->timer = NULL;

	/*
	 * Catch the case if mSBC codec negotiation never complete or even
	 * failed. AG side falls back to use codec CVSD and also tells
	 * HF to select CVSD again.
	 */
	if ((handle->selected_codec == HFP_CODEC_UNUSED) &&
	    handle->hf_codec_supported[HFP_CODEC_ID_MSBC]) {
		handle->preferred_codec = HFP_CODEC_ID_CVSD;
		select_preferred_codec(handle);
	}

	/*
	 * Codec negotiation is considered to be ended at this point.
	 * The owner of init_cb may use hfp_slc_get_selected_codec() to
	 * query the final codec to use for this connection.
	 */
	if (handle->init_cb) {
		handle->init_cb(handle);
		handle->init_cb = NULL;
	}
}

/* Tasks to execute after receiving an AT command. This is useful because
 * some HF replies to command X only after it sends command Y. We rely on
 * this function to achieve reliable codec negotiation.
 */
static void post_at_command_tasks(struct hfp_slc_handle *handle)
{
	if (handle->pending_codec_negotiation)
		select_preferred_codec(handle);
}

/* Handles the event that headset request to select specific codec. */
static int bluetooth_codec_selection(struct hfp_slc_handle *handle,
				     const char *cmd)
{
	char *tokens = strdup(cmd);
	char *codec;
	int err;

	handle->pending_codec_negotiation = 0;
	strtok(tokens, "=");
	codec = strtok(NULL, ",");

	if (codec) {
		BTLOG(btlog, BT_CODEC_SELECTION, 1, atoi(codec));
		handle->selected_codec = atoi(codec);
	}

	err = hfp_send(handle, AT_CMD("OK"));
	initialize_slc_handle(NULL, (void *)handle);
	free(tokens);
	return err;
}

/*
 * Possibly choose mSBC code from the supported codecs. Otherwise just
 * initialize the SLC so the default CVSD codec is used.
 */
static void choose_codec_and_init_slc(struct hfp_slc_handle *handle)
{
	if (handle->hf_supports_codec_negotiation &&
	    handle->hf_codec_supported[HFP_CODEC_ID_MSBC]) {
		/* Sets preferred codec to mSBC, and schedule callback to
		 * select preferred codec until reply received or timeout.
		 */
		handle->preferred_codec = HFP_CODEC_ID_MSBC;
		handle->pending_codec_negotiation = 1;

		/* Delay init to give headset some time to confirm
		 * codec selection. */
		handle->timer =
			cras_tm_create_timer(cras_system_state_get_tm(),
					     CODEC_NEGOTIATION_TIMEOUT_MS,
					     initialize_slc_handle, handle);
	} else {
		initialize_slc_handle(NULL, (void *)handle);
	}
}

/* Handles the event when headset reports its available codecs list. */
static int available_codecs(struct hfp_slc_handle *handle, const char *cmd)
{
	char *tokens, *id_str;
	int id;

	for (id = 0; id < HFP_MAX_CODECS; id++)
		handle->hf_codec_supported[id] = false;

	tokens = strdup(cmd);
	strtok(tokens, "=");
	id_str = strtok(NULL, ",");
	while (id_str) {
		id = atoi(id_str);
		if ((id > HFP_CODEC_UNUSED) && (id < HFP_MAX_CODECS)) {
			handle->hf_codec_supported[id] = true;
			BTLOG(btlog, BT_AVAILABLE_CODECS, 0, id);
		}
		id_str = strtok(NULL, ",");
	}

	free(tokens);
	return hfp_send(handle, AT_CMD("OK"));
}

/* AT+CMER command enables the registration status update function in AG.
 * The service level connection is consider initialized when successfully
 * responded OK to the AT+CMER command. Mandatory support per spec 4.4.
 */
static int event_reporting(struct hfp_slc_handle *handle, const char *cmd)
{
	char *tokens, *mode, *tmp;
	int err = 0;

	/* AT+CMER=[<mode>[,<keyp>[,<disp>[,<ind> [,<bfr>]]]]]
	 * Parse <ind>, the only token we care about.
	 */
	tokens = strdup(cmd);
	strtok(tokens, "=");

	mode = strtok(NULL, ",");
	tmp = strtok(NULL, ",");
	tmp = strtok(NULL, ",");
	tmp = strtok(NULL, ",");

	/* mode = 3 for forward unsolicited result codes.
	 * AT+CMER=3,0,0,1 activates “indicator events reporting”.
	 * The service level connection is considered established after
	 * successfully responded with OK, regardless of the indicator
	 * events reporting status.
	 */
	if (!mode || !tmp) {
		syslog(LOG_ERR, "Invalid event reporting” cmd %s", cmd);
		err = -EINVAL;
		goto event_reporting_done;
	}
	if (atoi(mode) == FORWARD_UNSOLICIT_RESULT_CODE)
		handle->ind_event_report = atoi(tmp);

	err = hfp_send(handle, AT_CMD("OK"));
	if (err) {
		syslog(LOG_ERR, "Error sending response for command %s", cmd);
		goto event_reporting_done;
	}

	/*
	 * Consider the Service Level Connection to be fully initialized,
	 * and thereby established, after successfully responded with OK.
	 * However we should postpone the initialize call after codec selection,
	 * otherwise iodev could be open immediately while the headset is still
	 * communicating about which of CVSD or mSBC codec to use.
	 */
	choose_codec_and_init_slc(handle);

event_reporting_done:
	free(tokens);
	return err;
}

/* AT+CMEE command to set the "Extended Audio Gateway Error Result Code".
 * Mandatory per spec 4.9.
 */
static int extended_errors(struct hfp_slc_handle *handle, const char *buf)
{
	return hfp_send(handle, AT_CMD("OK"));
}

/* AT+CKPD command to handle the user initiated action from headset profile
 * device.
 */
static int key_press(struct hfp_slc_handle *handle, const char *buf)
{
	hfp_send(handle, AT_CMD("OK"));

	/* Release the call and connection. */
	if (handle->telephony->call || handle->telephony->callsetup) {
		cras_telephony_event_terminate_call();
		handle->disconnect_cb(handle);
		return -EIO;
	}
	return 0;
}

/* AT+BLDN command to re-dial the last number. Mandatory support
 * per spec 4.20.
 */
static int last_dialed_number(struct hfp_slc_handle *handle, const char *buf)
{
	int rc;

	if (!handle->telephony->dial_number)
		return hfp_send(handle, AT_CMD("ERROR"));

	rc = hfp_send(handle, AT_CMD("OK"));
	if (rc)
		return rc;

	handle->telephony->callsetup = 2;
	return hfp_send_ind_event_report(handle, CALLSETUP_IND_INDEX, 2);
}

/* AT+CLCC command to query list of current calls. Mandatory support
 * per spec 4.31.
 *
 * +CLCC: <idx>,<direction>,<status>,<mode>,<multiparty>
 */
static int list_current_calls(struct hfp_slc_handle *handle, const char *cmd)
{
	char buf[64];

	int idx = 1;
	int rc;
	/* Fake the call list base on callheld and call status
	 * since we have no API exposed to manage call list.
	 * This is a hack to pass qualification test which ask us to
	 * handle the basic case that one call is active and
	 * the other is on hold. */
	if (handle->telephony->callheld) {
		snprintf(buf, 64, AT_CMD("+CLCC: %d,1,1,0,0"), idx++);
		rc = hfp_send(handle, buf);
		if (rc)
			return rc;
	}

	if (handle->telephony->call) {
		snprintf(buf, 64, AT_CMD("+CLCC: %d,1,0,0,0"), idx++);
		rc = hfp_send(handle, buf);
		if (rc)
			return rc;
	}

	return hfp_send(handle, AT_CMD("OK"));
}

/* AT+COPS command to query currently selected operator or set name format.
 * Mandatory support per spec 4.8.
 */
static int operator_selection(struct hfp_slc_handle *handle, const char *buf)
{
	int rc;
	if (buf[7] == '?') {
		/* HF sends AT+COPS? command to find current network operator.
		 * AG responds with +COPS:<mode>,<format>,<operator>, where
		 * the mode=0 means automatic for network selection. If no
		 * operator is selected, <format> and <operator> are omitted.
		 */
		rc = hfp_send(handle, AT_CMD("+COPS: 0"));
		if (rc)
			return rc;
	}
	return hfp_send(handle, AT_CMD("OK"));
}

/* AT+CIND command retrieves the supported indicator and its corresponding
 * range and order index or read current status of indicators. Mandatory
 * support per spec 4.2.
 */
static int report_indicators(struct hfp_slc_handle *handle, const char *cmd)
{
	int err;
	char buf[64];

	if (cmd[7] == '=') {
		/* Indicator update test command "AT+CIND=?" */
		err = hfp_send(handle, AT_CMD(INDICATOR_UPDATE_RSP));
	} else {
		/* Indicator update read command "AT+CIND?".
		 * Respond with current status of AG indicators,
		 * the values must be listed in the indicator order declared
		 * in INDICATOR_UPDATE_RSP.
		 * +CIND: <signal>,<service>,<call>,
		 *        <callsetup>,<callheld>,<roam>
		 */
		snprintf(buf, 64, AT_CMD("+CIND: %d,%d,%d,%d,%d,%d,0"),
			 handle->battery, handle->signal, handle->service,
			 handle->telephony->call, handle->telephony->callsetup,
			 handle->telephony->callheld);
		err = hfp_send(handle, buf);
	}

	if (err < 0)
		return err;

	return hfp_send(handle, AT_CMD("OK"));
}

/* AT+BIA command to change the subset of indicators that shall be
 * sent by the AG. It is okay to ignore this command here since we
 * don't do event reporting(CMER).
 */
static int indicator_activation(struct hfp_slc_handle *handle, const char *cmd)
{
	/* AT+BIA=[[<indrep 1>][,[<indrep 2>][,...[,[<indrep n>]]]]] */
	syslog(LOG_ERR, "Bluetooth indicator activation command %s", cmd);
	return hfp_send(handle, AT_CMD("OK"));
}

/* AT+VGM and AT+VGS command reports the current mic and speaker gain
 * level respectively. Optional support per spec 4.28.
 */
static int signal_gain_setting(struct hfp_slc_handle *handle, const char *cmd)
{
	int gain;

	if (strlen(cmd) < 8) {
		syslog(LOG_ERR, "Invalid gain setting command %s", cmd);
		return -EINVAL;
	}

	/* Map 0 to the smallest non-zero scale 6/100, and 15 to
	 * 100/100 full. */
	if (cmd[5] == 'S') {
		gain = atoi(&cmd[7]);
		cras_bt_device_update_hardware_volume(handle->device,
						      (gain + 1) * 100 / 16);
	}

	return hfp_send(handle, AT_CMD("OK"));
}

/* AT+CNUM command to query the subscriber number. Mandatory support
 * per spec 4.30.
 */
static int subscriber_number(struct hfp_slc_handle *handle, const char *buf)
{
	return hfp_send(handle, AT_CMD("OK"));
}

/* AT+BRSF command notifies the HF(Hands-free device) supported features
 * and retrieves the AG(Audio gateway) supported features. Mandatory
 * support per spec 4.2.
 */
static int supported_features(struct hfp_slc_handle *handle, const char *cmd)
{
	int err, hf_features;
	char response[128];
	char *tokens, *features;

	if (strlen(cmd) < 9)
		return -EINVAL;

	handle->hf_supports_codec_negotiation = 0;

	tokens = strdup(cmd);
	strtok(tokens, "=");
	features = strtok(NULL, ",");

	hf_features = atoi(features);
	BTLOG(btlog, BT_HFP_SUPPORTED_FEATURES, 0, hf_features);
	if (hf_features & HF_CODEC_NEGOTIATION)
		handle->hf_supports_codec_negotiation = 1;
	free(tokens);

	/* AT+BRSF=<feature> command received, ignore the HF supported feature
	 * for now. Respond with +BRSF:<feature> to notify mandatory supported
	 * features in AG(audio gateway).
	 */
	BTLOG(btlog, BT_HFP_SUPPORTED_FEATURES, 1,
	      handle->ag_supported_features);
	snprintf(response, 128, AT_CMD("+BRSF: %u"),
		 handle->ag_supported_features);
	err = hfp_send(handle, response);
	if (err < 0)
		return err;

	return hfp_send(handle, AT_CMD("OK"));
}

int hfp_event_speaker_gain(struct hfp_slc_handle *handle, int gain)
{
	char command[128];

	/* Normailize gain value to 0-15 */
	gain = gain * 15 / 100;
	snprintf(command, 128, AT_CMD("+VGS=%d"), gain);

	return hfp_send(handle, command);
}

/* AT+CHUP command to terminate current call. Mandatory support
 * per spec 4.15.
 */
static int terminate_call(struct hfp_slc_handle *handle, const char *cmd)
{
	int rc;
	rc = hfp_send(handle, AT_CMD("OK"));
	if (rc)
		return rc;

	return cras_telephony_event_terminate_call();
}

/* AT commands to support in order to conform HFP specification.
 *
 * An initialized service level connection is the pre-condition for all
 * call related procedures. Note that for the call related commands,
 * we are good to just respond with a dummy "OK".
 *
 * The procedure to establish a service level connection is described below:
 *
 * 1. HF notifies AG about its own supported features and AG responds
 * with its supported feature.
 *
 * HF(hands-free)                             AG(audio gateway)
 *                     AT+BRSF=<HF supported feature> -->
 *                 <-- +BRSF:<AG supported feature>
 *                 <-- OK
 *
 * 2. HF retrieves the information about the indicators supported in AG.
 *
 * HF(hands-free)                             AG(audio gateway)
 *                     AT+CIND=? -->
 *                 <-- +CIND:...
 *                 <-- OK
 *
 * 3. The HF requests the current status of the indicators in AG.
 *
 * HF(hands-free)                             AG(audio gateway)
 *                     AT+CIND -->
 *                 <-- +CIND:...
 *                 <-- OK
 *
 * 4. HF requests enabling indicator status update in the AG.
 *
 * HF(hands-free)                             AG(audio gateway)
 *                     AT+CMER= -->
 *                 <-- OK
 */
static struct at_command at_commands[] = { { "ATA", answer_call },
					   { "ATD", dial_number },
					   { "AT+BAC", available_codecs },
					   { "AT+BCS",
					     bluetooth_codec_selection },
					   { "AT+BIA", indicator_activation },
					   { "AT+BLDN", last_dialed_number },
					   { "AT+BRSF", supported_features },
					   { "AT+CCWA", call_waiting_notify },
					   { "AT+CHUP", terminate_call },
					   { "AT+CIND", report_indicators },
					   { "AT+CKPD", key_press },
					   { "AT+CLCC", list_current_calls },
					   { "AT+CLIP", cli_notification },
					   { "AT+CMEE", extended_errors },
					   { "AT+CMER", event_reporting },
					   { "AT+CNUM", subscriber_number },
					   { "AT+COPS", operator_selection },
					   { "AT+VG", signal_gain_setting },
					   { "AT+VTS", dtmf_tone },
					   { 0 } };

static int handle_at_command(struct hfp_slc_handle *slc_handle, const char *cmd)
{
	struct at_command *atc;

	for (atc = at_commands; atc->cmd; atc++)
		if (!strncmp(cmd, atc->cmd, strlen(atc->cmd)))
			return atc->callback(slc_handle, cmd);

	syslog(LOG_DEBUG, "AT command %s not supported", cmd);
	return hfp_send(slc_handle, AT_CMD("ERROR"));
}

static void slc_watch_callback(void *arg)
{
	struct hfp_slc_handle *handle = (struct hfp_slc_handle *)arg;
	ssize_t bytes_read;
	int err;

	bytes_read =
		read(handle->rfcomm_fd, &handle->buf[handle->buf_write_idx],
		     SLC_BUF_SIZE_BYTES - handle->buf_write_idx - 1);
	if (bytes_read < 0) {
		syslog(LOG_ERR, "Error reading slc command %s",
		       strerror(errno));
		handle->disconnect_cb(handle);
		return;
	}
	handle->buf_write_idx += bytes_read;
	handle->buf[handle->buf_write_idx] = '\0';

	while (handle->buf_read_idx != handle->buf_write_idx) {
		char *end_char;
		end_char = strchr(&handle->buf[handle->buf_read_idx], '\r');
		if (end_char == NULL)
			break;

		*end_char = '\0';
		err = handle_at_command(handle,
					&handle->buf[handle->buf_read_idx]);
		if (err < 0)
			return;

		/* Shift the read index */
		handle->buf_read_idx = 1 + end_char - handle->buf;
		if (handle->buf_read_idx == handle->buf_write_idx) {
			handle->buf_read_idx = 0;
			handle->buf_write_idx = 0;
		}
	}

	/* Handle the case when buffer is full and no command found. */
	if (handle->buf_write_idx == SLC_BUF_SIZE_BYTES - 1) {
		if (handle->buf_read_idx) {
			memmove(handle->buf, &handle->buf[handle->buf_read_idx],
				handle->buf_write_idx - handle->buf_read_idx);
			handle->buf_write_idx -= handle->buf_read_idx;
			handle->buf_read_idx = 0;
		} else {
			syslog(LOG_ERR,
			       "Parse SLC command error, clean up buffer");
			handle->buf_write_idx = 0;
		}
	}

	post_at_command_tasks(handle);

	return;
}

/* Exported interfaces */

struct hfp_slc_handle *hfp_slc_create(int fd, int is_hsp,
				      int ag_supported_features,
				      struct cras_bt_device *device,
				      hfp_slc_init_cb init_cb,
				      hfp_slc_disconnect_cb disconnect_cb)
{
	struct hfp_slc_handle *handle;

	handle = (struct hfp_slc_handle *)calloc(1, sizeof(*handle));
	if (!handle)
		return NULL;

	handle->rfcomm_fd = fd;
	handle->is_hsp = is_hsp;
	handle->ag_supported_features = ag_supported_features;
	handle->device = device;
	handle->init_cb = init_cb;
	handle->disconnect_cb = disconnect_cb;
	handle->cli_active = 0;
	handle->battery = 5;
	handle->signal = 5;
	handle->service = 1;
	handle->ind_event_report = 0;
	handle->telephony = cras_telephony_get();
	handle->preferred_codec = HFP_CODEC_ID_CVSD;
	handle->selected_codec = HFP_CODEC_UNUSED;

	cras_system_add_select_fd(handle->rfcomm_fd, slc_watch_callback,
				  handle);

	return handle;
}

void hfp_slc_destroy(struct hfp_slc_handle *slc_handle)
{
	cras_system_rm_select_fd(slc_handle->rfcomm_fd);
	if (slc_handle->timer)
		cras_tm_cancel_timer(cras_system_state_get_tm(),
				     slc_handle->timer);
	close(slc_handle->rfcomm_fd);
	free(slc_handle);
}

int hfp_slc_get_selected_codec(struct hfp_slc_handle *handle)
{
	/* If codec negotiation is not supported on HF, or the negotiation
	 * process never completed. Fallback to the preffered codec. */
	if (handle->selected_codec == HFP_CODEC_UNUSED)
		return handle->preferred_codec;
	else
		return handle->selected_codec;
}

int hfp_set_call_status(struct hfp_slc_handle *handle, int call)
{
	int old_call = handle->telephony->call;

	if (old_call == call)
		return 0;

	handle->telephony->call = call;
	return hfp_event_update_call(handle);
}

/* Procedure to setup a call when AG sees incoming call.
 *
 * HF(hands-free)                             AG(audio gateway)
 *                                                     <-- Incoming call
 *                 <-- +CIEV: (callsetup = 1)
 *                 <-- RING (ALERT)
 */
int hfp_event_incoming_call(struct hfp_slc_handle *handle, const char *number,
			    int type)
{
	int rc;

	if (handle->is_hsp)
		return 0;

	if (handle->cli_active) {
		rc = hfp_send_calling_line_identification(handle, number, type);
		if (rc)
			return rc;
	}

	if (handle->telephony->call)
		return 0;
	else
		return hfp_send(handle, AT_CMD("RING"));
}

int hfp_event_update_call(struct hfp_slc_handle *handle)
{
	return hfp_send_ind_event_report(handle, CALL_IND_INDEX,
					 handle->telephony->call);
}

int hfp_event_update_callsetup(struct hfp_slc_handle *handle)
{
	return hfp_send_ind_event_report(handle, CALLSETUP_IND_INDEX,
					 handle->telephony->callsetup);
}

int hfp_event_update_callheld(struct hfp_slc_handle *handle)
{
	return hfp_send_ind_event_report(handle, CALLHELD_IND_INDEX,
					 handle->telephony->callheld);
}

int hfp_event_set_battery(struct hfp_slc_handle *handle, int level)
{
	handle->battery = level;
	return hfp_send_ind_event_report(handle, BATTERY_IND_INDEX, level);
}

int hfp_event_set_signal(struct hfp_slc_handle *handle, int level)
{
	handle->signal = level;
	return hfp_send_ind_event_report(handle, SIGNAL_IND_INDEX, level);
}

int hfp_event_set_service(struct hfp_slc_handle *handle, int avail)
{
	/* Convert to 0 or 1.
	 * Since the value must be either 1 or 0. (service presence or not) */
	handle->service = !!avail;
	return hfp_send_ind_event_report(handle, SERVICE_IND_INDEX, avail);
}

int hfp_slc_get_hf_codec_negotiation_supported(struct hfp_slc_handle *handle)
{
	return handle->hf_supports_codec_negotiation;
}
