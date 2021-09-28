/*
 * Copyright © 2016 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *  Lyude Paul <lyude@redhat.com>
 */

#include "config.h"

#include <string.h>
#include <errno.h>
#include <math.h>
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <pthread.h>
#include <glib.h>
#include <pixman.h>
#include <cairo.h>

#include "igt_chamelium.h"
#include "igt_core.h"
#include "igt_aux.h"
#include "igt_edid.h"
#include "igt_frame.h"
#include "igt_list.h"
#include "igt_kms.h"
#include "igt_rc.h"

/**
 * SECTION:igt_chamelium
 * @short_description: Library for using the Chamelium into igt tests
 * @title: Chamelium
 * @include: igt_chamelium.h
 *
 * This library contains helpers for using Chameliums in IGT tests. This allows
 * for tests to simulate more difficult tasks to automate such as display
 * hotplugging, faulty display behaviors, etc.
 *
 * More information on the Chamelium can be found
 * [on the ChromeOS project page](https://www.chromium.org/chromium-os/testing/chamelium).
 *
 * In order to run tests using the Chamelium, a valid configuration file must be
 * present. It must contain Chamelium-specific keys as shown with the following
 * example:
 *
 * |[<!-- language="plain" -->
 *	[Chamelium]
 *	URL=http://chameleon:9992 # The URL used for connecting to the Chamelium's RPC server
 *
 *	# The rest of the sections are used for defining connector mappings.
 *	# This is required so any tests using the Chamelium know which connector
 *	# on the test machine should be connected to each Chamelium port.
 *	#
 *	# In the event that any of these mappings are specified incorrectly,
 *	# any hotplugging tests for the incorrect connector mapping will fail.
 *
 *	[Chamelium:DP-1] # The name of the DRM connector
 *	ChameliumPortID=1 # The ID of the port on the Chamelium this connector is attached to
 *
 *	[Chamelium:HDMI-A-1]
 *	ChameliumPortID=3
 * ]|
 *
 */

struct chamelium_edid {
	struct chamelium *chamelium;
	struct edid *base;
	struct edid *raw[CHAMELIUM_MAX_PORTS];
	int ids[CHAMELIUM_MAX_PORTS];
	struct igt_list link;
};

struct chamelium_port {
	unsigned int type;
	int id;
	int connector_id;
	char *name;
};

struct chamelium_frame_dump {
	unsigned char *bgr;
	size_t size;
	int width;
	int height;
	struct chamelium_port *port;
};

struct chamelium_fb_crc_async_data {
	cairo_surface_t *fb_surface;

	pthread_t thread_id;
	igt_crc_t *ret;
};

struct chamelium {
	xmlrpc_env env;
	xmlrpc_client *client;
	char *url;

	/* Indicates the last port to have been used for capturing video */
	struct chamelium_port *capturing_port;

	int drm_fd;

	struct igt_list edids;
	struct chamelium_port ports[CHAMELIUM_MAX_PORTS];
	int port_count;
};

static struct chamelium *cleanup_instance;

static void chamelium_do_calculate_fb_crc(cairo_surface_t *fb_surface,
					  igt_crc_t *out);

/**
 * chamelium_get_ports:
 * @chamelium: The Chamelium instance to use
 * @count: Where to store the number of ports
 *
 * Retrieves all of the ports currently configured for use with this chamelium
 *
 * Returns: an array containing a pointer to each configured chamelium port
 */
struct chamelium_port **chamelium_get_ports(struct chamelium *chamelium,
					    int *count)
{
	int i;
	struct chamelium_port **ret =
		calloc(sizeof(void*), chamelium->port_count);

	*count = chamelium->port_count;
	for (i = 0; i < chamelium->port_count; i++)
		ret[i] = &chamelium->ports[i];

	return ret;
}

/**
 * chamelium_port_get_type:
 * @port: The chamelium port to retrieve the type from
 *
 * Retrieves the DRM connector type of the physical port on the Chamelium. It
 * should be noted that this type may differ from the type provided by the
 * driver.
 *
 * Returns: the DRM connector type of the physical Chamelium port
 */
unsigned int chamelium_port_get_type(const struct chamelium_port *port) {
	return port->type;
}

/**
 * chamelium_port_get_connector:
 * @chamelium: The Chamelium instance to use
 * @port: The chamelium port to retrieve the DRM connector for
 * @reprobe: Whether or not to reprobe the DRM connector
 *
 * Get a drmModeConnector object for the given Chamelium port, and optionally
 * reprobe the port in the process
 *
 * Returns: a drmModeConnector object corresponding to the given port
 */
drmModeConnector *chamelium_port_get_connector(struct chamelium *chamelium,
					       struct chamelium_port *port,
					       bool reprobe)
{
	drmModeConnector *connector;

	if (reprobe)
		connector = drmModeGetConnector(chamelium->drm_fd,
						port->connector_id);
	else
		connector = drmModeGetConnectorCurrent(
		    chamelium->drm_fd, port->connector_id);

	return connector;
}

/**
 * chamelium_port_get_name:
 * @port: The chamelium port to retrieve the name of
 *
 * Gets the name of the DRM connector corresponding to the given Chamelium
 * port.
 *
 * Returns: the name of the DRM connector
 */
const char *chamelium_port_get_name(struct chamelium_port *port)
{
	return port->name;
}

/**
 * chamelium_destroy_frame_dump:
 * @dump: The frame dump to destroy
 *
 * Destroys the given frame dump and frees all of the resources associated with
 * it.
 */
void chamelium_destroy_frame_dump(struct chamelium_frame_dump *dump)
{
	free(dump->bgr);
	free(dump);
}

void chamelium_destroy_audio_file(struct chamelium_audio_file *audio_file)
{
	free(audio_file->path);
	free(audio_file);
}

struct fsm_monitor_args {
	struct chamelium *chamelium;
	struct chamelium_port *port;
	struct udev_monitor *mon;
};

/*
 * Whenever resolutions or other factors change with the display output, the
 * Chamelium's display receivers need to be fully reset in order to perform any
 * frame-capturing related tasks. This requires cutting off the display then
 * turning it back on, and is indicated by the Chamelium sending hotplug events
 */
static void *chamelium_fsm_mon(void *data)
{
	struct fsm_monitor_args *args = data;
	drmModeConnector *connector;
	int drm_fd = args->chamelium->drm_fd;

	/*
	 * Wait for the chamelium to try unplugging the connector, otherwise
	 * the thread calling chamelium_rpc will kill us
	 */
	igt_hotplug_detected(args->mon, 60);

	/*
	 * Just in case the RPC call being executed returns before we complete
	 * the FSM modesetting sequence, so we don't leave the display in a bad
	 * state.
	 */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	igt_debug("Chamelium needs FSM, handling\n");
	connector = chamelium_port_get_connector(args->chamelium, args->port,
						 false);
	kmstest_set_connector_dpms(drm_fd, connector, DRM_MODE_DPMS_OFF);
	kmstest_set_connector_dpms(drm_fd, connector, DRM_MODE_DPMS_ON);

	drmModeFreeConnector(connector);
	return NULL;
}

static xmlrpc_value *__chamelium_rpc_va(struct chamelium *chamelium,
					struct chamelium_port *fsm_port,
					const char *method_name,
					const char *format_str,
					va_list va_args)
{
	xmlrpc_value *res = NULL;
	struct fsm_monitor_args monitor_args;
	pthread_t fsm_thread_id;

	/* Cleanup the last error, if any */
	if (chamelium->env.fault_occurred) {
		xmlrpc_env_clean(&chamelium->env);
		xmlrpc_env_init(&chamelium->env);
	}

	/* Unfortunately xmlrpc_client's event loop helpers are rather useless
	 * for implementing any sort of event loop, since they provide no way
	 * to poll for events other then the RPC response. This means in order
	 * to handle the chamelium attempting FSM, we have to fork into another
	 * thread and have that handle hotplugging displays
	 */
	if (fsm_port) {
		monitor_args.chamelium = chamelium;
		monitor_args.port = fsm_port;
		monitor_args.mon = igt_watch_hotplug();
		pthread_create(&fsm_thread_id, NULL, chamelium_fsm_mon,
			       &monitor_args);
	}

	xmlrpc_client_call2f_va(&chamelium->env, chamelium->client,
				chamelium->url, method_name, format_str, &res,
				va_args);

	if (fsm_port) {
		pthread_cancel(fsm_thread_id);
		pthread_join(fsm_thread_id, NULL);
		igt_cleanup_hotplug(monitor_args.mon);
	}

	return res;
}

static xmlrpc_value *__chamelium_rpc(struct chamelium *chamelium,
				     struct chamelium_port *fsm_port,
				     const char *method_name,
				     const char *format_str,
				     ...)
{
	xmlrpc_value *res;
	va_list va_args;

	va_start(va_args, format_str);
	res = __chamelium_rpc_va(chamelium, fsm_port, method_name,
				 format_str, va_args);
	va_end(va_args);

	return res;
}

static xmlrpc_value *chamelium_rpc(struct chamelium *chamelium,
				   struct chamelium_port *fsm_port,
				   const char *method_name,
				   const char *format_str,
				   ...)
{
	xmlrpc_value *res;
	va_list va_args;

	va_start(va_args, format_str);
	res = __chamelium_rpc_va(chamelium, fsm_port, method_name,
				 format_str, va_args);
	va_end(va_args);

	igt_assert_f(!chamelium->env.fault_occurred,
		     "Chamelium RPC call failed: %s\n",
		     chamelium->env.fault_string);

	return res;
}

static bool __chamelium_is_reachable(struct chamelium *chamelium)
{
	xmlrpc_value *res;

	/* GetSupportedInputs does not require a port and is harmless */
	res = __chamelium_rpc(chamelium, NULL, "GetSupportedInputs", "()");

	if (res != NULL)
		xmlrpc_DECREF(res);

	if (chamelium->env.fault_occurred)
		igt_debug("Chamelium RPC call failed: %s\n",
			  chamelium->env.fault_string);

	return !chamelium->env.fault_occurred;
}

