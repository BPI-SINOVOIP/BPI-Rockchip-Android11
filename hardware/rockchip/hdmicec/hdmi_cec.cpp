/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <hardware/hdmi_cec.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <stdlib.h>
#include <string.h>
#include <hdmicec.h>
#include <unistd.h>
#include <cutils/properties.h>

static int hdmi_cec_device_open(const struct hw_module_t* module, const char* name,
				struct hw_device_t** device);

static struct hw_module_methods_t hdmi_cec_module_methods = {
	open: hdmi_cec_device_open
};

hdmi_module_t HAL_MODULE_INFO_SYM = {
	common: {
		tag: HARDWARE_MODULE_TAG,
		version_major: 1,
		version_minor: 0,
		id: HDMI_CEC_HARDWARE_MODULE_ID,
		name: "Rockchip hdmi cec module",
		author: "Rockchip",
		methods: &hdmi_cec_module_methods,
	}
};

static int logicaddr_to_type(cec_logical_address_t addr)
{
	int type;

	switch (addr) {
	case CEC_ADDR_TV:
		type = CEC_LOG_ADDR_TYPE_TV;
	break;
	case CEC_ADDR_RECORDER_1:
	case CEC_ADDR_RECORDER_2:
	case CEC_ADDR_RECORDER_3:
		type = CEC_LOG_ADDR_TYPE_RECORD;
	break;
	case CEC_ADDR_TUNER_1:
	case CEC_ADDR_TUNER_2:
	case CEC_ADDR_TUNER_3:
	case CEC_ADDR_TUNER_4:
		type = CEC_LOG_ADDR_TYPE_TUNER;
	break;
	case CEC_ADDR_PLAYBACK_1:
	case CEC_ADDR_PLAYBACK_2:
	case CEC_ADDR_PLAYBACK_3:
		type = CEC_LOG_ADDR_TYPE_PLAYBACK;
	break;
	case CEC_ADDR_AUDIO_SYSTEM:
		type = CEC_LOG_ADDR_TYPE_AUDIOSYSTEM;
	break;
	default:
		type = -1;
	}

	return type;
}

static int latype_to_devtype(int latype)
{
	int devtype;

	switch (latype) {
	case CEC_LOG_ADDR_TYPE_TV:
		devtype = CEC_OP_PRIM_DEVTYPE_TV;
	break;
	case CEC_LOG_ADDR_TYPE_RECORD:
		devtype = CEC_OP_PRIM_DEVTYPE_RECORD;
	break;
	case CEC_LOG_ADDR_TYPE_TUNER:
		devtype = CEC_OP_PRIM_DEVTYPE_TUNER;
	break;
	case CEC_LOG_ADDR_TYPE_PLAYBACK:
		devtype = CEC_OP_PRIM_DEVTYPE_PLAYBACK;
	break;
	case CEC_LOG_ADDR_TYPE_AUDIOSYSTEM:
		devtype = CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM;
	break;
	default:
		devtype = -1;
	}

	return devtype;
}

