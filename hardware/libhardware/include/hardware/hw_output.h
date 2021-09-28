/*
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_HW_OUTPUT_INTERFACE_H
#define ANDROID_HW_OUTPUT_INTERFACE_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <cutils/native_handle.h>

__BEGIN_DECLS

#define HW_OUTPUT_MODULE_API_VERSION_0_1  HARDWARE_MODULE_API_VERSION(0, 1)

#define HW_OUTPUT_DEVICE_API_VERSION_0_1  HARDWARE_DEVICE_API_VERSION(0, 1)

/*
 * The id of this module
 */
#define HW_OUTPUT_HARDWARE_MODULE_ID "hw_output"

#define HW_OUTPUT_DEFAULT_DEVICE "hw_output_device"

/*****************************************************************************/

/*
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
typedef struct hw_output_module {
    struct hw_module_t common;
} hw_output_module_t;

/*****************************************************************************/
typedef struct drm_mode {
    uint32_t width;
    uint32_t height;
    float refreshRate;
    uint32_t clock;
    uint32_t flags;
    uint32_t interlaceFlag;
    uint32_t yuvFlag;
    uint32_t connectorId;
    uint32_t mode_type;
    uint32_t idx;
    uint32_t hsync_start;
    uint32_t hsync_end;
    uint32_t htotal;
    uint32_t hskew;
    uint32_t vsync_start;
    uint32_t vsync_end;
    uint32_t vtotal;
    uint32_t vscan;

}drm_mode_t;

typedef struct connector_info {
    uint32_t type;
    uint32_t id;
    uint32_t state;
}connector_info_t;

typedef uint32_t hw_output_type_t;

typedef struct hw_output_device_info {
    /* Device ID */
    int device_id;

    /* Type of physical TV input. */
    hw_output_type_t type;

    union {
        struct {
            /* HDMI port ID number */
            uint32_t port_id;
        } hdmi;

        /* TODO: add other type specific information. */

        int32_t type_info_reserved[16];
    };

    /* TODO: Add capability if necessary. */

    int32_t reserved[16];
} hw_output_device_info_t;

/* See tv_input_event_t for more details. */
enum {
    /*
     * Hardware notifies the framework that a device is available.
     *
     * Note that DEVICE_AVAILABLE and DEVICE_UNAVAILABLE events do not represent
     * hotplug events (i.e. plugging cable into or out of the physical port).
     * These events notify the framework whether the port is available or not.
     * For a concrete example, when a user plugs in or pulls out the HDMI cable
     * from a HDMI port, it does not generate DEVICE_AVAILABLE and/or
     * DEVICE_UNAVAILABLE events. However, if a user inserts a pluggable USB
     * tuner into the Android device, it will generate a DEVICE_AVAILABLE event
     * and when the port is removed, it should generate a DEVICE_UNAVAILABLE
     * event.
     *
     * For hotplug events, please see STREAM_CONFIGURATION_CHANGED for more
     * details.
     *
     * HAL implementation should register devices by using this event when the
     * device boots up. The framework will recognize device reported via this
     * event only. In addition, the implementation could use this event to
     * notify the framework that a removable TV input device (such as USB tuner
     * as stated in the example above) is attached.
     */
    TV_INPUT_EVENT_DEVICE_AVAILABLE = 1,
    /*
     * Hardware notifies the framework that a device is unavailable.
     *
     * HAL implementation should generate this event when a device registered
     * by TV_INPUT_EVENT_DEVICE_AVAILABLE is no longer available. For example,
     * the event can indicate that a USB tuner is plugged out from the Android
     * device.
     *
     * Note that this event is not for indicating cable plugged out of the port;
     * for that purpose, the implementation should use
     * STREAM_CONFIGURATION_CHANGED event. This event represents the port itself
     * being no longer available.
     */
    TV_INPUT_EVENT_DEVICE_UNAVAILABLE = 2,
    /*
     * Stream configurations are changed. Client should regard all open streams
     * at the specific device are closed, and should call
     * get_stream_configurations() again, opening some of them if necessary.
     *
     * HAL implementation should generate this event when the available stream
     * configurations change for any reason. A typical use case of this event
     * would be to notify the framework that the input signal has changed
     * resolution, or that the cable is plugged out so that the number of
     * available streams is 0.
     *
     * The implementation may use this event to indicate hotplug status of the
     * port. the framework regards input devices with no available streams as
     * disconnected, so the implementation can generate this event with no
     * available streams to indicate that this device is disconnected, and vice
     * versa.
     */
    TV_INPUT_EVENT_STREAM_CONFIGURATIONS_CHANGED = 3,
    /*
     * Hardware is done with capture request with the buffer. Client can assume
     * ownership of the buffer again.
     *
     * HAL implementation should generate this event after request_capture() if
     * it succeeded. The event shall have the buffer with the captured image.
     */
    TV_INPUT_EVENT_CAPTURE_SUCCEEDED = 4,
    /*
     * Hardware met a failure while processing a capture request or client
     * canceled the request. Client can assume ownership of the buffer again.
     *
     * The event is similar to TV_INPUT_EVENT_CAPTURE_SUCCEEDED, but HAL
     * implementation generates this event upon a failure to process
     * request_capture(), or a request cancellation.
     */
    TV_INPUT_EVENT_CAPTURE_FAILED = 5,
};
typedef uint32_t hw_output_event_type_t;

