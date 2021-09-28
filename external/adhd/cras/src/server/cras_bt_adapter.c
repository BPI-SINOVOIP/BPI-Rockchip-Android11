/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <dbus/dbus.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "bluetooth.h"
#include "cras_bt_adapter.h"
#include "cras_bt_constants.h"
#include "utlist.h"

/*
 * Object to represent a bluetooth adapter on the system. Used to query the
 * capabilities regarding certain bluetooth audio.
 * Members:
 *    conn - The dbus connection used to send message to bluetoothd.
 *    object_path - Object path of the bluetooth adapter.
 *    address - The BT address of this adapter.
 *    name - The readable name of this adapter.
 *    bluetooth_class - The bluetooth class of device.
 *    powered - Powered on or off.
 *    bus_type - Type of bus this adapter runs on.
 *    wide_band_speech - If this adapter supports wide band speech.
 */
struct cras_bt_adapter {
	DBusConnection *conn;
	char *object_path;
	char *address;
	char *name;
	uint32_t bluetooth_class;
	int powered;
	int bus_type;
	int wide_band_speech;

	struct cras_bt_adapter *prev, *next;
};

static struct cras_bt_adapter *adapters;

static int cras_bt_adapter_query_bus_type(struct cras_bt_adapter *adapter)
{
	static const char *hci_str = "hci";
	struct hci_dev_info dev_info;
	char *pos;
	int ctl, err;

	/* Object path [variable prefix]/{hci0,hci1,...} */
	pos = strstr(adapter->object_path, hci_str);
	if (!pos)
		return -1;

	ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
	if (ctl < 0) {
		syslog(LOG_ERR, "Error creating HCI ctl socket");
		return -1;
	}

	/* dev_id = 0 for hci0 */
	dev_info.type = 0;
	dev_info.dev_id = atoi(pos + 3);
	err = ioctl(ctl, HCIGETDEVINFO, (void *)&dev_info);
	if (err) {
		syslog(LOG_ERR, "HCI get dev info error %s", strerror(errno));
		close(ctl);
		return -1;
	}
	if ((dev_info.type & 0x0f) < HCI_BUS_MAX)
		adapter->bus_type = (dev_info.type & 0x0f);

	close(ctl);
	return 0;
}

struct cras_bt_adapter *cras_bt_adapter_create(DBusConnection *conn,
					       const char *object_path)
{
	struct cras_bt_adapter *adapter;

	adapter = calloc(1, sizeof(*adapter));
	if (adapter == NULL)
		return NULL;

	adapter->conn = conn;
	adapter->object_path = strdup(object_path);
	if (adapter->object_path == NULL) {
		free(adapter);
		return NULL;
	}

	DL_APPEND(adapters, adapter);

	/* Set bus type to USB as default when query fails. */
	if (cras_bt_adapter_query_bus_type(adapter))
		adapter->bus_type = HCI_USB;

	return adapter;
}

void cras_bt_adapter_destroy(struct cras_bt_adapter *adapter)
{
	DL_DELETE(adapters, adapter);

	free(adapter->object_path);
	free(adapter->address);
	free(adapter->name);
	free(adapter);
}

void cras_bt_adapter_reset()
{
	while (adapters) {
		syslog(LOG_INFO, "Bluetooth Adapter: %s removed",
		       adapters->address);
		cras_bt_adapter_destroy(adapters);
	}
}

struct cras_bt_adapter *cras_bt_adapter_get(const char *object_path)
{
	struct cras_bt_adapter *adapter;

	if (object_path == NULL)
		return NULL;

	DL_FOREACH (adapters, adapter) {
		if (strcmp(adapter->object_path, object_path) == 0)
			return adapter;
	}

	return NULL;
}

size_t cras_bt_adapter_get_list(struct cras_bt_adapter ***adapter_list_out)
{
	struct cras_bt_adapter *adapter;
	struct cras_bt_adapter **adapter_list = NULL;
	size_t num_adapters = 0;

	DL_FOREACH (adapters, adapter) {
		struct cras_bt_adapter **tmp;

		tmp = realloc(adapter_list,
			      sizeof(adapter_list[0]) * (num_adapters + 1));
		if (!tmp) {
			free(adapter_list);
			return -ENOMEM;
		}

		adapter_list = tmp;
		adapter_list[num_adapters++] = adapter;
	}

	*adapter_list_out = adapter_list;
	return num_adapters;
}

const char *cras_bt_adapter_object_path(const struct cras_bt_adapter *adapter)
{
	return adapter->object_path;
}

const char *cras_bt_adapter_address(const struct cras_bt_adapter *adapter)
{
	return adapter->address;
}

const char *cras_bt_adapter_name(const struct cras_bt_adapter *adapter)
{
	return adapter->name;
}

int cras_bt_adapter_powered(const struct cras_bt_adapter *adapter)
{
	return adapter->powered;
}

int cras_bt_adapter_wbs_supported(struct cras_bt_adapter *adapter)
{
	return adapter->wide_band_speech;
}

static void bt_adapter_set_powered(struct cras_bt_adapter *adapter, int powered)
{
	adapter->powered = powered;
	if (powered)
		cras_bt_adapter_get_supported_capabilities(adapter);
}