static int set_kernel_logical_address(struct hdmi_cec_context_t* ctx, cec_logical_address_t addr)
{
	int ret, la_type, dev_type;
	int mode = CEC_MODE_INITIATOR | CEC_MODE_EXCL_FOLLOWER_PASSTHRU;
	struct cec_log_addrs log_addr;

	ALOGD("%s, logic addr:%02x\n", __func__, addr);

	if (ctx->fd < 0) {
		ALOGE("%s open error", __func__);
		return -ENOENT;
	}

	la_type = logicaddr_to_type(addr);
	if (la_type < 0) {
		ALOGE("%s invalid logic type\n", __func__);
		return -EINVAL;
	}

	dev_type = latype_to_devtype(la_type);
	if (dev_type < 0) {
		ALOGE("%s invalid device type\n", __func__);
		return -EINVAL;
	}

	ret = ioctl(ctx->fd, CEC_S_MODE, &mode);
	if (ret) {
		ALOGE("CEC set mode error!\n");
		return ret;
	}

	ret = ioctl(ctx->fd, CEC_ADAP_G_LOG_ADDRS, &log_addr);
	if (ret) {
		ALOGE("%s get logic address err ret:%d\n", __func__, ret);
		return -EINVAL;
	}

	ALOGI("primary_device_type:%02x,log_addr_type:%02x,log_addr[0]:%02x\n",
	      log_addr.primary_device_type[0], log_addr.log_addr_type[0], log_addr.log_addr[0]);
	if (log_addr.log_addr[0] != CEC_LOG_ADDR_INVALID && log_addr.log_addr[0]) {
		ALOGD("LA is existing, not need to set logic addr\n");
		return 0;
	}

	log_addr.cec_version = HDMI_CEC_VERSION;
	log_addr.num_log_addrs = 1;
	log_addr.log_addr[0] = addr;
	log_addr.vendor_id = HDMI_CEC_VENDOR_ID;
	log_addr.osd_name[0] = 'R';
	log_addr.osd_name[1] = 'K';
	log_addr.primary_device_type[0] = dev_type;
	log_addr.log_addr_type[0] = la_type;

	ret = ioctl(ctx->fd, CEC_ADAP_S_LOG_ADDRS, &log_addr);
	if (ret) {
		ALOGE("%s set logic address err ret:%d\n", __func__, ret);
		return -1;
	}

	return 0;
}

static int hdmi_cec_add_logical_address(const struct hdmi_cec_device* dev,
					cec_logical_address_t addr)
{

	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	return set_kernel_logical_address(ctx, addr);
}

static void hdmi_cec_clear_logical_address(const struct hdmi_cec_device* dev)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	int ret;
	struct cec_log_addrs log_addr;

	ALOGI("%s", __func__);
	if (ctx->fd < 0) {
		ALOGE("%s open error!", __func__);
		return;
	}

	if (!ctx->cec_init) {
		ALOGI("%s cec is not init!", __func__);
		return;
	}

	log_addr.num_log_addrs = 0;
	ret = ioctl(ctx->fd, CEC_ADAP_S_LOG_ADDRS, &log_addr);
	if (ret) {
		ALOGE("%s set logic address err ret:%d\n", __func__, ret);
		return;
	}
}

static int hdmi_cec_get_physical_address(const struct hdmi_cec_device* dev, uint16_t* addr)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	int ret, i;
	uint16_t val = 0;

	if (addr == NULL) {
		ALOGE("%s addr is null", __func__);
		return -ENXIO;
	}

	if (ctx->fd < 0) {
		ALOGE("%s open error!", __func__);
		return -ENOENT;
	}

	for (i = 0; i < 5; i++) {
		ret = ioctl(ctx->fd, CEC_ADAP_G_PHYS_ADDR, &val);
		if (ret) {
			ALOGE("CEC read physical addr error! ret:%d\n", ret);
			return ret;
		}

		if((val != 0xffff) && val) {
			break;
		}
		usleep(20000);
	}

	if (i == 5) {
		ALOGE("get phy addr err!:%x\n", val);
		return -EINVAL;
	}

	*addr = val;
	ALOGI("%s val = %x", __func__, val);

	return 0;
}

static int hdmi_cec_is_connected(const struct hdmi_cec_device* dev, int port_id)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	if (ctx->hotplug)
		return HDMI_CONNECTED;
	else
		return HDMI_NOT_CONNECTED;
}