typedef struct hw_output_capture_result {
    /* Device ID */
    int device_id;

    /* Stream ID */
    int stream_id;

    /* Sequence number of the request */
    uint32_t seq;

    /*
     * The buffer passed to hardware in request_capture(). The content of
     * buffer is undefined (although buffer itself is valid) for
     * TV_INPUT_CAPTURE_FAILED event.
     */
    buffer_handle_t buffer;

    /*
     * Error code for the request. -ECANCELED if request is cancelled; other
     * error codes are unknown errors.
     */
    int error_code;
} hw_output_capture_result_t;

typedef struct hw_output_event {
    hw_output_event_type_t type;

    union {
        /*
         * TV_INPUT_EVENT_DEVICE_AVAILABLE: all fields are relevant
         * TV_INPUT_EVENT_DEVICE_UNAVAILABLE: only device_id is relevant
         * TV_INPUT_EVENT_STREAM_CONFIGURATIONS_CHANGED: only device_id is
         *    relevant
         */
        hw_output_device_info_t device_info;
        /*
         * TV_INPUT_EVENT_CAPTURE_SUCCEEDED: error_code is not relevant
         * TV_INPUT_EVENT_CAPTURE_FAILED: all fields are relevant
         */
        hw_output_capture_result_t capture_result;
    };
} hw_output_event_t;

typedef struct hw_output_callback_ops {
    /*
     * event contains the type of the event and additional data if necessary.
     * The event object is guaranteed to be valid only for the duration of the
     * call.
     *
     * data is an object supplied at device initialization, opaque to the
     * hardware.
     */
    void (*notify)(struct hw_output_device* dev,
            hw_output_event_t* event, void* data);
} hw_output_callback_ops_t;

enum {
    TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE = 1,
    TV_STREAM_TYPE_BUFFER_PRODUCER = 2,
};
typedef uint32_t hw_stream_type_t;

typedef struct hw_stream_config {
    /*
     * ID number of the stream. This value is used to identify the whole stream
     * configuration.
     */
    int stream_id;

    /* Type of the stream */
    hw_stream_type_t type;

    /* Max width/height of the stream. */
    uint32_t max_video_width;
    uint32_t max_video_height;
} hw_stream_config_t;

typedef struct buffer_producer_stream {
    /*
     * IN/OUT: Width / height of the stream. Client may request for specific
     * size but hardware may change it. Client must allocate buffers with
     * specified width and height.
     */
    uint32_t width;
    uint32_t height;

    /* OUT: Client must set this usage when allocating buffer. */
    uint32_t usage;

    /* OUT: Client must allocate a buffer with this format. */
    uint32_t format;
} buffer_producer_stream_t;

