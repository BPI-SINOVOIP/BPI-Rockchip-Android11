/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "UinputDevice.h"

#include <android-base/stringprintf.h>

namespace android {

// --- UinputDevice ---

UinputDevice::UinputDevice(const char* name) : mName(name) {}

UinputDevice::~UinputDevice() {
    if (ioctl(mDeviceFd, UI_DEV_DESTROY)) {
        ALOGE("Error while destroying uinput device: %s", strerror(errno));
    }
    mDeviceFd.reset();
}

void UinputDevice::init() {
    mDeviceFd = android::base::unique_fd(open("/dev/uinput", O_WRONLY | O_NONBLOCK));
    if (mDeviceFd < 0) {
        FAIL() << "Can't open /dev/uinput :" << strerror(errno);
    }

    struct uinput_user_dev device = {};
    strlcpy(device.name, mName, UINPUT_MAX_NAME_SIZE);
    device.id.bustype = BUS_USB;
    device.id.vendor = 0x01;
    device.id.product = 0x01;
    device.id.version = 1;

    ASSERT_NO_FATAL_FAILURE(configureDevice(mDeviceFd, &device));

    if (write(mDeviceFd, &device, sizeof(device)) < 0) {
        FAIL() << "Could not write uinput_user_dev struct into uinput file descriptor: "
               << strerror(errno);
    }

    if (ioctl(mDeviceFd, UI_DEV_CREATE)) {
        FAIL() << "Error in ioctl : UI_DEV_CREATE: " << strerror(errno);
    }
}

void UinputDevice::injectEvent(uint16_t type, uint16_t code, int32_t value) {
    struct input_event event = {};
    event.type = type;
    event.code = code;
    event.value = value;
    event.time = {}; // uinput ignores the timestamp

    if (write(mDeviceFd, &event, sizeof(input_event)) < 0) {
        std::string msg = base::StringPrintf("Could not write event %" PRIu16 " %" PRIu16
                                             " with value %" PRId32 " : %s",
                                             type, code, value, strerror(errno));
        ALOGE("%s", msg.c_str());
        FAIL() << msg.c_str();
    }
}

// --- UinputKeyboard ---

UinputKeyboard::UinputKeyboard(std::initializer_list<int> keys)
      : UinputDevice(UinputKeyboard::KEYBOARD_NAME), mKeys(keys.begin(), keys.end()) {}

void UinputKeyboard::configureDevice(int fd, uinput_user_dev* device) {
    // enable key press/release event
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY)) {
        FAIL() << "Error in ioctl : UI_SET_EVBIT : EV_KEY: " << strerror(errno);
    }

    // enable set of KEY events
    std::for_each(mKeys.begin(), mKeys.end(), [fd](int key) {
        if (ioctl(fd, UI_SET_KEYBIT, key)) {
            FAIL() << "Error in ioctl : UI_SET_KEYBIT : " << key << " : " << strerror(errno);
        }
    });

    // enable synchronization event
    if (ioctl(fd, UI_SET_EVBIT, EV_SYN)) {
        FAIL() << "Error in ioctl : UI_SET_EVBIT : EV_SYN: " << strerror(errno);
    }
}

void UinputKeyboard::pressKey(int key) {
    if (mKeys.find(key) == mKeys.end()) {
        FAIL() << mName << ": Cannot inject key press: Key not found: " << key;
    }
    injectEvent(EV_KEY, key, 1);
    injectEvent(EV_SYN, SYN_REPORT, 0);
}

void UinputKeyboard::releaseKey(int key) {
    if (mKeys.find(key) == mKeys.end()) {
        FAIL() << mName << ": Cannot inject key release: Key not found: " << key;
    }
    injectEvent(EV_KEY, key, 0);
    injectEvent(EV_SYN, SYN_REPORT, 0);
}

void UinputKeyboard::pressAndReleaseKey(int key) {
    pressKey(key);
    releaseKey(key);
}

// --- UinputHomeKey ---

UinputHomeKey::UinputHomeKey() : UinputKeyboard({KEY_HOME}) {}

void UinputHomeKey::pressAndReleaseHomeKey() {
    pressAndReleaseKey(KEY_HOME);
}

// --- UinputSteamController
UinputSteamController::UinputSteamController() : UinputKeyboard({BTN_GEAR_DOWN, BTN_GEAR_UP}) {}

// --- UinputTouchScreen ---
UinputTouchScreen::UinputTouchScreen(const Rect* size)
      : UinputDevice(UinputTouchScreen::DEVICE_NAME), mSize(*size) {}

void UinputTouchScreen::configureDevice(int fd, uinput_user_dev* device) {
    // Setup the touch screen device
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_REL);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_SLOT);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_TOUCH_MAJOR);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_TOOL_TYPE);
    ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);

    device->absmin[ABS_MT_SLOT] = RAW_SLOT_MIN;
    device->absmax[ABS_MT_SLOT] = RAW_SLOT_MAX;
    device->absmin[ABS_MT_TOUCH_MAJOR] = RAW_TOUCH_MIN;
    device->absmax[ABS_MT_TOUCH_MAJOR] = RAW_TOUCH_MAX;
    device->absmin[ABS_MT_POSITION_X] = mSize.left;
    device->absmax[ABS_MT_POSITION_X] = mSize.right - 1;
    device->absmin[ABS_MT_POSITION_Y] = mSize.top;
    device->absmax[ABS_MT_POSITION_Y] = mSize.bottom - 1;
    device->absmin[ABS_MT_TRACKING_ID] = RAW_ID_MIN;
    device->absmax[ABS_MT_TRACKING_ID] = RAW_ID_MAX;
}

void UinputTouchScreen::sendSlot(int32_t slot) {
    injectEvent(EV_ABS, ABS_MT_SLOT, slot);
}

void UinputTouchScreen::sendTrackingId(int32_t trackingId) {
    injectEvent(EV_ABS, ABS_MT_TRACKING_ID, trackingId);
}

void UinputTouchScreen::sendDown(const Point& point) {
    injectEvent(EV_KEY, BTN_TOUCH, 1);
    injectEvent(EV_ABS, ABS_MT_POSITION_X, point.x);
    injectEvent(EV_ABS, ABS_MT_POSITION_Y, point.y);
    injectEvent(EV_SYN, SYN_REPORT, 0);
}

void UinputTouchScreen::sendMove(const Point& point) {
    injectEvent(EV_ABS, ABS_MT_POSITION_X, point.x);
    injectEvent(EV_ABS, ABS_MT_POSITION_Y, point.y);
    injectEvent(EV_SYN, SYN_REPORT, 0);
}

void UinputTouchScreen::sendUp() {
    sendTrackingId(0xffffffff);
    injectEvent(EV_KEY, BTN_TOUCH, 0);
    injectEvent(EV_SYN, SYN_REPORT, 0);
}

void UinputTouchScreen::sendToolType(int32_t toolType) {
    injectEvent(EV_ABS, ABS_MT_TOOL_TYPE, toolType);
    injectEvent(EV_SYN, SYN_REPORT, 0);
}

// Get the center x, y base on the range definition.
const Point UinputTouchScreen::getCenterPoint() {
    return Point(mSize.left + mSize.width() / 2, mSize.top + mSize.height() / 2);
}

} // namespace android