static int hdmi_cec_send_message(const struct hdmi_cec_device* dev, const cec_message_t* message)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	struct cec_msg cecframe;
	int i, ret = 0;

	if (!ctx->enable) {
		ALOGE("%s cec disabled\n", __func__);
		return -EPERM;
	}

	if (ctx->fd < 0) {
		ALOGE("%s open error", __func__);
		return -ENOENT;
	}

	if(!ctx->hotplug)
		return -EPERM;

	memset(&cecframe, 0, sizeof(struct cec_msg));
	if (message->initiator == message->destination) {
		struct cec_log_addrs log_addr;

		ret = ioctl(ctx->fd, CEC_ADAP_G_LOG_ADDRS, &log_addr);
		if (ret) {
			ALOGE("%s get logic address err ret:%d\n", __func__, ret);
			return -EINVAL;
		}

		ALOGD("kernel logic addr:%02x, preferred logic addr:%02x",
		      log_addr.log_addr[0], message->initiator);
		if (log_addr.log_addr[0] != CEC_LOG_ADDR_INVALID && log_addr.log_addr[0]) {
			ALOGI("kernel logaddr is existing\n");
			if (log_addr.log_addr[0] == message->initiator) {
				ALOGI("kernel logaddr is preferred logaddr\n");
				return HDMI_RESULT_NACK;
			} else {
				ALOGI("preferred log addr is not kernel log addr\n");
				return HDMI_RESULT_SUCCESS;
			}
		} else {
			ALOGI("kernel logaddr is not existing\n");
			if(!set_kernel_logical_address(ctx, message->initiator)) {
				for (i = 0; i < 5; i++) {
					if (!ctx->phy_addr || ctx->phy_addr == 0xffff) {
						ALOGE("phy addr not ready\n");
						usleep(200000);
					} else {
						break;
					}
				}
			}
			if (i == 5) {
				ALOGE("can't make kernel addr done\n");
				return HDMI_RESULT_FAIL;
			} else {

				return HDMI_RESULT_NACK;
			}
		}
	}

	cecframe.msg[0] = (message->initiator << 4) | message->destination;
	cecframe.len = message->length + 1;
	cecframe.msg[1] = message->body[0];
	ALOGI("send msg LEN:%d,opcode:%02x,addr:%02x\n",
	      cecframe.len ,cecframe.msg[1],cecframe.msg[0]);
	if (cecframe.len > 16)
		cecframe.len = 0;
	for (ret = 0; ret < cecframe.len; ret++)
		cecframe.msg[ret + 2] = message->body[ret + 1];
	if (cecframe.msg[1] == 0x90)
		cecframe.msg[2] = 0;

	ret = ioctl(ctx->fd, CEC_TRANSMIT, &cecframe);

 	if (ret < 0) {
 		ALOGE("ioctl err:%d\n", ret);
		return HDMI_RESULT_FAIL;
	}
	if (cecframe.tx_status & CEC_TX_STATUS_NACK) {
		ALOGE("HDMI_RESULT_NACK\n");
		return HDMI_RESULT_NACK;
	}
	else if (cecframe.tx_status & CEC_TX_STATUS_OK) {
		ALOGE("HDMI_RESULT_SUCCESS\n");
		return HDMI_RESULT_SUCCESS;
	}
	else {
		ALOGE("HDMI_RESULT_BUSY\n");
		return HDMI_RESULT_BUSY;
	}
	return HDMI_RESULT_FAIL;
}

static void hdmi_cec_register_event_callback(const struct hdmi_cec_device* dev,
					     event_callback_t callback, void* arg)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	ALOGI("%s", __func__);
	ctx->event_callback = callback;
	ctx->cec_arg = arg;
}

static void hdmi_cec_get_version(const struct hdmi_cec_device* dev, int* version)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	ALOGI("%s", __func__);
	*version = HDMI_CEC_VERSION;
}

static void hdmi_cec_get_vendor_id(const struct hdmi_cec_device* dev, uint32_t* vendor_id)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	ALOGI("%s", __func__);
	*vendor_id = HDMI_CEC_VENDOR_ID;
}