void chamelium_wait_reachable(struct chamelium *chamelium, int timeout)
{
	bool chamelium_online = igt_wait(__chamelium_is_reachable(chamelium),
					 timeout * 1000, 100);

	igt_assert_f(chamelium_online,
		     "Couldn't connect to Chamelium for %ds", timeout);
}

/**
 * chamelium_plug:
 * @chamelium: The Chamelium instance to use
 * @port: The port on the chamelium to plug
 *
 * Simulate a display connector being plugged into the system using the
 * chamelium.
 */
void chamelium_plug(struct chamelium *chamelium, struct chamelium_port *port)
{
	igt_debug("Plugging %s (Chamelium port ID %d)\n", port->name, port->id);
	xmlrpc_DECREF(chamelium_rpc(chamelium, NULL, "Plug", "(i)", port->id));
}

/**
 * chamelium_unplug:
 * @chamelium: The Chamelium instance to use
 * @port: The port on the chamelium to unplug
 *
 * Simulate a display connector being unplugged from the system using the
 * chamelium.
 */
void chamelium_unplug(struct chamelium *chamelium, struct chamelium_port *port)
{
	igt_debug("Unplugging port %s\n", port->name);
	xmlrpc_DECREF(chamelium_rpc(chamelium, NULL, "Unplug", "(i)",
				    port->id));
}

/**
 * chamelium_is_plugged:
 * @chamelium: The Chamelium instance to use
 * @port: The port on the Chamelium to check the status of
 *
 * Check whether or not the given port has been plugged into the system using
 * #chamelium_plug.
 *
 * Returns: %true if the connector is set to plugged in, %false otherwise.
 */
bool chamelium_is_plugged(struct chamelium *chamelium,
			  struct chamelium_port *port)
{
	xmlrpc_value *res;
	xmlrpc_bool is_plugged;

	res = chamelium_rpc(chamelium, NULL, "IsPlugged", "(i)", port->id);

	xmlrpc_read_bool(&chamelium->env, res, &is_plugged);
	xmlrpc_DECREF(res);

	return is_plugged;
}

/**
 * chamelium_port_wait_video_input_stable:
 * @chamelium: The Chamelium instance to use
 * @port: The port on the Chamelium to check the status of
 * @timeout_secs: How long to wait for a video signal to appear before timing
 * out
 *
 * Waits for a video signal to appear on the given port. This is useful for
 * checking whether or not we've setup a monitor correctly.
 *
 * Returns: %true if a video signal was detected, %false if we timed out
 */
bool chamelium_port_wait_video_input_stable(struct chamelium *chamelium,
					    struct chamelium_port *port,
					    int timeout_secs)
{
	xmlrpc_value *res;
	xmlrpc_bool is_on;

	igt_debug("Waiting for video input to stabalize on %s\n", port->name);

	res = chamelium_rpc(chamelium, port, "WaitVideoInputStable", "(ii)",
			    port->id, timeout_secs);

	xmlrpc_read_bool(&chamelium->env, res, &is_on);
	xmlrpc_DECREF(res);

	return is_on;
}

/**
 * chamelium_fire_hpd_pulses:
 * @chamelium: The Chamelium instance to use
 * @port: The port to fire the HPD pulses on
 * @width_msec: How long each pulse should last
 * @count: The number of pulses to send
 *
 * A convienence function for sending multiple hotplug pulses to the system.
 * The pulses start at low (e.g. connector is disconnected), and then alternate
 * from high (e.g. connector is plugged in) to low. This is the equivalent of
 * repeatedly calling #chamelium_plug and #chamelium_unplug, waiting
 * @width_msec between each call.
 *
 * If @count is even, the last pulse sent will be high, and if it's odd then it
 * will be low. Resetting the HPD line back to it's previous state, if desired,
 * is the responsibility of the caller.
 */
void chamelium_fire_hpd_pulses(struct chamelium *chamelium,
			       struct chamelium_port *port,
			       int width_msec, int count)
{
	xmlrpc_value *pulse_widths = xmlrpc_array_new(&chamelium->env);
	xmlrpc_value *width = xmlrpc_int_new(&chamelium->env, width_msec);
	int i;

	igt_debug("Firing %d HPD pulses with width of %d msec on %s\n",
		  count, width_msec, port->name);

	for (i = 0; i < count; i++)
		xmlrpc_array_append_item(&chamelium->env, pulse_widths, width);

	xmlrpc_DECREF(chamelium_rpc(chamelium, NULL, "FireMixedHpdPulses",
				    "(iA)", port->id, pulse_widths));

	xmlrpc_DECREF(width);
	xmlrpc_DECREF(pulse_widths);
}

/**
 * chamelium_fire_mixed_hpd_pulses:
 * @chamelium: The Chamelium instance to use
 * @port: The port to fire the HPD pulses on
 * @...: The length of each pulse in milliseconds, terminated with a %0
 *
 * Does the same thing as #chamelium_fire_hpd_pulses, but allows the caller to
 * specify the length of each individual pulse.
 */
void chamelium_fire_mixed_hpd_pulses(struct chamelium *chamelium,
				     struct chamelium_port *port, ...)
{
	va_list args;
	xmlrpc_value *pulse_widths = xmlrpc_array_new(&chamelium->env), *width;
	int arg;

	igt_debug("Firing mixed HPD pulses on %s\n", port->name);

	va_start(args, port);
	for (arg = va_arg(args, int); arg; arg = va_arg(args, int)) {
		width = xmlrpc_int_new(&chamelium->env, arg);
		xmlrpc_array_append_item(&chamelium->env, pulse_widths, width);
		xmlrpc_DECREF(width);
	}
	va_end(args);

	xmlrpc_DECREF(chamelium_rpc(chamelium, NULL, "FireMixedHpdPulses",
				    "(iA)", port->id, pulse_widths));

	xmlrpc_DECREF(pulse_widths);
}

/**
 * chamelium_schedule_hpd_toggle:
 * @chamelium: The Chamelium instance to use
 * @port: The port to fire the HPD pulses on
 * @delay_ms: Delay in milli-second before the toggle takes place
 * @rising_edge: Whether the toggle should be a rising edge or a falling edge
 *
 * Instructs the chamelium to schedule an hpd toggle (either a rising edge or
 * a falling edge, depending on @rising_edg) after @delay_ms have passed.
 * This is useful for testing things such as hpd after a suspend/resume cycle.
 */
void chamelium_schedule_hpd_toggle(struct chamelium *chamelium,
				   struct chamelium_port *port, int delay_ms,
				   bool rising_edge)
{
	igt_debug("Scheduling HPD toggle on %s in %d ms\n", port->name,
		  delay_ms);

	xmlrpc_DECREF(chamelium_rpc(chamelium, NULL, "ScheduleHpdToggle",
				    "(iii)", port->id, delay_ms, rising_edge));
}

static int chamelium_upload_edid(struct chamelium *chamelium,
				 const struct edid *edid)
{
	xmlrpc_value *res;
	int edid_id;

	res = chamelium_rpc(chamelium, NULL, "CreateEdid", "(6)",
			    edid, edid_get_size(edid));
	xmlrpc_read_int(&chamelium->env, res, &edid_id);
	xmlrpc_DECREF(res);

	return edid_id;
}

static void chamelium_destroy_edid(struct chamelium *chamelium, int edid_id)
{
	xmlrpc_DECREF(chamelium_rpc(chamelium, NULL, "DestroyEdid", "(i)",
				    edid_id));
}

/**
 * chamelium_new_edid:
 * @chamelium: The Chamelium instance to use
 * @edid: The edid blob to upload to the chamelium
 *
 * Uploads and registers a new EDID with the chamelium. The EDID will be
 * destroyed automatically when #chamelium_deinit is called.
 *
 * Callers shouldn't assume that the raw EDID they provide is uploaded as-is to
 * the Chamelium. The EDID may be mutated (e.g. a serial number can be appended
 * to be able to uniquely identify the EDID). To retrieve the exact EDID that
 * will be applied to a particular port, use #chamelium_edid_get_raw.
 *
 * Returns: An opaque pointer to the Chamelium EDID
 */
struct chamelium_edid *chamelium_new_edid(struct chamelium *chamelium,
					  const struct edid *edid)
{
	struct chamelium_edid *chamelium_edid;
	size_t edid_size = edid_get_size(edid);

	chamelium_edid = calloc(1, sizeof(struct chamelium_edid));
	chamelium_edid->chamelium = chamelium;
	chamelium_edid->base = malloc(edid_size);
	memcpy(chamelium_edid->base, edid, edid_size);
	igt_list_add(&chamelium_edid->link, &chamelium->edids);

	return chamelium_edid;
}

/**
 * chamelium_port_tag_edid: tag the EDID with the provided Chamelium port.
 */
static void chamelium_port_tag_edid(struct chamelium_port *port,
				    struct edid *edid)
{
	uint32_t *serial;

	/* Product code: Chamelium */
	edid->prod_code[0] = 'C';
	edid->prod_code[1] = 'H';

	/* Serial: Chamelium port ID */
	serial = (uint32_t *) &edid->serial;
	*serial = port->id;

	edid_update_checksum(edid);
}

/**
 * chamelium_edid_get_raw: get the raw EDID
 * @edid: the Chamelium EDID
 * @port: the Chamelium port
 *
 * The EDID provided to #chamelium_new_edid may be mutated for identification
 * purposes. This function allows to retrieve the exact EDID that will be set
 * for a given port.
 *
 * The returned raw EDID is only valid until the next call to this function.
 */
const struct edid *chamelium_edid_get_raw(struct chamelium_edid *edid,
					  struct chamelium_port *port)
{
	size_t port_index = port - edid->chamelium->ports;
	size_t edid_size;

	if (!edid->raw[port_index]) {
		edid_size = edid_get_size(edid->base);
		edid->raw[port_index] = malloc(edid_size);
		memcpy(edid->raw[port_index], edid->base, edid_size);
		chamelium_port_tag_edid(port, edid->raw[port_index]);
	}

	return edid->raw[port_index];
}

