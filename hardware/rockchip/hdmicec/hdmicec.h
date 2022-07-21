/*
 * Copyright (C) 2012 The Android Open Source Project
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
#ifndef HDMI_CEC_UEVENT_H
#define HDMI_CEC_UEVENT_H

#include <linux/ioctl.h>

//struct cec_framedata {
//	char srcdestaddr;
//	char opcode;
//	char args[CEC_MESSAGE_BODY_MAX_LENGTH];
//	char argcount;
//	char returnval;
//};

#define HDMI_CEC_VERSION 0x05
#define HDMI_CEC_VENDOR_ID 0x000001
#define HDMI_CEC_PORT_ID 0x000001
#define HDMI_CEC_HAL_VERSION "1.0"

#define CEC_MAX_LOG_ADDRS 4
#define CEC_MAX_MSG_SIZE	16
#define CEC_MODE_MONITOR		(0xe << 4)
#define CEC_MODE_INITIATOR		(0x1 << 0)
#define CEC_MODE_EXCL_FOLLOWER		(0x2 << 4)
#define CEC_MODE_EXCL_FOLLOWER_PASSTHRU	(0x3 << 4)

#define CEC_LOG_ADDR_INVALID		0xff
#define CEC_PHYS_ADDR_INVALID		0xffff

#define CEC_LOG_ADDR_TYPE_TV		0
#define CEC_LOG_ADDR_TYPE_RECORD	1
#define CEC_LOG_ADDR_TYPE_TUNER		2
#define CEC_LOG_ADDR_TYPE_PLAYBACK	3
#define CEC_LOG_ADDR_TYPE_AUDIOSYSTEM	4
#define CEC_LOG_ADDR_TYPE_SPECIFIC	5
#define CEC_LOG_ADDR_TYPE_UNREGISTERED	6

/* Events */

/* event flags */
#define CEC_EVENT_FL_INITIAL_STATE	(1 << 0)
#define CEC_EVENT_FL_DROPPED_EVENTS	(1 << 1)

/* Event that occurs when the adapter state changes */
#define CEC_EVENT_STATE_CHANGE		1
/*
 * This event is sent when messages are lost because the application
 * didn't empty the message queue in time
 */
#define CEC_EVENT_LOST_MSGS		2
#define CEC_EVENT_PIN_CEC_LOW		3
#define CEC_EVENT_PIN_CEC_HIGH		4
#define CEC_EVENT_PIN_HPD_LOW		5
#define CEC_EVENT_PIN_HPD_HIGH		6

/* Primary Device Type Operand (prim_devtype) */
#define CEC_OP_PRIM_DEVTYPE_TV				0
#define CEC_OP_PRIM_DEVTYPE_RECORD			1
#define CEC_OP_PRIM_DEVTYPE_TUNER			3
#define CEC_OP_PRIM_DEVTYPE_PLAYBACK			4
#define CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM			5
#define CEC_OP_PRIM_DEVTYPE_SWITCH			6
#define CEC_OP_PRIM_DEVTYPE_PROCESSOR			7

/* cec_msg tx/rx_status field */
#define CEC_TX_STATUS_OK		(1 << 0)
#define CEC_TX_STATUS_ARB_LOST		(1 << 1)
#define CEC_TX_STATUS_NACK		(1 << 2)
#define CEC_TX_STATUS_LOW_DRIVE		(1 << 3)
#define CEC_TX_STATUS_ERROR		(1 << 4)
#define CEC_TX_STATUS_MAX_RETRIES	(1 << 5)

#define CEC_RX_STATUS_OK		(1 << 0)
#define CEC_RX_STATUS_TIMEOUT		(1 << 1)
#define CEC_RX_STATUS_FEATURE_ABORT	(1 << 2)

struct cec_msg {
	__u64 tx_ts;
	__u64 rx_ts;
	__u32 len;
	__u32 timeout;
	__u32 sequence;
	__u32 flags;
	__u8 msg[CEC_MAX_MSG_SIZE];
	__u8 reply;
	__u8 rx_status;
	__u8 tx_status;
	__u8 tx_arb_lost_cnt;
	__u8 tx_nack_cnt;
	__u8 tx_low_drive_cnt;
	__u8 tx_error_cnt;
};

struct cec_event_lost_msgs {
	__u32 lost_msgs;
};

struct cec_event_state_change {
	__u16 phys_addr;
	__u16 log_addr_mask;
};

struct cec_event {
	__u64 ts;
	__u32 event;
	__u32 flags;
	union {
		struct cec_event_state_change state_change;
		struct cec_event_lost_msgs lost_msgs;
		__u32 raw[16];
	};
};

struct cec_log_addrs {
	__u8 log_addr[CEC_MAX_LOG_ADDRS];
	__u16 log_addr_mask;
	__u8 cec_version;
	__u8 num_log_addrs;
	__u32 vendor_id;
	__u32 flags;
	char osd_name[15];
	__u8 primary_device_type[CEC_MAX_LOG_ADDRS];
	__u8 log_addr_type[CEC_MAX_LOG_ADDRS];

	/* CEC 2.0 */
	__u8 all_device_types[CEC_MAX_LOG_ADDRS];
	__u8 features[CEC_MAX_LOG_ADDRS][12];
};

/* ioctls */

/* Adapter capabilities */
#define CEC_ADAP_G_CAPS		_IOWR('a',  0, struct cec_caps)

/*
 * phys_addr is either 0 (if this is the CEC root device)
 * or a valid physical address obtained from the sink's EDID
 * as read by this CEC device (if this is a source device)
 * or a physical address obtained and modified from a sink
 * EDID and used for a sink CEC device.
 * If nothing is connected, then phys_addr is 0xffff.
 * See HDMI 1.4b, section 8.7 (Physical Address).
 *
 * The CEC_ADAP_S_PHYS_ADDR ioctl may not be available if that is handled
 * internally.
 */
#define CEC_ADAP_G_PHYS_ADDR	_IOR('a',  1, __u16)
#define CEC_ADAP_S_PHYS_ADDR	_IOW('a',  2, __u16)

/*
 * Configure the CEC adapter. It sets the device type and which
 * logical types it will try to claim. It will return which
 * logical addresses it could actually claim.
 * An error is returned if the adapter is disabled or if there
 * is no physical address assigned.
 */

#define CEC_ADAP_G_LOG_ADDRS	_IOR('a',  3, struct cec_log_addrs)
#define CEC_ADAP_S_LOG_ADDRS	_IOWR('a',  4, struct cec_log_addrs)

/* Transmit/receive a CEC command */
#define CEC_TRANSMIT		_IOWR('a',  5, struct cec_msg)
#define CEC_RECEIVE		_IOWR('a',  6, struct cec_msg)

/* Dequeue CEC events */
#define CEC_DQEVENT		_IOWR('a',  7, struct cec_event)

/*
 * Get and set the message handling mode for this filehandle.
 */
#define CEC_G_MODE		_IOR('a',  8, __u32)
#define CEC_S_MODE		_IOW('a',  9, __u32)


#define HDMI_STATE_PATH   "sys/class/drm/card0-HDMI-A-1/status"
#define HDMI_DEV_PATH   "dev/cec0"
struct hdmi_cec_context_t {
    hdmi_cec_device_t device;
    /* our private state goes below here */
	event_callback_t event_callback;
	void* cec_arg;
	struct hdmi_port_info port;
	int fd;
	bool enable;
	bool system_control;
	int phy_addr;
	bool hotplug;
	bool cec_init;
};

void init_uevent_thread(hdmi_cec_context_t* ctx);
#endif