void cras_bt_adapter_update_properties(struct cras_bt_adapter *adapter,
				       DBusMessageIter *properties_array_iter,
				       DBusMessageIter *invalidated_array_iter)
{
	while (dbus_message_iter_get_arg_type(properties_array_iter) !=
	       DBUS_TYPE_INVALID) {
		DBusMessageIter properties_dict_iter, variant_iter;
		const char *key;
		int type;

		dbus_message_iter_recurse(properties_array_iter,
					  &properties_dict_iter);

		dbus_message_iter_get_basic(&properties_dict_iter, &key);
		dbus_message_iter_next(&properties_dict_iter);

		dbus_message_iter_recurse(&properties_dict_iter, &variant_iter);
		type = dbus_message_iter_get_arg_type(&variant_iter);

		if (type == DBUS_TYPE_STRING) {
			const char *value;

			dbus_message_iter_get_basic(&variant_iter, &value);

			if (strcmp(key, "Address") == 0) {
				free(adapter->address);
				adapter->address = strdup(value);

			} else if (strcmp(key, "Alias") == 0) {
				free(adapter->name);
				adapter->name = strdup(value);
			}

		} else if (type == DBUS_TYPE_UINT32) {
			uint32_t value;

			dbus_message_iter_get_basic(&variant_iter, &value);

			if (strcmp(key, "Class") == 0)
				adapter->bluetooth_class = value;

		} else if (type == DBUS_TYPE_BOOLEAN) {
			int value;

			dbus_message_iter_get_basic(&variant_iter, &value);

			if (strcmp(key, "Powered") == 0)
				bt_adapter_set_powered(adapter, value);
		}

		dbus_message_iter_next(properties_array_iter);
	}

	while (invalidated_array_iter &&
	       dbus_message_iter_get_arg_type(invalidated_array_iter) !=
		       DBUS_TYPE_INVALID) {
		const char *key;

		dbus_message_iter_get_basic(invalidated_array_iter, &key);

		if (strcmp(key, "Address") == 0) {
			free(adapter->address);
			adapter->address = NULL;
		} else if (strcmp(key, "Alias") == 0) {
			free(adapter->name);
			adapter->name = NULL;
		} else if (strcmp(key, "Class") == 0) {
			adapter->bluetooth_class = 0;
		} else if (strcmp(key, "Powered") == 0) {
			adapter->powered = 0;
		}

		dbus_message_iter_next(invalidated_array_iter);
	}
}

int cras_bt_adapter_on_usb(struct cras_bt_adapter *adapter)
{
	return !!(adapter->bus_type == HCI_USB);
}

/*
 * Expect to receive supported capabilities in reply, like below format:
 * array [
 *   dict entry(
 *     string "wide band speech"
 *     variant
 *       boolean <value>
 *   )
 * ]
 */
static void on_get_supported_capabilities_reply(DBusPendingCall *pending_call,
						void *data)
{
	DBusMessage *reply;
	DBusMessageIter message_iter, capabilities;
	struct cras_bt_adapter *adapter;

	reply = dbus_pending_call_steal_reply(pending_call);
	dbus_pending_call_unref(pending_call);

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		syslog(LOG_ERR,
		       "GetSupportedCapabilities message replied error: %s",
		       dbus_message_get_error_name(reply));
		goto get_supported_capabilities_err;
	}

	if (!dbus_message_iter_init(reply, &message_iter)) {
		syslog(LOG_ERR, "GetSupportedCapabilities reply doesn't have"
				"argument");
		goto get_supported_capabilities_err;
	}

	DL_FOREACH (adapters, adapter) {
		if (adapter == (struct cras_bt_adapter *)data)
			break;
	}
	if (NULL == adapter)
		goto get_supported_capabilities_err;

	dbus_message_iter_recurse(&message_iter, &capabilities);

	while (dbus_message_iter_get_arg_type(&capabilities) !=
	       DBUS_TYPE_INVALID) {
		DBusMessageIter cap_dict_iter, variant_iter;
		const char *key;
		int type;

		dbus_message_iter_recurse(&capabilities, &cap_dict_iter);

		dbus_message_iter_get_basic(&cap_dict_iter, &key);
		dbus_message_iter_next(&cap_dict_iter);

		dbus_message_iter_recurse(&cap_dict_iter, &variant_iter);
		type = dbus_message_iter_get_arg_type(&variant_iter);

		if (type == DBUS_TYPE_BOOLEAN) {
			int value;

			dbus_message_iter_get_basic(&variant_iter, &value);

			if (strcmp(key, "wide band speech") == 0)
				adapter->wide_band_speech = value;
		}

		dbus_message_iter_next(&capabilities);
	}

get_supported_capabilities_err:
	dbus_message_unref(reply);
}

int cras_bt_adapter_get_supported_capabilities(struct cras_bt_adapter *adapter)
{
	DBusMessage *method_call;
	DBusError dbus_error;
	DBusPendingCall *pending_call;

	method_call = dbus_message_new_method_call(BLUEZ_SERVICE,
						   adapter->object_path,
						   BLUEZ_INTERFACE_ADAPTER,
						   "GetSupportedCapabilities");
	if (!method_call)
		return -ENOMEM;

	dbus_error_init(&dbus_error);
	if (!dbus_connection_send_with_reply(adapter->conn, method_call,
					     &pending_call,
					     DBUS_TIMEOUT_USE_DEFAULT)) {
		dbus_message_unref(method_call);
		syslog(LOG_ERR,
		       "Failed to send GetSupportedCapabilities message");
		return -EIO;
	}

	dbus_message_unref(method_call);
	if (!dbus_pending_call_set_notify(pending_call,
					  on_get_supported_capabilities_reply,
					  adapter, NULL)) {
		dbus_pending_call_cancel(pending_call);
		dbus_pending_call_unref(pending_call);
		return -EIO;
	}
	return 0;
}