/**
 * chamelium_port_set_edid:
 * @chamelium: The Chamelium instance to use
 * @port: The port on the Chamelium to set the EDID on
 * @edid: The Chamelium EDID to set or NULL to use the default Chamelium EDID
 *
 * Sets a port on the chamelium to use the specified EDID. This does not fire a
 * hotplug pulse on it's own, and merely changes what EDID the chamelium port
 * will report to us the next time we probe it. Users will need to reprobe the
 * connectors themselves if they want to see the EDID reported by the port
 * change.
 *
 * To create an EDID, see #chamelium_new_edid.
 */
void chamelium_port_set_edid(struct chamelium *chamelium,
			     struct chamelium_port *port,
			     struct chamelium_edid *edid)
{
	int edid_id;
	size_t port_index;
	const struct edid *raw_edid;

	if (edid) {
		port_index = port - chamelium->ports;
		edid_id = edid->ids[port_index];
		if (edid_id == 0) {
			raw_edid = chamelium_edid_get_raw(edid, port);
			edid_id = chamelium_upload_edid(chamelium, raw_edid);
			edid->ids[port_index] = edid_id;
		}
	} else {
		edid_id = 0;
	}

	xmlrpc_DECREF(chamelium_rpc(chamelium, NULL, "ApplyEdid", "(ii)",
				    port->id, edid_id));
}

/**
 * chamelium_port_set_ddc_state:
 * @chamelium: The Chamelium instance to use
 * @port: The port to change the DDC state on
 * @enabled: Whether or not to enable the DDC bus
 *
 * This disables the DDC bus (e.g. the i2c line on the connector that gives us
 * an EDID) of the specified port on the chamelium. This is useful for testing
 * behavior on legacy connectors such as VGA, where the presence of a DDC bus
 * is not always guaranteed.
 */
void chamelium_port_set_ddc_state(struct chamelium *chamelium,
				  struct chamelium_port *port,
				  bool enabled)
{
	igt_debug("%sabling DDC bus on %s\n",
		  enabled ? "En" : "Dis", port->name);

	xmlrpc_DECREF(chamelium_rpc(chamelium, NULL, "SetDdcState", "(ib)",
				    port->id, enabled));
}

/**
 * chamelium_port_get_ddc_state:
 * @chamelium: The Chamelium instance to use
 * @port: The port on the Chamelium to check the status of
 *
 * Check whether or not the DDC bus on the specified chamelium port is enabled
 * or not.
 *
 * Returns: %true if the DDC bus is enabled, %false otherwise.
 */
bool chamelium_port_get_ddc_state(struct chamelium *chamelium,
				  struct chamelium_port *port)
{
	xmlrpc_value *res;
	xmlrpc_bool enabled;

	res = chamelium_rpc(chamelium, NULL, "IsDdcEnabled", "(i)", port->id);
	xmlrpc_read_bool(&chamelium->env, res, &enabled);

	xmlrpc_DECREF(res);
	return enabled;
}

/**
 * chamelium_port_get_resolution:
 * @chamelium: The Chamelium instance to use
 * @port: The port on the Chamelium to check
 * @x: Where to store the horizontal resolution of the port
 * @y: Where to store the verical resolution of the port
 *
 * Check the current reported display resolution of the specified port on the
 * chamelium. This information is provided by the chamelium itself, not DRM.
 * Useful for verifying that we really are scanning out at the resolution we
 * think we are.
 */
void chamelium_port_get_resolution(struct chamelium *chamelium,
				   struct chamelium_port *port,
				   int *x, int *y)
{
	xmlrpc_value *res, *res_x, *res_y;

	res = chamelium_rpc(chamelium, port, "DetectResolution", "(i)",
			    port->id);

	xmlrpc_array_read_item(&chamelium->env, res, 0, &res_x);
	xmlrpc_array_read_item(&chamelium->env, res, 1, &res_y);
	xmlrpc_read_int(&chamelium->env, res_x, x);
	xmlrpc_read_int(&chamelium->env, res_y, y);

	xmlrpc_DECREF(res_x);
	xmlrpc_DECREF(res_y);
	xmlrpc_DECREF(res);
}

/** chamelium_supports_method: checks if the Chamelium board supports a method.
 *
 * Note: this actually tries to call the method.
 *
 * See https://crbug.com/977995 for a discussion about a better solution.
 */
static bool chamelium_supports_method(struct chamelium *chamelium,
				      const char *name)
{
	xmlrpc_value *res;

	res = __chamelium_rpc(chamelium, NULL, name, "()");
	if (res)
		xmlrpc_DECREF(res);

	/* XML-RPC has a special code for unsupported methods
	 * (XMLRPC_NO_SUCH_METHOD_ERROR) however the Chamelium implementation
	 * doesn't return it. */
	return (!chamelium->env.fault_occurred ||
		strstr(chamelium->env.fault_string, "not supported") == NULL);
}

bool chamelium_supports_get_video_params(struct chamelium *chamelium)
{
	return chamelium_supports_method(chamelium, "GetVideoParams");
}

static void read_int_from_xml_struct(struct chamelium *chamelium,
				     xmlrpc_value *struct_val, const char *key,
				     int *dst)
{
	xmlrpc_value *val = NULL;

	xmlrpc_struct_find_value(&chamelium->env, struct_val, key, &val);
	if (val) {
		xmlrpc_read_int(&chamelium->env, val, dst);
		xmlrpc_DECREF(val);
	} else
		*dst = -1;
}

static void video_params_from_xml(struct chamelium *chamelium,
				  xmlrpc_value *res,
				  struct chamelium_video_params *params)
{
	xmlrpc_value *val = NULL;

	xmlrpc_struct_find_value(&chamelium->env, res, "clock", &val);
	if (val) {
		xmlrpc_read_double(&chamelium->env, val, &params->clock);
		xmlrpc_DECREF(val);
	} else
		params->clock = NAN;

	read_int_from_xml_struct(chamelium, res, "htotal", &params->htotal);
	read_int_from_xml_struct(chamelium, res, "hactive", &params->hactive);
	read_int_from_xml_struct(chamelium, res, "hsync_offset",
				 &params->hsync_offset);
	read_int_from_xml_struct(chamelium, res, "hsync_width",
				 &params->hsync_width);
	read_int_from_xml_struct(chamelium, res, "hsync_polarity",
				 &params->hsync_polarity);
	read_int_from_xml_struct(chamelium, res, "vtotal", &params->vtotal);
	read_int_from_xml_struct(chamelium, res, "vactive", &params->vactive);
	read_int_from_xml_struct(chamelium, res, "vsync_offset",
				 &params->vsync_offset);
	read_int_from_xml_struct(chamelium, res, "vsync_width",
				 &params->vsync_width);
	read_int_from_xml_struct(chamelium, res, "vsync_polarity",
				 &params->vsync_polarity);
}

void chamelium_port_get_video_params(struct chamelium *chamelium,
				     struct chamelium_port *port,
				     struct chamelium_video_params *params)
{
	xmlrpc_value *res;

	res = chamelium_rpc(chamelium, NULL, "GetVideoParams", "(i)", port->id);
	video_params_from_xml(chamelium, res, params);

	xmlrpc_DECREF(res);
}

static void chamelium_get_captured_resolution(struct chamelium *chamelium,
					      int *w, int *h)
{
	xmlrpc_value *res, *res_w, *res_h;

	res = chamelium_rpc(chamelium, NULL, "GetCapturedResolution", "()");

	xmlrpc_array_read_item(&chamelium->env, res, 0, &res_w);
	xmlrpc_array_read_item(&chamelium->env, res, 1, &res_h);
	xmlrpc_read_int(&chamelium->env, res_w, w);
	xmlrpc_read_int(&chamelium->env, res_h, h);

	xmlrpc_DECREF(res_w);
	xmlrpc_DECREF(res_h);
	xmlrpc_DECREF(res);
}

static struct chamelium_frame_dump *frame_from_xml(struct chamelium *chamelium,
						   xmlrpc_value *frame_xml)
{
	struct chamelium_frame_dump *ret = malloc(sizeof(*ret));

	chamelium_get_captured_resolution(chamelium, &ret->width, &ret->height);
	ret->port = chamelium->capturing_port;
	xmlrpc_read_base64(&chamelium->env, frame_xml, &ret->size,
			   (void*)&ret->bgr);

	return ret;
}

/**
 * chamelium_port_dump_pixels:
 * @chamelium: The Chamelium instance to use
 * @port: The port to perform the video capture on
 * @x: The X coordinate to crop the screen capture to
 * @y: The Y coordinate to crop the screen capture to
 * @w: The width of the area to crop the screen capture to, or 0 for the whole
 * screen
 * @h: The height of the area to crop the screen capture to, or 0 for the whole
 * screen
 *
 * Captures the currently displayed image on the given chamelium port,
 * optionally cropped to a given region. In situations where pre-calculating
 * CRCs may not be reliable, this can be used as an alternative for figuring
 * out whether or not the correct images are being displayed on the screen.
 *
 * The frame dump data returned by this function should be freed when the
 * caller is done with it using #chamelium_destroy_frame_dump.
 *
 * As an important note: some of the EDIDs provided by the Chamelium cause
 * certain GPU drivers to default to using limited color ranges. This can cause
 * video captures from the Chamelium to provide different images then expected
 * due to the difference in color ranges (framebuffer uses full color range,
 * but the video output doesn't), and as a result lead to CRC mismatches. To
 * workaround this, the caller should force the connector to use full color
 * ranges by using #kmstest_set_connector_broadcast_rgb before setting up the
 * display.
 *
 * Returns: a chamelium_frame_dump struct
 */
struct chamelium_frame_dump *chamelium_port_dump_pixels(struct chamelium *chamelium,
							struct chamelium_port *port,
							int x, int y,
							int w, int h)
{
	xmlrpc_value *res;
	struct chamelium_frame_dump *frame;

	res = chamelium_rpc(chamelium, port, "DumpPixels",
			    (w && h) ? "(iiiii)" : "(innnn)",
			    port->id, x, y, w, h);
	chamelium->capturing_port = port;

	frame = frame_from_xml(chamelium, res);
	xmlrpc_DECREF(res);

	return frame;
}