static void hdmi_cec_get_port_info(const struct hdmi_cec_device* dev,
				   struct hdmi_port_info* list[], int* total)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	int ret, val, support;

	ret = 0;
	val = 0;
	support = 0;

	ALOGI("%s", __func__);
	if (ctx->fd > 0) {
		ret = ioctl(ctx->fd, CEC_ADAP_G_PHYS_ADDR, &val);
		if (!ret) {
			ALOGE("%s get port phy addr %x\n", __func__, val);
			if (val && (val != 0xffff))
				support = 1;
		}
	} else {
		ALOGE("%s open HDMI_DEV_PATH error", __FUNCTION__);
	}
	list[0] = &ctx->port;
	list[0]->type = HDMI_OUTPUT;
	list[0]->port_id = HDMI_CEC_PORT_ID;
	list[0]->cec_supported = support;
	list[0]->arc_supported = 0;
	list[0]->physical_address = val;
	*total = 1;
}

static void hdmi_cec_set_option(const struct hdmi_cec_device* dev, int flag, int value)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	int ret, fd;

	if (ctx->fd < 0) {
		ALOGE("%s open error", __func__);
		return;
	}

	switch (flag) {
		case HDMI_OPTION_WAKEUP:
			ALOGI("%s: Wakeup: value: %d", __FUNCTION__, value);
			break;
		case HDMI_OPTION_ENABLE_CEC:
			ALOGI("%s: Enable CEC: value: %d", __FUNCTION__, value);
			ctx->enable = !!value;
			break;
		case HDMI_OPTION_SYSTEM_CEC_CONTROL:
			ALOGI("%s: system_control: value: %d",
			      __FUNCTION__, value);
			ctx->system_control = !!value;
			break;
	}
}

static void hdmi_cec_set_audio_return_channel(const struct hdmi_cec_device* dev, int port_id, int flag)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	ALOGI("%s %d", __func__, port_id);
}

static int hdmi_cec_device_close(struct hw_device_t *dev)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	if (ctx) {
		close(ctx->fd);
		free(ctx);
	}
	ctx->enable = false;
	ctx->phy_addr = 0;

	return 0;
}

static int hdmi_cec_device_open(const struct hw_module_t* module, const char* name,
				struct hw_device_t** device)
{
	if (strcmp(name, HDMI_CEC_HARDWARE_INTERFACE))
		return -EINVAL;

	struct hdmi_cec_context_t *dev;
	dev = (hdmi_cec_context_t*)malloc(sizeof(*dev));

	/* initialize our state here */
	memset(dev, 0, sizeof(*dev));

	dev->enable = true;
	dev->system_control = false;
	dev->cec_init = false;
	/* initialize the procs */
	dev->device.common.tag = HARDWARE_DEVICE_TAG;
	dev->device.common.version = HDMI_CEC_DEVICE_API_VERSION_1_0;
	dev->device.common.module = const_cast<hw_module_t*>(module);
	dev->device.common.close = hdmi_cec_device_close;

	dev->device.add_logical_address = hdmi_cec_add_logical_address;
	dev->device.clear_logical_address = hdmi_cec_clear_logical_address;
	dev->device.get_physical_address = hdmi_cec_get_physical_address;
	dev->device.send_message = hdmi_cec_send_message;
	dev->device.register_event_callback = hdmi_cec_register_event_callback;
	dev->device.get_version = hdmi_cec_get_version;
	dev->device.get_vendor_id = hdmi_cec_get_vendor_id;
	dev->device.get_port_info = hdmi_cec_get_port_info;
	dev->device.set_option = hdmi_cec_set_option;
	dev->device.set_audio_return_channel = hdmi_cec_set_audio_return_channel;
	dev->device.is_connected = hdmi_cec_is_connected;
	dev->phy_addr = 0;
	dev->fd = open(HDMI_DEV_PATH,O_RDWR,0);
	ALOGE(HDMI_DEV_PATH);
	ALOGE("\n");
	if (dev->fd < 0) {
		ALOGE("%s open error!", __func__);
		ALOGE("cec %s\n", strerror(errno));
	}
	ALOGI("%s dev->fd = %d", __func__, dev->fd);
	property_set("vendor.sys.hdmicec.version",HDMI_CEC_HAL_VERSION);
	*device = &dev->device.common;
	init_uevent_thread(dev);

	ALOGI("rockchip hdmi cec modules loaded");
	return 0;
}