typedef struct hw_stream {
    /* IN: ID in the stream configuration */
    int stream_id;

    /* OUT: Type of the stream (for convenience) */
    hw_stream_type_t type;

    /* Data associated with the stream for client's use */
    union {
        /* OUT: A native handle describing the sideband stream source */
        native_handle_t* sideband_stream_source_handle;

        /* IN/OUT: Details are in buffer_producer_stream_t */
        buffer_producer_stream_t buffer_producer;
    };
} hw_stream_t;

/*
 * Every device data structure must begin with hw_device_t
 * followed by module specific public methods and attributes.
 */
typedef struct hw_output_device {
    struct hw_device_t common;

    /*
     * initialize:
     *
     * Provide callbacks to the device and start operation. At first, no device
     * is available and after initialize() completes, currently available
     * devices including static devices should notify via callback.
     *
     * Framework owns callbacks object.
     *
     * data is a framework-owned object which would be sent back to the
     * framework for each callback notifications.
     *
     * Return 0 on success.
     */
    int (*initialize)(struct hw_output_device* dev,
         void* data);
	int (*setMode)(struct hw_output_device* dev,
         int dpy, const char* mode);
	int (*setGamma)(struct hw_output_device* dev,
        int dpy,  uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b);
	int (*setBrightness)(struct hw_output_device* dev,
         int dpy, int bright);
	int (*setContrast)(struct hw_output_device* dev,
         int dpy, int contrast);
	int (*setSat)(struct hw_output_device* dev,
         int dpy, int sat);
	int (*setHue)(struct hw_output_device* dev,
         int dpy, int hue);
	int (*setScreenScale)(struct hw_output_device* dev,
         int dpy, int direction, int value);
	int (*setHdrMode)(struct hw_output_device* dev,
         int dpy, int hdrMode);
	int (*setColorMode)(struct hw_output_device* dev,
         int dpy, const char* colorMode);

	int (*getCurColorMode)(struct hw_output_device* dev,
         int dpy, char* curColorMode);
	int (*getCurMode)(struct hw_output_device* dev,
         int dpy, char* curMode);
	int (*getNumConnectors)(struct hw_output_device* dev,
         int dpy, int* numConnectors);
	
	int (*getConnectorState)(struct hw_output_device* dev,
         int dpy, int* state);

	int (*getBuiltIn)(struct hw_output_device* dev,
         int dpy, int* builtin);

	int (*getColorConfigs)(struct hw_output_device* dev,
         int dpy, int* configs);

	int (*getOverscan)(struct hw_output_device* dev,
         int dpy, uint32_t* overscans);

	int (*getBcsh)(struct hw_output_device* dev,
         int dpy, uint32_t* bcshs);

	drm_mode_t* (*getDisplayModes)(struct hw_output_device* dev, 
         int dpy, uint32_t* size);

	void (*hotplug)(struct hw_output_device* dev);
	void (*saveConfig)(struct hw_output_device* dev);
	int (*set3DMode)(struct hw_output_device* dev, const char* mode);
	int (*set3DLut)(struct hw_output_device* dev,
        int dpy,  uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b);
	connector_info_t* (*getConnectorInfo)(struct hw_output_device* dev, uint32_t* size);
	int (*updateDispHeader)(struct hw_output_device* dev);
} hw_output_device_t;
/** convenience API for opening and closing a supported device */

static inline int hw_output_open(const struct hw_module_t* module, 
        hw_output_device_t** device) {
    return module->methods->open(module, 
            HW_OUTPUT_DEFAULT_DEVICE, TO_HW_DEVICE_T_OPEN(device));
}

static inline int hw_output_close(hw_output_device_t* device) {
    return device->common.close(&device->common);
}

__END_DECLS

#endif  // ANDROID_TV_INPUT_INTERFACE_H