static void crc_from_xml(struct chamelium *chamelium,
			 xmlrpc_value *xml_crc, igt_crc_t *out)
{
	xmlrpc_value *res;
	int i;

	out->n_words = xmlrpc_array_size(&chamelium->env, xml_crc);
	for (i = 0; i < out->n_words; i++) {
		xmlrpc_array_read_item(&chamelium->env, xml_crc, i, &res);
		xmlrpc_read_int(&chamelium->env, res, (int*)&out->crc[i]);
		xmlrpc_DECREF(res);
	}
}

/**
 * chamelium_get_crc_for_area:
 * @chamelium: The Chamelium instance to use
 * @port: The port to perform the CRC checking on
 * @x: The X coordinate on the emulated display to start calculating the CRC
 * from
 * @y: The Y coordinate on the emulated display to start calculating the CRC
 * from
 * @w: The width of the area to fetch the CRC from, or %0 for the whole display
 * @h: The height of the area to fetch the CRC from, or %0 for the whole display
 *
 * Reads back the pixel CRC for an area on the specified chamelium port. This
 * is the same as using the CRC readback from a GPU, the main difference being
 * the data is provided by the chamelium and also allows us to specify a region
 * of the screen to use as opposed to the entire thing.
 *
 * As an important note: some of the EDIDs provided by the Chamelium cause
 * certain GPU drivers to default to using limited color ranges. This can cause
 * video captures from the Chamelium to provide different images then expected
 * due to the difference in color ranges (framebuffer uses full color range,
 * but the video output doesn't), and as a result lead to CRC mismatches. To
 * workaround this, the caller should force the connector to use full color
 * ranges by using #kmstest_set_connector_broadcast_rgb before setting up the
 * display.
 *
 * After the caller is finished with the EDID returned by this function, the
 * caller should manually free the resources associated with it.
 *
 * Returns: The CRC read back from the chamelium
 */
igt_crc_t *chamelium_get_crc_for_area(struct chamelium *chamelium,
				      struct chamelium_port *port,
				      int x, int y, int w, int h)
{
	xmlrpc_value *res;
	igt_crc_t *ret = malloc(sizeof(igt_crc_t));

	res = chamelium_rpc(chamelium, port, "ComputePixelChecksum",
			    (w && h) ? "(iiiii)" : "(innnn)",
			    port->id, x, y, w, h);
	chamelium->capturing_port = port;

	crc_from_xml(chamelium, res, ret);
	xmlrpc_DECREF(res);

	return ret;
}

/**
 * chamelium_start_capture:
 * @chamelium: The Chamelium instance to use
 * @port: The port to perform the video capture on
 * @x: The X coordinate to crop the video to
 * @y: The Y coordinate to crop the video to
 * @w: The width of the cropped video, or %0 for the whole display
 * @h: The height of the cropped video, or %0 for the whole display
 *
 * Starts capturing video frames on the given Chamelium port. Once the user is
 * finished capturing frames, they should call #chamelium_stop_capture.
 *
 * A blocking, one-shot version of this function is available: see
 * #chamelium_capture
 *
 * As an important note: some of the EDIDs provided by the Chamelium cause
 * certain GPU drivers to default to using limited color ranges. This can cause
 * video captures from the Chamelium to provide different images then expected
 * due to the difference in color ranges (framebuffer uses full color range,
 * but the video output doesn't), and as a result lead to CRC and frame dump
 * comparison mismatches. To workaround this, the caller should force the
 * connector to use full color ranges by using
 * #kmstest_set_connector_broadcast_rgb before setting up the display.
 */
void chamelium_start_capture(struct chamelium *chamelium,
			     struct chamelium_port *port, int x, int y, int w, int h)
{
	xmlrpc_DECREF(chamelium_rpc(chamelium, port, "StartCapturingVideo",
				    (w && h) ? "(iiiii)" : "(innnn)",
				    port->id, x, y, w, h));
	chamelium->capturing_port = port;
}

/**
 * chamelium_stop_capture:
 * @chamelium: The Chamelium instance to use
 * @frame_count: The number of frames to wait to capture, or %0 to stop
 * immediately
 *
 * Finishes capturing video frames on the given Chamelium port. If @frame_count
 * is specified, this call will block until the given number of frames have been
 * captured.
 */
void chamelium_stop_capture(struct chamelium *chamelium, int frame_count)
{
	xmlrpc_DECREF(chamelium_rpc(chamelium, NULL, "StopCapturingVideo",
				    "(i)", frame_count));
}

/**
 * chamelium_capture:
 * @chamelium: The Chamelium instance to use
 * @port: The port to perform the video capture on
 * @x: The X coordinate to crop the video to
 * @y: The Y coordinate to crop the video to
 * @w: The width of the cropped video, or %0 for the whole display
 * @h: The height of the cropped video, or %0 for the whole display
 * @frame_count: The number of frames to capture
 *
 * Captures the given number of frames on the chamelium. This is equivalent to
 * calling #chamelium_start_capture immediately followed by
 * #chamelium_stop_capture. The caller is blocked until all of the frames have
 * been captured.
 *
 * As an important note: some of the EDIDs provided by the Chamelium cause
 * certain GPU drivers to default to using limited color ranges. This can cause
 * video captures from the Chamelium to provide different images then expected
 * due to the difference in color ranges (framebuffer uses full color range,
 * but the video output doesn't), and as a result lead to CRC and frame dump
 * comparison mismatches. To workaround this, the caller should force the
 * connector to use full color ranges by using
 * #kmstest_set_connector_broadcast_rgb before setting up the display.
 */
void chamelium_capture(struct chamelium *chamelium, struct chamelium_port *port,
		       int x, int y, int w, int h, int frame_count)
{
	xmlrpc_DECREF(chamelium_rpc(chamelium, port, "CaptureVideo",
				    (w && h) ? "(iiiiii)" : "(iinnnn)",
				    port->id, frame_count, x, y, w, h));
	chamelium->capturing_port = port;
}

/**
 * chamelium_read_captured_crcs:
 * @chamelium: The Chamelium instance to use
 * @frame_count: Where to store the number of CRCs we read in
 *
 * Reads all of the CRCs that have been captured thus far from the Chamelium.
 *
 * Returns: An array of @frame_count length containing all of the CRCs we read
 */
igt_crc_t *chamelium_read_captured_crcs(struct chamelium *chamelium,
					int *frame_count)
{
	igt_crc_t *ret;
	xmlrpc_value *res, *elem;
	int i;

	res = chamelium_rpc(chamelium, NULL, "GetCapturedChecksums", "(in)", 0);

	*frame_count = xmlrpc_array_size(&chamelium->env, res);
	ret = calloc(sizeof(igt_crc_t), *frame_count);

	for (i = 0; i < *frame_count; i++) {
		xmlrpc_array_read_item(&chamelium->env, res, i, &elem);

		crc_from_xml(chamelium, elem, &ret[i]);
		ret[i].frame = i;

		xmlrpc_DECREF(elem);
	}

	xmlrpc_DECREF(res);

	return ret;
}

/**
 * chamelium_port_read_captured_frame:
 *
 * @chamelium: The Chamelium instance to use
 * @index: The index of the captured frame we want to get
 *
 * Retrieves a single video frame captured during the last video capture on the
 * Chamelium. This data should be freed using #chamelium_destroy_frame_data
 *
 * Returns: a chamelium_frame_dump struct
 */
struct chamelium_frame_dump *chamelium_read_captured_frame(struct chamelium *chamelium,
							   unsigned int index)
{
	xmlrpc_value *res;
	struct chamelium_frame_dump *frame;

	res = chamelium_rpc(chamelium, NULL, "ReadCapturedFrame", "(i)", index);
	frame = frame_from_xml(chamelium, res);
	xmlrpc_DECREF(res);

	return frame;
}

/**
 * chamelium_get_captured_frame_count:
 * @chamelium: The Chamelium instance to use
 *
 * Gets the number of frames that were captured during the last video capture.
 *
 * Returns: the number of frames the Chamelium captured during the last video
 * capture.
 */
int chamelium_get_captured_frame_count(struct chamelium *chamelium)
{
	xmlrpc_value *res;
	int ret;

	res = chamelium_rpc(chamelium, NULL, "GetCapturedFrameCount", "()");
	xmlrpc_read_int(&chamelium->env, res, &ret);

	xmlrpc_DECREF(res);
	return ret;
}

bool chamelium_supports_get_last_infoframe(struct chamelium *chamelium)
{
	return chamelium_supports_method(chamelium, "GetLastInfoFrame");
}

static const char *
chamelium_infoframe_type_str(enum chamelium_infoframe_type type)
{
	switch (type) {
	case CHAMELIUM_INFOFRAME_AVI:
		return "avi";
	case CHAMELIUM_INFOFRAME_AUDIO:
		return "audio";
	case CHAMELIUM_INFOFRAME_MPEG:
		return "mpeg";
	case CHAMELIUM_INFOFRAME_VENDOR:
		return "vendor";
	}
	assert(0); /* unreachable */
}

struct chamelium_infoframe *
chamelium_get_last_infoframe(struct chamelium *chamelium,
			     struct chamelium_port *port,
			     enum chamelium_infoframe_type type)
{
	xmlrpc_value *res, *res_version, *res_payload;
	struct chamelium_infoframe *infoframe;
	const unsigned char *payload;

	res = chamelium_rpc(chamelium, NULL, "GetLastInfoFrame", "(is)",
			    port->id, chamelium_infoframe_type_str(type));
	xmlrpc_struct_find_value(&chamelium->env, res, "version", &res_version);
	xmlrpc_struct_find_value(&chamelium->env, res, "payload", &res_payload);
	infoframe = calloc(1, sizeof(*infoframe));
	xmlrpc_read_int(&chamelium->env, res_version, &infoframe->version);
	xmlrpc_read_base64(&chamelium->env, res_payload,
			   &infoframe->payload_size, &payload);
	/* xmlrpc-c's docs say payload is actually not constant */
	infoframe->payload = (uint8_t *) payload;
	xmlrpc_DECREF(res_version);
	xmlrpc_DECREF(res_payload);
	xmlrpc_DECREF(res);

	if (infoframe->payload_size == 0) {
		chamelium_infoframe_destroy(infoframe);
		return NULL;
	}
	return infoframe;
}

void chamelium_infoframe_destroy(struct chamelium_infoframe *infoframe)
{
	free(infoframe->payload);
	free(infoframe);
}

bool chamelium_supports_trigger_link_failure(struct chamelium *chamelium)
{
	return chamelium_supports_method(chamelium, "TriggerLinkFailure");
}

/**
 * chamelium_trigger_link_failure: trigger a link failure on the provided port.
 */
void chamelium_trigger_link_failure(struct chamelium *chamelium,
				    struct chamelium_port *port)
{
	xmlrpc_DECREF(chamelium_rpc(chamelium, port, "TriggerLinkFailure",
				    "(i)", port->id));
}

bool chamelium_has_audio_support(struct chamelium *chamelium,
				 struct chamelium_port *port)
{
	xmlrpc_value *res;
	xmlrpc_bool has_support;

	if (!chamelium_supports_method(chamelium, "GetAudioFormat")) {
		igt_debug("The Chamelium device doesn't support GetAudioFormat\n");
		return false;
	}

	res = chamelium_rpc(chamelium, port, "HasAudioSupport", "(i)", port->id);
	xmlrpc_read_bool(&chamelium->env, res, &has_support);
	xmlrpc_DECREF(res);

	return has_support;
}

/**
 * chamelium_get_audio_channel_mapping:
 * @chamelium: the Chamelium instance
 * @port: the audio port
 * @mapping: will be filled with the channel mapping
 *
 * Obtains the channel mapping for an audio port.
 *
 * Audio channels are not guaranteed not to be swapped. Users can use the
 * channel mapping to match an input channel to a capture channel.
 *
 * The mapping contains one element per capture channel. Each element indicates
 * which input channel the capture channel is mapped to. As a special case, -1
 * means that the channel isn't mapped.
 */
void chamelium_get_audio_channel_mapping(struct chamelium *chamelium,
					 struct chamelium_port *port,
					 int mapping[static CHAMELIUM_MAX_AUDIO_CHANNELS])
{
	xmlrpc_value *res, *res_channel;
	int res_len, i;

	res = chamelium_rpc(chamelium, port, "GetAudioChannelMapping", "(i)",
			    port->id);
	res_len = xmlrpc_array_size(&chamelium->env, res);
	igt_assert(res_len == CHAMELIUM_MAX_AUDIO_CHANNELS);
	for (i = 0; i < res_len; i++) {
		xmlrpc_array_read_item(&chamelium->env, res, i, &res_channel);
		xmlrpc_read_int(&chamelium->env, res_channel, &mapping[i]);
		xmlrpc_DECREF(res_channel);
	}
	xmlrpc_DECREF(res);
}

static void audio_format_from_xml(struct chamelium *chamelium,
				  xmlrpc_value *res, int *rate, int *channels)
{
	xmlrpc_value *res_type, *res_rate, *res_sample_format, *res_channel;
	char *type, *sample_format;

	xmlrpc_struct_find_value(&chamelium->env, res, "file_type", &res_type);
	xmlrpc_struct_find_value(&chamelium->env, res, "rate", &res_rate);
	xmlrpc_struct_find_value(&chamelium->env, res, "sample_format", &res_sample_format);
	xmlrpc_struct_find_value(&chamelium->env, res, "channel", &res_channel);

	xmlrpc_read_string(&chamelium->env, res_type, (const char **) &type);
	igt_assert(strcmp(type, "raw") == 0);
	free(type);

	xmlrpc_read_string(&chamelium->env, res_sample_format, (const char **) &sample_format);
	igt_assert(strcmp(sample_format, "S32_LE") == 0);
	free(sample_format);

	if (rate)
		xmlrpc_read_int(&chamelium->env, res_rate, rate);
	if (channels) {
		xmlrpc_read_int(&chamelium->env, res_channel, channels);
		igt_assert(*channels <= CHAMELIUM_MAX_AUDIO_CHANNELS);
	}

	xmlrpc_DECREF(res_channel);
	xmlrpc_DECREF(res_sample_format);
	xmlrpc_DECREF(res_rate);
	xmlrpc_DECREF(res_type);
}

/**
 * chamelium_get_audio_format:
 * @chamelium: the Chamelium instance
 * @port: the audio port
 * @rate: if non-NULL, will be set to the sample rate in Hz
 * @channels: if non-NULL, will be set to the number of channels
 *
 * Obtains the audio format of the captured data. Users should start sending an
 * audio signal to the Chamelium device prior to calling this function.
 *
 * The captured data is guaranteed to be in the S32_LE format.
 */
void chamelium_get_audio_format(struct chamelium *chamelium,
				struct chamelium_port *port,
				int *rate, int *channels)
{
	xmlrpc_value *res;

	res = chamelium_rpc(chamelium, port, "GetAudioFormat", "(i)",
			    port->id);
	audio_format_from_xml(chamelium, res, rate, channels);
	xmlrpc_DECREF(res);
}

/**
 * chamelium_start_capturing_audio:
 * @chamelium: the Chamelium instance
 * @port: the port to capture audio from (it must support audio)
 * @save_to_file: whether the captured audio data should be saved to a file on
 * the Chamelium device
 *
 * Starts capturing audio from a Chamelium port. To stop the capture, use
 * #chamelium_stop_capturing_audio. To retrieve the audio data, either use the
 * stream server or enable @save_to_file (the latter is mainly useful for
 * debugging purposes).
 *
 * It isn't possible to capture audio from multiple ports at the same time.
 */
void chamelium_start_capturing_audio(struct chamelium *chamelium,
				    struct chamelium_port *port,
				    bool save_to_file)
{
	xmlrpc_value *res;

	res = chamelium_rpc(chamelium, port, "StartCapturingAudio", "(ib)",
			    port->id, save_to_file);
	xmlrpc_DECREF(res);
}

/**
 * chamelium_stop_capturing_audio:
 * @chamelium: the Chamelium instance
 * @port: the port from which audio is being captured
 *
 * Stops capturing audio from a Chamelium port. If
 * #chamelium_start_capturing_audio has been called with @save_to_file enabled,
 * this function will return a #chamelium_audio_file struct containing details
 * about the audio file. Once the caller is done with the struct, they should
 * release it with #chamelium_destroy_audio_file.
 */
struct chamelium_audio_file *chamelium_stop_capturing_audio(struct chamelium *chamelium,
							    struct chamelium_port *port)
{
	xmlrpc_value *res, *res_path, *res_props;
	struct chamelium_audio_file *file = NULL;
	char *path;

	res = chamelium_rpc(chamelium, NULL, "StopCapturingAudio", "(i)",
			    port->id);
	xmlrpc_array_read_item(&chamelium->env, res, 0, &res_path);
	xmlrpc_array_read_item(&chamelium->env, res, 1, &res_props);

	xmlrpc_read_string(&chamelium->env, res_path, (const char **) &path);

	if (strlen(path) > 0) {
		file = calloc(1, sizeof(*file));
		file->path = path;

		audio_format_from_xml(chamelium, res_props,
				      &file->rate, &file->channels);
	} else {
		free(path);
	}

	xmlrpc_DECREF(res_props);
	xmlrpc_DECREF(res_path);
	xmlrpc_DECREF(res);

	return file;
}

static pixman_image_t *convert_frame_format(pixman_image_t *src,
					    int format)
{
	pixman_image_t *converted;
	int w = pixman_image_get_width(src), h = pixman_image_get_height(src);

	converted = pixman_image_create_bits(format, w, h, NULL,
					     PIXMAN_FORMAT_BPP(format) / 8 * w);
	pixman_image_composite(PIXMAN_OP_ADD, src, NULL, converted,
			       0, 0, 0, 0, 0, 0, w, h);

	return converted;
}

static cairo_surface_t *convert_frame_dump_argb32(const struct chamelium_frame_dump *dump)
{
	cairo_surface_t *dump_surface;
	pixman_image_t *image_bgr;
	pixman_image_t *image_argb;
	int w = dump->width, h = dump->height;
	uint32_t *bits_bgr = (uint32_t *) dump->bgr;
	unsigned char *bits_argb;
	unsigned char *bits_target;
	int size;

	image_bgr = pixman_image_create_bits(
	    PIXMAN_b8g8r8, w, h, bits_bgr,
	    PIXMAN_FORMAT_BPP(PIXMAN_b8g8r8) / 8 * w);
	image_argb = convert_frame_format(image_bgr, PIXMAN_x8r8g8b8);
	pixman_image_unref(image_bgr);

	bits_argb = (unsigned char *) pixman_image_get_data(image_argb);

	dump_surface = cairo_image_surface_create(
	    CAIRO_FORMAT_ARGB32, w, h);

	bits_target = cairo_image_surface_get_data(dump_surface);
	size = cairo_image_surface_get_stride(dump_surface) * h;
	memcpy(bits_target, bits_argb, size);
	cairo_surface_mark_dirty(dump_surface);

	pixman_image_unref(image_argb);

	return dump_surface;
}

static void compared_frames_dump(cairo_surface_t *reference,
				 cairo_surface_t *capture,
				 igt_crc_t *reference_crc,
				 igt_crc_t *capture_crc)
{
	char *reference_suffix;
	char *capture_suffix;
	igt_crc_t local_reference_crc;
	igt_crc_t local_capture_crc;

	igt_assert(reference && capture);

	if (!reference_crc) {
		chamelium_do_calculate_fb_crc(reference, &local_reference_crc);
		reference_crc = &local_reference_crc;
	}

	if (!capture_crc) {
		chamelium_do_calculate_fb_crc(reference, &local_capture_crc);
		capture_crc = &local_capture_crc;
	}

	reference_suffix = igt_crc_to_string_extended(reference_crc, '-', 2);
	capture_suffix = igt_crc_to_string_extended(capture_crc, '-', 2);

	/* Write reference and capture frames to png. */
	igt_write_compared_frames_to_png(reference, capture, reference_suffix,
					 capture_suffix);

	free(reference_suffix);
	free(capture_suffix);
}

/**
 * chamelium_assert_frame_eq:
 * @chamelium: The chamelium instance the frame dump belongs to
 * @dump: The chamelium frame dump to check
 * @fb: The framebuffer to check against
 *
 * Asserts that the image contained in the chamelium frame dump is identical to
 * the given framebuffer. Useful for scenarios where pre-calculating CRCs might
 * not be ideal.
 */
void chamelium_assert_frame_eq(const struct chamelium *chamelium,
			       const struct chamelium_frame_dump *dump,
			       struct igt_fb *fb)
{
	cairo_surface_t *fb_surface;
	pixman_image_t *reference_src, *reference_bgr;
	int w = dump->width, h = dump->height;
	bool eq;

	/* Get the cairo surface for the framebuffer */
	fb_surface = igt_get_cairo_surface(chamelium->drm_fd, fb);

	/*
	 * Convert the reference image into the same format as the chamelium
	 * image
	 */
	reference_src = pixman_image_create_bits(
	    PIXMAN_x8r8g8b8, w, h,
	    (void*)cairo_image_surface_get_data(fb_surface),
	    cairo_image_surface_get_stride(fb_surface));
	reference_bgr = convert_frame_format(reference_src, PIXMAN_b8g8r8);
	pixman_image_unref(reference_src);

	/* Now do the actual comparison */
	eq = memcmp(dump->bgr, pixman_image_get_data(reference_bgr),
		    dump->size) == 0;

	pixman_image_unref(reference_bgr);

	igt_fail_on_f(!eq,
		      "Chamelium frame dump didn't match reference image\n");
}

/**
 * chamelium_assert_crc_eq_or_dump:
 * @chamelium: The chamelium instance the frame dump belongs to
 * @reference_crc: The CRC for the reference frame
 * @capture_crc: The CRC for the captured frame
 * @fb: pointer to an #igt_fb structure
 *
 * Asserts that the CRC provided for both the reference and the captured frame
 * are identical. If they are not, this grabs the captured frame and saves it
 * along with the reference to a png file.
 */
void chamelium_assert_crc_eq_or_dump(struct chamelium *chamelium,
				     igt_crc_t *reference_crc,
				     igt_crc_t *capture_crc, struct igt_fb *fb,
				     int index)
{
	struct chamelium_frame_dump *frame;
	cairo_surface_t *reference;
	cairo_surface_t *capture;
	bool eq;

	igt_debug("Reference CRC: %s\n", igt_crc_to_string(reference_crc));
	igt_debug("Captured CRC: %s\n", igt_crc_to_string(capture_crc));

	eq = igt_check_crc_equal(reference_crc, capture_crc);
	if (!eq && igt_frame_dump_is_enabled()) {
		/* Convert the reference framebuffer to cairo. */
		reference = igt_get_cairo_surface(chamelium->drm_fd, fb);

		/* Grab the captured frame from the Chamelium. */
		frame = chamelium_read_captured_frame(chamelium, index);
		igt_assert(frame);

		/* Convert the captured frame to cairo. */
		capture = convert_frame_dump_argb32(frame);
		igt_assert(capture);

		compared_frames_dump(reference, capture, reference_crc,
				     capture_crc);

		cairo_surface_destroy(reference);
		cairo_surface_destroy(capture);
		chamelium_destroy_frame_dump(frame);
	}

	igt_assert(eq);
}

/**
 * chamelium_assert_frame_match_or_dump:
 * @chamelium: The chamelium instance the frame dump belongs to
 * @frame: The chamelium frame dump to match
 * @fb: pointer to an #igt_fb structure
 * @check: the type of frame matching check to use
 *
 * Asserts that the provided captured frame matches the reference frame from
 * the framebuffer. If they do not, this saves the reference and captured frames
 * to a png file.
 */
void chamelium_assert_frame_match_or_dump(struct chamelium *chamelium,
					  struct chamelium_port *port,
					  const struct chamelium_frame_dump *frame,
					  struct igt_fb *fb,
					  enum chamelium_check check)
{
	cairo_surface_t *reference;
	cairo_surface_t *capture;
	igt_crc_t *reference_crc;
	igt_crc_t *capture_crc;
	bool match;

	/* Grab the reference frame from framebuffer */
	reference = igt_get_cairo_surface(chamelium->drm_fd, fb);

	/* Grab the captured frame from chamelium */
	capture = convert_frame_dump_argb32(frame);

	switch (check) {
	case CHAMELIUM_CHECK_ANALOG:
		match = igt_check_analog_frame_match(reference, capture);
		break;
	case CHAMELIUM_CHECK_CHECKERBOARD:
		match = igt_check_checkerboard_frame_match(reference, capture);
		break;
	default:
		igt_assert(false);
	}

	if (!match && igt_frame_dump_is_enabled()) {
		reference_crc = malloc(sizeof(igt_crc_t));
		igt_assert(reference_crc);

		/* Calculate the reference frame CRC. */
		chamelium_do_calculate_fb_crc(reference, reference_crc);

		/* Get the captured frame CRC from the Chamelium. */
		capture_crc = chamelium_get_crc_for_area(chamelium, port, 0, 0,
							 0, 0);
		igt_assert(capture_crc);

		compared_frames_dump(reference, capture, reference_crc,
				     capture_crc);

		free(reference_crc);
		free(capture_crc);
	}

	igt_assert(match);

	cairo_surface_destroy(reference);
	cairo_surface_destroy(capture);
}

/**
 * chamelium_analog_frame_crop:
 * @chamelium: The Chamelium instance to use
 * @dump: The chamelium frame dump to crop
 * @width: The cropped frame width
 * @height: The cropped frame height
 *
 * Detects the corners of a chamelium frame and crops it to the requested
 * width/height. This is useful for VGA frame dumps that also contain the
 * pixels dumped during the blanking intervals.
 *
 * The detection is done on a brightness-threshold-basis, that is adapted
 * to the reference frame used by i-g-t. It may not be as relevant for other
 * frames.
 */
void chamelium_crop_analog_frame(struct chamelium_frame_dump *dump, int width,
				 int height)
{
	unsigned char *bgr;
	unsigned char *p;
	unsigned char *q;
	int top, left;
	int x, y, xx, yy;
	int score;

	if (dump->width == width && dump->height == height)
		return;

	/* Start with the most bottom-right position. */
	top = dump->height - height;
	left = dump->width - width;

	igt_assert(top >= 0 && left >= 0);

	igt_debug("Cropping analog frame from %dx%d to %dx%d\n", dump->width,
		  dump->height, width, height);

	/* Detect the top-left corner of the frame. */
	for (x = 0; x < dump->width; x++) {
		for (y = 0; y < dump->height; y++) {
			p = &dump->bgr[(x + y * dump->width) * 3];

			/* Detect significantly bright pixels. */
			if (p[0] < 50 && p[1] < 50 && p[2] < 50)
				continue;

			/*
			 * Make sure close-by pixels are also significantly
			 * bright.
			 */
			score = 0;
			for (xx = x; xx < x + 10; xx++) {
				for (yy = y; yy < y + 10; yy++) {
					p = &dump->bgr[(xx + yy * dump->width) * 3];

					if (p[0] > 50 && p[1] > 50 && p[2] > 50)
						score++;
				}
			}

			/* Not enough pixels are significantly bright. */
			if (score < 25)
				continue;

			if (x < left)
				left = x;

			if (y < top)
				top = y;

			if (left == x || top == y)
				continue;
		}
	}

	igt_debug("Detected analog frame edges at %dx%d\n", left, top);

	/* Crop the frame given the detected top-left corner. */
	bgr = malloc(width * height * 3);

	for (y = 0; y < height; y++) {
		p = &dump->bgr[(left + (top + y) * dump->width) * 3];
		q = &bgr[(y * width) * 3];
		memcpy(q, p, width * 3);
	}

	free(dump->bgr);
	dump->width = width;
	dump->height = height;
	dump->bgr = bgr;
}

/**
 * chamelium_get_frame_limit:
 * @chamelium: The Chamelium instance to use
 * @port: The port to check the frame limit on
 * @w: The width of the area to get the capture frame limit for, or %0 for the
 * whole display
 * @h: The height of the area to get the capture frame limit for, or %0 for the
 * whole display
 *
 * Gets the max number of frames we can capture with the Chamelium for the given
 * resolution.
 *
 * Returns: The number of the max number of frames we can capture
 */
int chamelium_get_frame_limit(struct chamelium *chamelium,
			      struct chamelium_port *port,
			      int w, int h)
{
	xmlrpc_value *res;
	int ret;

	if (!w && !h)
		chamelium_port_get_resolution(chamelium, port, &w, &h);

	res = chamelium_rpc(chamelium, port, "GetMaxFrameLimit", "(iii)",
			    port->id, w, h);

	xmlrpc_read_int(&chamelium->env, res, &ret);
	xmlrpc_DECREF(res);

	return ret;
}

static uint32_t chamelium_xrgb_hash16(const unsigned char *buffer, int width,
				      int height, int k, int m)
{
	unsigned char r, g, b;
	uint64_t sum = 0;
	uint64_t count = 0;
	uint64_t value;
	uint32_t hash;
	int index;
	int i;

	for (i=0; i < width * height; i++) {
		if ((i % m) != k)
			continue;

		index = i * 4;

		r = buffer[index + 2];
		g = buffer[index + 1];
		b = buffer[index + 0];

		value = r | (g << 8) | (b << 16);
		sum += ++count * value;
	}

	hash = ((sum >> 0) ^ (sum >> 16) ^ (sum >> 32) ^ (sum >> 48)) & 0xffff;

	return hash;
}

static void chamelium_do_calculate_fb_crc(cairo_surface_t *fb_surface,
					  igt_crc_t *out)
{
	unsigned char *buffer;
	int n = 4;
	int w, h;
	int i, j;

	buffer = cairo_image_surface_get_data(fb_surface);
	w = cairo_image_surface_get_width(fb_surface);
	h = cairo_image_surface_get_height(fb_surface);

	for (i = 0; i < n; i++) {
		j = n - i - 1;
		out->crc[i] = chamelium_xrgb_hash16(buffer, w, h, j, n);
	}

	out->n_words = n;
}

/**
 * chamelium_calculate_fb_crc:
 * @fd: The drm file descriptor
 * @fb: The framebuffer to calculate the CRC for
 *
 * Calculates the CRC for the provided framebuffer, using the Chamelium's CRC
 * algorithm. This calculates the CRC in a synchronous fashion.
 *
 * Returns: The calculated CRC
 */
igt_crc_t *chamelium_calculate_fb_crc(int fd, struct igt_fb *fb)
{
	igt_crc_t *ret = calloc(1, sizeof(igt_crc_t));
	cairo_surface_t *fb_surface;

	/* Get the cairo surface for the framebuffer */
	fb_surface = igt_get_cairo_surface(fd, fb);

	chamelium_do_calculate_fb_crc(fb_surface, ret);

	cairo_surface_destroy(fb_surface);

	return ret;
}

static void *chamelium_calculate_fb_crc_async_work(void *data)
{
	struct chamelium_fb_crc_async_data *fb_crc;

	fb_crc = (struct chamelium_fb_crc_async_data *) data;

	chamelium_do_calculate_fb_crc(fb_crc->fb_surface, fb_crc->ret);

	return NULL;
}

/**
 * chamelium_calculate_fb_crc_launch:
 * @fd: The drm file descriptor
 * @fb: The framebuffer to calculate the CRC for
 *
 * Launches the CRC calculation for the provided framebuffer, using the
 * Chamelium's CRC algorithm. This calculates the CRC in an asynchronous
 * fashion.
 *
 * The returned structure should be passed to a subsequent call to
 * chamelium_calculate_fb_crc_result. It should not be freed.
 *
 * Returns: An intermediate structure for the CRC calculation work.
 */
struct chamelium_fb_crc_async_data *chamelium_calculate_fb_crc_async_start(int fd,
									   struct igt_fb *fb)
{
	struct chamelium_fb_crc_async_data *fb_crc;

	fb_crc = calloc(1, sizeof(struct chamelium_fb_crc_async_data));
	fb_crc->ret = calloc(1, sizeof(igt_crc_t));

	/* Get the cairo surface for the framebuffer */
	fb_crc->fb_surface = igt_get_cairo_surface(fd, fb);

	pthread_create(&fb_crc->thread_id, NULL,
		       chamelium_calculate_fb_crc_async_work, fb_crc);

	return fb_crc;
}

/**
 * chamelium_calculate_fb_crc_result:
 * @fb_crc: An intermediate structure with thread-related information
 *
 * Blocks until the asynchronous CRC calculation is finished, and then returns
 * its result.
 *
 * Returns: The calculated CRC
 */
igt_crc_t *chamelium_calculate_fb_crc_async_finish(struct chamelium_fb_crc_async_data *fb_crc)
{
	igt_crc_t *ret;

	pthread_join(fb_crc->thread_id, NULL);

	ret = fb_crc->ret;
	free(fb_crc);

	return ret;
}

static unsigned int chamelium_get_port_type(struct chamelium *chamelium,
					    struct chamelium_port *port)
{
	xmlrpc_value *res;
	const char *port_type_str;
	unsigned int port_type;

	res = chamelium_rpc(chamelium, NULL, "GetConnectorType",
			    "(i)", port->id);

	xmlrpc_read_string(&chamelium->env, res, &port_type_str);
	igt_debug("Port %d is of type '%s'\n", port->id, port_type_str);

	if (strcmp(port_type_str, "DP") == 0)
		port_type = DRM_MODE_CONNECTOR_DisplayPort;
	else if (strcmp(port_type_str, "HDMI") == 0)
		port_type = DRM_MODE_CONNECTOR_HDMIA;
	else if (strcmp(port_type_str, "VGA") == 0)
		port_type = DRM_MODE_CONNECTOR_VGA;
	else
		port_type = DRM_MODE_CONNECTOR_Unknown;

	free((void*)port_type_str);
	xmlrpc_DECREF(res);

	return port_type;
}

static bool chamelium_has_video_support(struct chamelium *chamelium,
					int port_id)
{
	xmlrpc_value *res;
	int has_video_support;

	res = chamelium_rpc(chamelium, NULL, "HasVideoSupport", "(i)", port_id);
	xmlrpc_read_bool(&chamelium->env, res, &has_video_support);
	xmlrpc_DECREF(res);

	return has_video_support;
}

/**
 * chamelium_get_video_ports: retrieve a list of video port IDs
 *
 * Returns: the number of video port IDs
 */
static size_t chamelium_get_video_ports(struct chamelium *chamelium,
					int port_ids[static CHAMELIUM_MAX_PORTS])
{
	xmlrpc_value *res, *res_port;
	int res_len, i, port_id;
	size_t port_ids_len = 0;

	res = chamelium_rpc(chamelium, NULL, "GetSupportedInputs", "()");
	res_len = xmlrpc_array_size(&chamelium->env, res);
	for (i = 0; i < res_len; i++) {
		xmlrpc_array_read_item(&chamelium->env, res, i, &res_port);
		xmlrpc_read_int(&chamelium->env, res_port, &port_id);
		xmlrpc_DECREF(res_port);

		if (!chamelium_has_video_support(chamelium, port_id))
			continue;

		igt_assert(port_ids_len < CHAMELIUM_MAX_PORTS);
		port_ids[port_ids_len] = port_id;
		port_ids_len++;
	}
	xmlrpc_DECREF(res);

	return port_ids_len;
}

static bool chamelium_read_port_mappings(struct chamelium *chamelium,
					 int drm_fd)
{
	drmModeRes *res;
	drmModeConnector *connector;
	struct chamelium_port *port;
	GError *error = NULL;
	char **group_list;
	char *group, *map_name;
	int port_i, i, j;
	bool ret = true;

	res = drmModeGetResources(drm_fd);
	if (!res)
		return false;

	group_list = g_key_file_get_groups(igt_key_file, NULL);

	/* Count how many connector mappings are specified in the config */
	for (i = 0; group_list[i] != NULL; i++) {
		if (strstr(group_list[i], "Chamelium:"))
			chamelium->port_count++;
	}
	igt_assert(chamelium->port_count <= CHAMELIUM_MAX_PORTS);

	port_i = 0;
	for (i = 0; group_list[i] != NULL; i++) {
		group = group_list[i];

		if (!strstr(group, "Chamelium:"))
			continue;

		map_name = group + (sizeof("Chamelium:") - 1);

		port = &chamelium->ports[port_i++];
		port->name = strdup(map_name);
		port->id = g_key_file_get_integer(igt_key_file, group,
						  "ChameliumPortID",
						  &error);
		if (!port->id) {
			igt_warn("Failed to read chamelium port ID for %s: %s\n",
				 map_name, error->message);
			ret = false;
			goto out;
		}

		port->type = chamelium_get_port_type(chamelium, port);
		if (port->type == DRM_MODE_CONNECTOR_Unknown) {
			igt_warn("Unable to retrieve the physical port type from the Chamelium for '%s'\n",
				 map_name);
			ret = false;
			goto out;
		}

		for (j = 0;
		     j < res->count_connectors && !port->connector_id;
		     j++) {
			char name[50];

			connector = drmModeGetConnectorCurrent(
			    drm_fd, res->connectors[j]);

			/* We have to generate the connector name on our own */
			snprintf(name, 50, "%s-%u",
				 kmstest_connector_type_str(connector->connector_type),
				 connector->connector_type_id);

			if (strcmp(name, map_name) == 0)
				port->connector_id = connector->connector_id;

			drmModeFreeConnector(connector);
		}
		if (!port->connector_id) {
			igt_warn("No connector found with name '%s'\n",
				 map_name);
			ret = false;
			goto out;
		}

		igt_debug("Port '%s' with physical type '%s' mapped to Chamelium port %d\n",
			  map_name, kmstest_connector_type_str(port->type),
			  port->id);
	}

out:
	g_strfreev(group_list);
	drmModeFreeResources(res);

	return ret;
}

static int port_id_from_edid(int drm_fd, drmModeConnector *connector)
{
	int port_id = -1;
	bool ok;
	uint64_t edid_blob_id;
	drmModePropertyBlobRes *edid_blob;
	const struct edid *edid;
	char mfg[3];

	if (connector->connection != DRM_MODE_CONNECTED) {
		igt_debug("Skipping auto-discovery for connector %s-%d: "
			  "connector status is not connected\n",
			  kmstest_connector_type_str(connector->connector_type),
			  connector->connector_type_id);
		return -1;
	}

	ok = kmstest_get_property(drm_fd, connector->connector_id,
				  DRM_MODE_OBJECT_CONNECTOR, "EDID",
				  NULL, &edid_blob_id, NULL);
	if (!ok || !edid_blob_id) {
		igt_debug("Skipping auto-discovery for connector %s-%d: "
			  "missing the EDID property\n",
			  kmstest_connector_type_str(connector->connector_type),
			  connector->connector_type_id);
		return -1;
	}

	edid_blob = drmModeGetPropertyBlob(drm_fd, edid_blob_id);
	igt_assert(edid_blob);

	edid = (const struct edid *) edid_blob->data;

	edid_get_mfg(edid, mfg);
	if (memcmp(mfg, "IGT", 3) != 0) {
		igt_debug("Skipping connector %s-%d for auto-discovery: "
			  "manufacturer is %.3s, not IGT\n",
			  kmstest_connector_type_str(connector->connector_type),
			  connector->connector_type_id, mfg);
		goto out;
	}

	if (edid->prod_code[0] != 'C' || edid->prod_code[1] != 'H') {
		igt_warn("Invalid EDID for IGT connector %s-%d: "
			  "invalid product code\n",
			  kmstest_connector_type_str(connector->connector_type),
			  connector->connector_type_id);
		goto out;
	}

	port_id = *(uint32_t *) &edid->serial;
	igt_debug("Auto-discovery mapped connector %s-%d to Chamelium "
		  "port ID %d\n",
		  kmstest_connector_type_str(connector->connector_type),
		  connector->connector_type_id, port_id);

out:
	drmModeFreePropertyBlob(edid_blob);
	return port_id;
}

/**
 * chamelium_autodiscover: automagically discover the Chamelium port mapping
 *
 * The Chamelium API uses port IDs wheras the Device Under Test uses DRM
 * connectors. We need to know which Chamelium port is plugged to a given DRM
 * connector. This has typically been done via a configuration file in the
 * past (see #chamelium_read_port_mappings), but this function provides an
 * automatic way to do it.
 *
 * We will plug all Chamelium ports with a different EDID on each. Then we'll
 * read the EDID on each DRM connector and infer the Chamelium port ID.
 */
static bool chamelium_autodiscover(struct chamelium *chamelium, int drm_fd)
{
	int candidate_ports[CHAMELIUM_MAX_PORTS];
	size_t candidate_ports_len;
	drmModeRes *res;
	drmModeConnector *connector;
	struct chamelium_port *port;
	size_t i, j, port_count;
	int port_id;
	uint32_t conn_id;
	struct chamelium_edid *edid;
	bool found;
	uint32_t discovered_conns[CHAMELIUM_MAX_PORTS] = {0};
	char conn_name[64];
	struct timespec start;
	uint64_t elapsed_ns;

	candidate_ports_len = chamelium_get_video_ports(chamelium,
							candidate_ports);

	igt_debug("Starting Chamelium port auto-discovery on %zu ports\n",
		  candidate_ports_len);
	igt_gettime(&start);

	edid = chamelium_new_edid(chamelium, igt_kms_get_base_edid());

	/* Set EDID and plug ports we want to auto-discover */
	port_count = chamelium->port_count;
	for (i = 0; i < candidate_ports_len; i++) {
		port_id = candidate_ports[i];

		/* Get or add a chamelium_port slot */
		port = NULL;
		for (j = 0; j < chamelium->port_count; j++) {
			if (chamelium->ports[j].id == port_id) {
				port = &chamelium->ports[j];
				break;
			}
		}
		if (!port) {
			igt_assert(port_count < CHAMELIUM_MAX_PORTS);
			port = &chamelium->ports[port_count];
			port_count++;

			port->id = port_id;
		}

		chamelium_port_set_edid(chamelium, port, edid);
		chamelium_plug(chamelium, port);
	}

	/* Reprobe connectors and build the mapping */
	res = drmModeGetResources(drm_fd);
	if (!res)
		return false;

	for (i = 0; i < res->count_connectors; i++) {
		conn_id = res->connectors[i];

		/* Read the EDID and parse the Chamelium port ID we stored
		 * there. */
		connector = drmModeGetConnector(drm_fd, res->connectors[i]);
		port_id = port_id_from_edid(drm_fd, connector);
		drmModeFreeConnector(connector);
		if (port_id < 0)
			continue;

		/* If we already have a mapping from the config file, check
		 * that it's consistent. */
		found = false;
		for (j = 0; j < chamelium->port_count; j++) {
			port = &chamelium->ports[j];
			if (port->connector_id == conn_id) {
				found = true;
				igt_assert_f(port->id == port_id,
					     "Inconsistency detected in .igtrc: "
					     "connector %s is configured with "
					     "Chamelium port %d, but is "
					     "connected to port %d\n",
					     port->name, port->id, port_id);
				break;
			}
		}
		if (found)
			continue;

		/* We got a new mapping */
		found = false;
		for (j = 0; j < candidate_ports_len; j++) {
			if (port_id == candidate_ports[j]) {
				found = true;
				discovered_conns[j] = conn_id;
				break;
			}
		}
		igt_assert_f(found, "Auto-discovered a port (%d) we haven't "
			     "setup\n", port_id);
	}

	drmModeFreeResources(res);

	/* We now have a Chamelium port ID ↔ DRM connector ID mapping:
	 * candidate_ports contains the Chamelium port IDs and
	 * discovered_conns contains the DRM connector IDs. */
	for (i = 0; i < candidate_ports_len; i++) {
		port_id = candidate_ports[i];
		conn_id = discovered_conns[i];
		if (!conn_id) {
			continue;
		}

		port = &chamelium->ports[chamelium->port_count];
		chamelium->port_count++;

		port->id = port_id;
		port->type = chamelium_get_port_type(chamelium, port);
		port->connector_id = conn_id;

		connector = drmModeGetConnectorCurrent(drm_fd, conn_id);
		snprintf(conn_name, sizeof(conn_name), "%s-%u",
			 kmstest_connector_type_str(connector->connector_type),
			 connector->connector_type_id);
		drmModeFreeConnector(connector);
		port->name = strdup(conn_name);
	}

	elapsed_ns = igt_nsec_elapsed(&start);
	igt_debug("Auto-discovery took %fms\n",
		  (float) elapsed_ns / (1000 * 1000));

	return true;
}

static bool chamelium_read_config(struct chamelium *chamelium, int drm_fd)
{
	GError *error = NULL;

	if (!igt_key_file) {
		igt_warn("No configuration file available for chamelium\n");
		return false;
	}

	chamelium->url = g_key_file_get_string(igt_key_file, "Chamelium", "URL",
					       &error);
	if (!chamelium->url) {
		igt_warn("Couldn't read chamelium URL from config file: %s\n",
			 error->message);
		return false;
	}

	if (!chamelium_read_port_mappings(chamelium, drm_fd)) {
		return false;
	}
	return chamelium_autodiscover(chamelium, drm_fd);
}

/**
 * chamelium_reset:
 * @chamelium: The Chamelium instance to use
 *
 * Resets the chamelium's IO board. As well, this also has the effect of
 * causing all of the chamelium ports to get set to unplugged
 */
void chamelium_reset(struct chamelium *chamelium)
{
	igt_debug("Resetting the chamelium\n");
	xmlrpc_DECREF(chamelium_rpc(chamelium, NULL, "Reset", "()"));
}

static void chamelium_exit_handler(int sig)
{
	igt_debug("Deinitializing Chamelium\n");

	if (cleanup_instance)
		chamelium_deinit(cleanup_instance);
}

/**
 * chamelium_init:
 * @chamelium: The Chamelium instance to use
 * @drm_fd: a display initialized with #igt_display_require
 *
 * Sets up a connection with a chamelium, using the URL specified in the
 * Chamelium configuration. This must be called first before trying to use the
 * chamelium.
 *
 * If we fail to establish a connection with the chamelium, fail to find a
 * configured connector, etc. we fail the current test.
 *
 * Returns: A newly initialized chamelium struct, or NULL on error
 */
struct chamelium *chamelium_init(int drm_fd)
{
	struct chamelium *chamelium = malloc(sizeof(struct chamelium));

	if (!chamelium)
		return NULL;

	/* A chamelium instance was set up previously, so clean it up before
	 * starting a new one
	 */
	if (cleanup_instance)
		chamelium_deinit(cleanup_instance);

	memset(chamelium, 0, sizeof(*chamelium));
	chamelium->drm_fd = drm_fd;
	igt_list_init(&chamelium->edids);

	/* Setup the libxmlrpc context */
	xmlrpc_env_init(&chamelium->env);
	xmlrpc_client_setup_global_const(&chamelium->env);
	xmlrpc_client_create(&chamelium->env, XMLRPC_CLIENT_NO_FLAGS, PACKAGE,
			     PACKAGE_VERSION, NULL, 0, &chamelium->client);
	if (chamelium->env.fault_occurred) {
		igt_debug("Failed to init xmlrpc: %s\n",
			  chamelium->env.fault_string);
		goto error;
	}

	if (!chamelium_read_config(chamelium, drm_fd))
		goto error;

	cleanup_instance = chamelium;
	igt_install_exit_handler(chamelium_exit_handler);

	return chamelium;

error:
	xmlrpc_env_clean(&chamelium->env);
	free(chamelium);

	return NULL;
}

/**
 * chamelium_deinit:
 * @chamelium: The Chamelium instance to use
 *
 * Frees the resources used by a connection to the chamelium that was set up
 * with #chamelium_init. As well, this function restores the state of the
 * chamelium like it was before calling #chamelium_init. This function is also
 * called as an exit handler, so users only need to call manually if they don't
 * want the chamelium interfering with other tests in the same file.
 */
void chamelium_deinit(struct chamelium *chamelium)
{
	int i;
	struct chamelium_edid *pos, *tmp;

	/* We want to make sure we leave all of the ports plugged in, since
	 * testing setups requiring multiple monitors are probably using the
	 * chamelium to provide said monitors
	 */
	chamelium_reset(chamelium);
	for (i = 0; i < chamelium->port_count; i++)
		chamelium_plug(chamelium, &chamelium->ports[i]);

	/* Destroy any EDIDs we created to make sure we don't leak them */
	igt_list_for_each_safe(pos, tmp, &chamelium->edids, link) {
		for (i = 0; i < CHAMELIUM_MAX_PORTS; i++) {
			if (pos->ids[i])
				chamelium_destroy_edid(chamelium, pos->ids[i]);
			free(pos->raw[i]);
		}
		free(pos->base);
		free(pos);
	}

	xmlrpc_client_destroy(chamelium->client);
	xmlrpc_env_clean(&chamelium->env);

	for (i = 0; i < chamelium->port_count; i++)
		free(chamelium->ports[i].name);

	free(chamelium);
}

igt_constructor {
	/* Frame dumps can be large, so we need to be able to handle very large
	 * responses
	 *
	 * Limit here is 15MB
	 */
	xmlrpc_limit_set(XMLRPC_XML_SIZE_LIMIT_ID, 15728640);
}
