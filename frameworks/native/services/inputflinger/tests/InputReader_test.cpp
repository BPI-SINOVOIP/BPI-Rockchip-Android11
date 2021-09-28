/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include <CursorInputMapper.h>
#include <InputDevice.h>
#include <InputMapper.h>
#include <InputReader.h>
#include <InputReaderBase.h>
#include <InputReaderFactory.h>
#include <KeyboardInputMapper.h>
#include <MultiTouchInputMapper.h>
#include <SingleTouchInputMapper.h>
#include <SwitchInputMapper.h>
#include <TestInputListener.h>
#include <TouchInputMapper.h>
#include <UinputDevice.h>

#include <android-base/thread_annotations.h>
#include <gtest/gtest.h>
#include <inttypes.h>
#include <math.h>

namespace android {

using std::chrono_literals::operator""ms;

// Timeout for waiting for an expected event
static constexpr std::chrono::duration WAIT_TIMEOUT = 100ms;

// An arbitrary time value.
static const nsecs_t ARBITRARY_TIME = 1234;

// Arbitrary display properties.
static const int32_t DISPLAY_ID = 0;
static const int32_t SECONDARY_DISPLAY_ID = DISPLAY_ID + 1;
static const int32_t DISPLAY_WIDTH = 480;
static const int32_t DISPLAY_HEIGHT = 800;
static const int32_t VIRTUAL_DISPLAY_ID = 1;
static const int32_t VIRTUAL_DISPLAY_WIDTH = 400;
static const int32_t VIRTUAL_DISPLAY_HEIGHT = 500;
static const char* VIRTUAL_DISPLAY_UNIQUE_ID = "virtual:1";
static constexpr std::optional<uint8_t> NO_PORT = std::nullopt; // no physical port is specified

// Error tolerance for floating point assertions.
static const float EPSILON = 0.001f;

template<typename T>
static inline T min(T a, T b) {
    return a < b ? a : b;
}

static inline float avg(float x, float y) {
    return (x + y) / 2;
}


// --- FakePointerController ---

class FakePointerController : public PointerControllerInterface {
    bool mHaveBounds;
    float mMinX, mMinY, mMaxX, mMaxY;
    float mX, mY;
    int32_t mButtonState;
    int32_t mDisplayId;

protected:
    virtual ~FakePointerController() { }

public:
    FakePointerController() :
        mHaveBounds(false), mMinX(0), mMinY(0), mMaxX(0), mMaxY(0), mX(0), mY(0),
        mButtonState(0), mDisplayId(ADISPLAY_ID_DEFAULT) {
    }

    void setBounds(float minX, float minY, float maxX, float maxY) {
        mHaveBounds = true;
        mMinX = minX;
        mMinY = minY;
        mMaxX = maxX;
        mMaxY = maxY;
    }

    virtual void setPosition(float x, float y) {
        mX = x;
        mY = y;
    }

    virtual void setButtonState(int32_t buttonState) {
        mButtonState = buttonState;
    }

    virtual int32_t getButtonState() const {
        return mButtonState;
    }

    virtual void getPosition(float* outX, float* outY) const {
        *outX = mX;
        *outY = mY;
    }

    virtual int32_t getDisplayId() const {
        return mDisplayId;
    }

    virtual void setDisplayViewport(const DisplayViewport& viewport) {
        mDisplayId = viewport.displayId;
    }

    const std::map<int32_t, std::vector<int32_t>>& getSpots() {
        return mSpotsByDisplay;
    }

private:
    virtual bool getBounds(float* outMinX, float* outMinY, float* outMaxX, float* outMaxY) const {
        *outMinX = mMinX;
        *outMinY = mMinY;
        *outMaxX = mMaxX;
        *outMaxY = mMaxY;
        return mHaveBounds;
    }

    virtual void move(float deltaX, float deltaY) {
        mX += deltaX;
        if (mX < mMinX) mX = mMinX;
        if (mX > mMaxX) mX = mMaxX;
        mY += deltaY;
        if (mY < mMinY) mY = mMinY;
        if (mY > mMaxY) mY = mMaxY;
    }

    virtual void fade(Transition) {
    }

    virtual void unfade(Transition) {
    }

    virtual void setPresentation(Presentation) {
    }

    virtual void setSpots(const PointerCoords*, const uint32_t*, BitSet32 spotIdBits,
            int32_t displayId) {
        std::vector<int32_t> newSpots;
        // Add spots for fingers that are down.
        for (BitSet32 idBits(spotIdBits); !idBits.isEmpty(); ) {
            uint32_t id = idBits.clearFirstMarkedBit();
            newSpots.push_back(id);
        }

        mSpotsByDisplay[displayId] = newSpots;
    }

    virtual void clearSpots() {
    }

    std::map<int32_t, std::vector<int32_t>> mSpotsByDisplay;
};


// --- FakeInputReaderPolicy ---

class FakeInputReaderPolicy : public InputReaderPolicyInterface {
    std::mutex mLock;
    std::condition_variable mDevicesChangedCondition;

    InputReaderConfiguration mConfig;
    KeyedVector<int32_t, sp<FakePointerController> > mPointerControllers;
    std::vector<InputDeviceInfo> mInputDevices GUARDED_BY(mLock);
    bool mInputDevicesChanged GUARDED_BY(mLock){false};
    std::vector<DisplayViewport> mViewports;
    TouchAffineTransformation transform;

protected:
    virtual ~FakeInputReaderPolicy() { }

public:
    FakeInputReaderPolicy() {
    }

    void assertInputDevicesChanged() {
        waitForInputDevices([](bool devicesChanged) {
            if (!devicesChanged) {
                FAIL() << "Timed out waiting for notifyInputDevicesChanged() to be called.";
            }
        });
    }

    void assertInputDevicesNotChanged() {
        waitForInputDevices([](bool devicesChanged) {
            if (devicesChanged) {
                FAIL() << "Expected notifyInputDevicesChanged() to not be called.";
            }
        });
    }

    virtual void clearViewports() {
        mViewports.clear();
        mConfig.setDisplayViewports(mViewports);
    }

    std::optional<DisplayViewport> getDisplayViewportByUniqueId(const std::string& uniqueId) const {
        return mConfig.getDisplayViewportByUniqueId(uniqueId);
    }
    std::optional<DisplayViewport> getDisplayViewportByType(ViewportType type) const {
        return mConfig.getDisplayViewportByType(type);
    }

    std::optional<DisplayViewport> getDisplayViewportByPort(uint8_t displayPort) const {
        return mConfig.getDisplayViewportByPort(displayPort);
    }

    void addDisplayViewport(int32_t displayId, int32_t width, int32_t height, int32_t orientation,
            const std::string& uniqueId, std::optional<uint8_t> physicalPort,
            ViewportType viewportType) {
        const DisplayViewport viewport = createDisplayViewport(displayId, width, height,
                orientation, uniqueId, physicalPort, viewportType);
        mViewports.push_back(viewport);
        mConfig.setDisplayViewports(mViewports);
    }

    bool updateViewport(const DisplayViewport& viewport) {
        size_t count = mViewports.size();
        for (size_t i = 0; i < count; i++) {
            const DisplayViewport& currentViewport = mViewports[i];
            if (currentViewport.displayId == viewport.displayId) {
                mViewports[i] = viewport;
                mConfig.setDisplayViewports(mViewports);
                return true;
            }
        }
        // no viewport found.
        return false;
    }

    void addExcludedDeviceName(const std::string& deviceName) {
        mConfig.excludedDeviceNames.push_back(deviceName);
    }

    void addInputPortAssociation(const std::string& inputPort, uint8_t displayPort) {
        mConfig.portAssociations.insert({inputPort, displayPort});
    }

    void addDisabledDevice(int32_t deviceId) { mConfig.disabledDevices.insert(deviceId); }

    void removeDisabledDevice(int32_t deviceId) { mConfig.disabledDevices.erase(deviceId); }

    void setPointerController(int32_t deviceId, const sp<FakePointerController>& controller) {
        mPointerControllers.add(deviceId, controller);
    }

    const InputReaderConfiguration* getReaderConfiguration() const {
        return &mConfig;
    }

    const std::vector<InputDeviceInfo>& getInputDevices() const {
        return mInputDevices;
    }

    TouchAffineTransformation getTouchAffineTransformation(const std::string& inputDeviceDescriptor,
            int32_t surfaceRotation) {
        return transform;
    }

    int32_t notifyDisplayIdChanged(){
        return 0;
    }

    void setTouchAffineTransformation(const TouchAffineTransformation t) {
        transform = t;
    }

    void setPointerCapture(bool enabled) {
        mConfig.pointerCapture = enabled;
    }

    void setShowTouches(bool enabled) {
        mConfig.showTouches = enabled;
    }

    void setDefaultPointerDisplayId(int32_t pointerDisplayId) {
        mConfig.defaultPointerDisplayId = pointerDisplayId;
    }

private:
    DisplayViewport createDisplayViewport(int32_t displayId, int32_t width, int32_t height,
            int32_t orientation, const std::string& uniqueId, std::optional<uint8_t> physicalPort,
            ViewportType type) {
        bool isRotated = (orientation == DISPLAY_ORIENTATION_90
                || orientation == DISPLAY_ORIENTATION_270);
        DisplayViewport v;
        v.displayId = displayId;
        v.orientation = orientation;
        v.logicalLeft = 0;
        v.logicalTop = 0;
        v.logicalRight = isRotated ? height : width;
        v.logicalBottom = isRotated ? width : height;
        v.physicalLeft = 0;
        v.physicalTop = 0;
        v.physicalRight = isRotated ? height : width;
        v.physicalBottom = isRotated ? width : height;
        v.deviceWidth = isRotated ? height : width;
        v.deviceHeight = isRotated ? width : height;
        v.uniqueId = uniqueId;
        v.physicalPort = physicalPort;
        v.type = type;
        return v;
    }

    virtual void getReaderConfiguration(InputReaderConfiguration* outConfig) {
        *outConfig = mConfig;
    }

    virtual sp<PointerControllerInterface> obtainPointerController(int32_t deviceId) {
        return mPointerControllers.valueFor(deviceId);
    }

    virtual void notifyInputDevicesChanged(const std::vector<InputDeviceInfo>& inputDevices) {
        std::scoped_lock<std::mutex> lock(mLock);
        mInputDevices = inputDevices;
        mInputDevicesChanged = true;
        mDevicesChangedCondition.notify_all();
    }

    virtual sp<KeyCharacterMap> getKeyboardLayoutOverlay(const InputDeviceIdentifier&) {
        return nullptr;
    }

    virtual std::string getDeviceAlias(const InputDeviceIdentifier&) {
        return "";
    }

    void waitForInputDevices(std::function<void(bool)> processDevicesChanged) {
        std::unique_lock<std::mutex> lock(mLock);
        base::ScopedLockAssertion assumeLocked(mLock);

        const bool devicesChanged =
                mDevicesChangedCondition.wait_for(lock, WAIT_TIMEOUT, [this]() REQUIRES(mLock) {
                    return mInputDevicesChanged;
                });
        ASSERT_NO_FATAL_FAILURE(processDevicesChanged(devicesChanged));
        mInputDevicesChanged = false;
    }
};

// --- FakeEventHub ---

class FakeEventHub : public EventHubInterface {
    struct KeyInfo {
        int32_t keyCode;
        uint32_t flags;
    };

    struct Device {
        InputDeviceIdentifier identifier;
        uint32_t classes;
        PropertyMap configuration;
        KeyedVector<int, RawAbsoluteAxisInfo> absoluteAxes;
        KeyedVector<int, bool> relativeAxes;
        KeyedVector<int32_t, int32_t> keyCodeStates;
        KeyedVector<int32_t, int32_t> scanCodeStates;
        KeyedVector<int32_t, int32_t> switchStates;
        KeyedVector<int32_t, int32_t> absoluteAxisValue;
        KeyedVector<int32_t, KeyInfo> keysByScanCode;
        KeyedVector<int32_t, KeyInfo> keysByUsageCode;
        KeyedVector<int32_t, bool> leds;
        std::vector<VirtualKeyDefinition> virtualKeys;
        bool enabled;

        status_t enable() {
            enabled = true;
            return OK;
        }

        status_t disable() {
            enabled = false;
            return OK;
        }

        explicit Device(uint32_t classes) :
                classes(classes), enabled(true) {
        }
    };

    std::mutex mLock;
    std::condition_variable mEventsCondition;

    KeyedVector<int32_t, Device*> mDevices;
    std::vector<std::string> mExcludedDevices;
    List<RawEvent> mEvents GUARDED_BY(mLock);
    std::unordered_map<int32_t /*deviceId*/, std::vector<TouchVideoFrame>> mVideoFrames;

public:
    virtual ~FakeEventHub() {
        for (size_t i = 0; i < mDevices.size(); i++) {
            delete mDevices.valueAt(i);
        }
    }

    FakeEventHub() { }

    void addDevice(int32_t deviceId, const std::string& name, uint32_t classes) {
        Device* device = new Device(classes);
        device->identifier.name = name;
        mDevices.add(deviceId, device);

        enqueueEvent(ARBITRARY_TIME, deviceId, EventHubInterface::DEVICE_ADDED, 0, 0);
    }

    void removeDevice(int32_t deviceId) {
        delete mDevices.valueFor(deviceId);
        mDevices.removeItem(deviceId);

        enqueueEvent(ARBITRARY_TIME, deviceId, EventHubInterface::DEVICE_REMOVED, 0, 0);
    }

    bool isDeviceEnabled(int32_t deviceId) {
        Device* device = getDevice(deviceId);
        if (device == nullptr) {
            ALOGE("Incorrect device id=%" PRId32 " provided to %s", deviceId, __func__);
            return false;
        }
        return device->enabled;
    }

    status_t enableDevice(int32_t deviceId) {
        status_t result;
        Device* device = getDevice(deviceId);
        if (device == nullptr) {
            ALOGE("Incorrect device id=%" PRId32 " provided to %s", deviceId, __func__);
            return BAD_VALUE;
        }
        if (device->enabled) {
            ALOGW("Duplicate call to %s, device %" PRId32 " already enabled", __func__, deviceId);
            return OK;
        }
        result = device->enable();
        return result;
    }

    status_t disableDevice(int32_t deviceId) {
        Device* device = getDevice(deviceId);
        if (device == nullptr) {
            ALOGE("Incorrect device id=%" PRId32 " provided to %s", deviceId, __func__);
            return BAD_VALUE;
        }
        if (!device->enabled) {
            ALOGW("Duplicate call to %s, device %" PRId32 " already disabled", __func__, deviceId);
            return OK;
        }
        return device->disable();
    }

    void finishDeviceScan() {
        enqueueEvent(ARBITRARY_TIME, 0, EventHubInterface::FINISHED_DEVICE_SCAN, 0, 0);
    }

    void addConfigurationProperty(int32_t deviceId, const String8& key, const String8& value) {
        Device* device = getDevice(deviceId);
        device->configuration.addProperty(key, value);
    }

    void addConfigurationMap(int32_t deviceId, const PropertyMap* configuration) {
        Device* device = getDevice(deviceId);
        device->configuration.addAll(configuration);
    }

    void addAbsoluteAxis(int32_t deviceId, int axis,
            int32_t minValue, int32_t maxValue, int flat, int fuzz, int resolution = 0) {
        Device* device = getDevice(deviceId);

        RawAbsoluteAxisInfo info;
        info.valid = true;
        info.minValue = minValue;
        info.maxValue = maxValue;
        info.flat = flat;
        info.fuzz = fuzz;
        info.resolution = resolution;
        device->absoluteAxes.add(axis, info);
    }

    void addRelativeAxis(int32_t deviceId, int32_t axis) {
        Device* device = getDevice(deviceId);
        device->relativeAxes.add(axis, true);
    }

    void setKeyCodeState(int32_t deviceId, int32_t keyCode, int32_t state) {
        Device* device = getDevice(deviceId);
        device->keyCodeStates.replaceValueFor(keyCode, state);
    }

    void setScanCodeState(int32_t deviceId, int32_t scanCode, int32_t state) {
        Device* device = getDevice(deviceId);
        device->scanCodeStates.replaceValueFor(scanCode, state);
    }

    void setSwitchState(int32_t deviceId, int32_t switchCode, int32_t state) {
        Device* device = getDevice(deviceId);
        device->switchStates.replaceValueFor(switchCode, state);
    }

    void setAbsoluteAxisValue(int32_t deviceId, int32_t axis, int32_t value) {
        Device* device = getDevice(deviceId);
        device->absoluteAxisValue.replaceValueFor(axis, value);
    }

    void addKey(int32_t deviceId, int32_t scanCode, int32_t usageCode,
            int32_t keyCode, uint32_t flags) {
        Device* device = getDevice(deviceId);
        KeyInfo info;
        info.keyCode = keyCode;
        info.flags = flags;
        if (scanCode) {
            device->keysByScanCode.add(scanCode, info);
        }
        if (usageCode) {
            device->keysByUsageCode.add(usageCode, info);
        }
    }

    void addLed(int32_t deviceId, int32_t led, bool initialState) {
        Device* device = getDevice(deviceId);
        device->leds.add(led, initialState);
    }

    bool getLedState(int32_t deviceId, int32_t led) {
        Device* device = getDevice(deviceId);
        return device->leds.valueFor(led);
    }

    std::vector<std::string>& getExcludedDevices() {
        return mExcludedDevices;
    }

    void addVirtualKeyDefinition(int32_t deviceId, const VirtualKeyDefinition& definition) {
        Device* device = getDevice(deviceId);
        device->virtualKeys.push_back(definition);
    }

    void enqueueEvent(nsecs_t when, int32_t deviceId, int32_t type,
            int32_t code, int32_t value) {
        std::scoped_lock<std::mutex> lock(mLock);
        RawEvent event;
        event.when = when;
        event.deviceId = deviceId;
        event.type = type;
        event.code = code;
        event.value = value;
        mEvents.push_back(event);

        if (type == EV_ABS) {
            setAbsoluteAxisValue(deviceId, code, value);
        }
    }

    void setVideoFrames(std::unordered_map<int32_t /*deviceId*/,
            std::vector<TouchVideoFrame>> videoFrames) {
        mVideoFrames = std::move(videoFrames);
    }

    void assertQueueIsEmpty() {
        std::unique_lock<std::mutex> lock(mLock);
        base::ScopedLockAssertion assumeLocked(mLock);
        const bool queueIsEmpty =
                mEventsCondition.wait_for(lock, WAIT_TIMEOUT,
                                          [this]() REQUIRES(mLock) { return mEvents.size() == 0; });
        if (!queueIsEmpty) {
            FAIL() << "Timed out waiting for EventHub queue to be emptied.";
        }
    }

private:
    Device* getDevice(int32_t deviceId) const {
        ssize_t index = mDevices.indexOfKey(deviceId);
        return index >= 0 ? mDevices.valueAt(index) : nullptr;
    }

    virtual uint32_t getDeviceClasses(int32_t deviceId) const {
        Device* device = getDevice(deviceId);
        return device ? device->classes : 0;
    }

    virtual InputDeviceIdentifier getDeviceIdentifier(int32_t deviceId) const {
        Device* device = getDevice(deviceId);
        return device ? device->identifier : InputDeviceIdentifier();
    }

    virtual int32_t getDeviceControllerNumber(int32_t) const {
        return 0;
    }

    virtual void getConfiguration(int32_t deviceId, PropertyMap* outConfiguration) const {
        Device* device = getDevice(deviceId);
        if (device) {
            *outConfiguration = device->configuration;
        }
    }

    virtual status_t getAbsoluteAxisInfo(int32_t deviceId, int axis,
            RawAbsoluteAxisInfo* outAxisInfo) const {
        Device* device = getDevice(deviceId);
        if (device && device->enabled) {
            ssize_t index = device->absoluteAxes.indexOfKey(axis);
            if (index >= 0) {
                *outAxisInfo = device->absoluteAxes.valueAt(index);
                return OK;
            }
        }
        outAxisInfo->clear();
        return -1;
    }

    virtual bool hasRelativeAxis(int32_t deviceId, int axis) const {
        Device* device = getDevice(deviceId);
        if (device) {
            return device->relativeAxes.indexOfKey(axis) >= 0;
        }
        return false;
    }

    virtual bool hasInputProperty(int32_t, int) const {
        return false;
    }

    virtual status_t mapKey(int32_t deviceId,
            int32_t scanCode, int32_t usageCode, int32_t metaState,
            int32_t* outKeycode, int32_t *outMetaState, uint32_t* outFlags) const {
        Device* device = getDevice(deviceId);
        if (device) {
            const KeyInfo* key = getKey(device, scanCode, usageCode);
            if (key) {
                if (outKeycode) {
                    *outKeycode = key->keyCode;
                }
                if (outFlags) {
                    *outFlags = key->flags;
                }
                if (outMetaState) {
                    *outMetaState = metaState;
                }
                return OK;
            }
        }
        return NAME_NOT_FOUND;
    }

    const KeyInfo* getKey(Device* device, int32_t scanCode, int32_t usageCode) const {
        if (usageCode) {
            ssize_t index = device->keysByUsageCode.indexOfKey(usageCode);
            if (index >= 0) {
                return &device->keysByUsageCode.valueAt(index);
            }
        }
        if (scanCode) {
            ssize_t index = device->keysByScanCode.indexOfKey(scanCode);
            if (index >= 0) {
                return &device->keysByScanCode.valueAt(index);
            }
        }
        return nullptr;
    }

    virtual status_t mapAxis(int32_t, int32_t, AxisInfo*) const {
        return NAME_NOT_FOUND;
    }

    virtual void setExcludedDevices(const std::vector<std::string>& devices) {
        mExcludedDevices = devices;
    }

    virtual size_t getEvents(int, RawEvent* buffer, size_t) {
        std::scoped_lock<std::mutex> lock(mLock);
        if (mEvents.empty()) {
            return 0;
        }

        *buffer = *mEvents.begin();
        mEvents.erase(mEvents.begin());
        mEventsCondition.notify_all();
        return 1;
    }

    virtual std::vector<TouchVideoFrame> getVideoFrames(int32_t deviceId) {
        auto it = mVideoFrames.find(deviceId);
        if (it != mVideoFrames.end()) {
            std::vector<TouchVideoFrame> frames = std::move(it->second);
            mVideoFrames.erase(deviceId);
            return frames;
        }
        return {};
    }

    virtual int32_t getScanCodeState(int32_t deviceId, int32_t scanCode) const {
        Device* device = getDevice(deviceId);
        if (device) {
            ssize_t index = device->scanCodeStates.indexOfKey(scanCode);
            if (index >= 0) {
                return device->scanCodeStates.valueAt(index);
            }
        }
        return AKEY_STATE_UNKNOWN;
    }

    virtual int32_t getKeyCodeState(int32_t deviceId, int32_t keyCode) const {
        Device* device = getDevice(deviceId);
        if (device) {
            ssize_t index = device->keyCodeStates.indexOfKey(keyCode);
            if (index >= 0) {
                return device->keyCodeStates.valueAt(index);
            }
        }
        return AKEY_STATE_UNKNOWN;
    }

    virtual int32_t getSwitchState(int32_t deviceId, int32_t sw) const {
        Device* device = getDevice(deviceId);
        if (device) {
            ssize_t index = device->switchStates.indexOfKey(sw);
            if (index >= 0) {
                return device->switchStates.valueAt(index);
            }
        }
        return AKEY_STATE_UNKNOWN;
    }

    virtual status_t getAbsoluteAxisValue(int32_t deviceId, int32_t axis,
            int32_t* outValue) const {
        Device* device = getDevice(deviceId);
        if (device) {
            ssize_t index = device->absoluteAxisValue.indexOfKey(axis);
            if (index >= 0) {
                *outValue = device->absoluteAxisValue.valueAt(index);
                return OK;
            }
        }
        *outValue = 0;
        return -1;
    }

    virtual bool markSupportedKeyCodes(int32_t deviceId, size_t numCodes, const int32_t* keyCodes,
            uint8_t* outFlags) const {
        bool result = false;
        Device* device = getDevice(deviceId);
        if (device) {
            for (size_t i = 0; i < numCodes; i++) {
                for (size_t j = 0; j < device->keysByScanCode.size(); j++) {
                    if (keyCodes[i] == device->keysByScanCode.valueAt(j).keyCode) {
                        outFlags[i] = 1;
                        result = true;
                    }
                }
                for (size_t j = 0; j < device->keysByUsageCode.size(); j++) {
                    if (keyCodes[i] == device->keysByUsageCode.valueAt(j).keyCode) {
                        outFlags[i] = 1;
                        result = true;
                    }
                }
            }
        }
        return result;
    }

    virtual bool hasScanCode(int32_t deviceId, int32_t scanCode) const {
        Device* device = getDevice(deviceId);
        if (device) {
            ssize_t index = device->keysByScanCode.indexOfKey(scanCode);
            return index >= 0;
        }
        return false;
    }

    virtual bool hasLed(int32_t deviceId, int32_t led) const {
        Device* device = getDevice(deviceId);
        return device && device->leds.indexOfKey(led) >= 0;
    }

    virtual void setLedState(int32_t deviceId, int32_t led, bool on) {
        Device* device = getDevice(deviceId);
        if (device) {
            ssize_t index = device->leds.indexOfKey(led);
            if (index >= 0) {
                device->leds.replaceValueAt(led, on);
            } else {
                ADD_FAILURE()
                        << "Attempted to set the state of an LED that the EventHub declared "
                        "was not present.  led=" << led;
            }
        }
    }

    virtual void getVirtualKeyDefinitions(int32_t deviceId,
            std::vector<VirtualKeyDefinition>& outVirtualKeys) const {
        outVirtualKeys.clear();

        Device* device = getDevice(deviceId);
        if (device) {
            outVirtualKeys = device->virtualKeys;
        }
    }

    virtual sp<KeyCharacterMap> getKeyCharacterMap(int32_t) const {
        return nullptr;
    }

    virtual bool setKeyboardLayoutOverlay(int32_t, const sp<KeyCharacterMap>&) {
        return false;
    }

    virtual void vibrate(int32_t, nsecs_t) {
    }

    virtual void cancelVibrate(int32_t) {
    }

    virtual bool isExternal(int32_t) const {
        return false;
    }

    virtual void dump(std::string&) {
    }

    virtual void monitor() {
    }

    virtual void requestReopenDevices() {
    }

    virtual void wake() {
    }
};


// --- FakeInputReaderContext ---

class FakeInputReaderContext : public InputReaderContext {
    std::shared_ptr<EventHubInterface> mEventHub;
    sp<InputReaderPolicyInterface> mPolicy;
    sp<InputListenerInterface> mListener;
    int32_t mGlobalMetaState;
    bool mUpdateGlobalMetaStateWasCalled;
    int32_t mGeneration;
    int32_t mNextId;
    wp<PointerControllerInterface> mPointerController;

public:
    FakeInputReaderContext(std::shared_ptr<EventHubInterface> eventHub,
                           const sp<InputReaderPolicyInterface>& policy,
                           const sp<InputListenerInterface>& listener)
          : mEventHub(eventHub),
            mPolicy(policy),
            mListener(listener),
            mGlobalMetaState(0),
            mNextId(1) {}

    virtual ~FakeInputReaderContext() { }

    void assertUpdateGlobalMetaStateWasCalled() {
        ASSERT_TRUE(mUpdateGlobalMetaStateWasCalled)
                << "Expected updateGlobalMetaState() to have been called.";
        mUpdateGlobalMetaStateWasCalled = false;
    }

    void setGlobalMetaState(int32_t state) {
        mGlobalMetaState = state;
    }

    uint32_t getGeneration() {
        return mGeneration;
    }

    void updatePointerDisplay() {
        sp<PointerControllerInterface> controller = mPointerController.promote();
        if (controller != nullptr) {
            InputReaderConfiguration config;
            mPolicy->getReaderConfiguration(&config);
            auto viewport = config.getDisplayViewportById(config.defaultPointerDisplayId);
            if (viewport) {
                controller->setDisplayViewport(*viewport);
            }
        }
    }

private:
    virtual void updateGlobalMetaState() {
        mUpdateGlobalMetaStateWasCalled = true;
    }

    virtual int32_t getGlobalMetaState() {
        return mGlobalMetaState;
    }

    virtual EventHubInterface* getEventHub() {
        return mEventHub.get();
    }

    virtual InputReaderPolicyInterface* getPolicy() {
        return mPolicy.get();
    }

    virtual InputListenerInterface* getListener() {
        return mListener.get();
    }

    virtual void disableVirtualKeysUntil(nsecs_t) {
    }

    virtual bool shouldDropVirtualKey(nsecs_t, int32_t, int32_t) { return false; }

    virtual sp<PointerControllerInterface> getPointerController(int32_t deviceId) {
        sp<PointerControllerInterface> controller = mPointerController.promote();
        if (controller == nullptr) {
            controller = mPolicy->obtainPointerController(deviceId);
            mPointerController = controller;
            updatePointerDisplay();
        }
        return controller;
    }

    virtual void fadePointer() {
    }

    virtual void requestTimeoutAtTime(nsecs_t) {
    }

    virtual int32_t bumpGeneration() {
        return ++mGeneration;
    }

    virtual void getExternalStylusDevices(std::vector<InputDeviceInfo>& outDevices) {

    }

    virtual void dispatchExternalStylusState(const StylusState&) {

    }

    virtual int32_t getNextId() { return mNextId++; }
};


// --- FakeInputMapper ---

class FakeInputMapper : public InputMapper {
    uint32_t mSources;
    int32_t mKeyboardType;
    int32_t mMetaState;
    KeyedVector<int32_t, int32_t> mKeyCodeStates;
    KeyedVector<int32_t, int32_t> mScanCodeStates;
    KeyedVector<int32_t, int32_t> mSwitchStates;
    std::vector<int32_t> mSupportedKeyCodes;

    std::mutex mLock;
    std::condition_variable mStateChangedCondition;
    bool mConfigureWasCalled GUARDED_BY(mLock);
    bool mResetWasCalled GUARDED_BY(mLock);
    bool mProcessWasCalled GUARDED_BY(mLock);
    RawEvent mLastEvent GUARDED_BY(mLock);

    std::optional<DisplayViewport> mViewport;
public:
    FakeInputMapper(InputDeviceContext& deviceContext, uint32_t sources)
          : InputMapper(deviceContext),
            mSources(sources),
            mKeyboardType(AINPUT_KEYBOARD_TYPE_NONE),
            mMetaState(0),
            mConfigureWasCalled(false),
            mResetWasCalled(false),
            mProcessWasCalled(false) {}

    virtual ~FakeInputMapper() { }

    void setKeyboardType(int32_t keyboardType) {
        mKeyboardType = keyboardType;
    }

    void setMetaState(int32_t metaState) {
        mMetaState = metaState;
    }

    void assertConfigureWasCalled() {
        std::unique_lock<std::mutex> lock(mLock);
        base::ScopedLockAssertion assumeLocked(mLock);
        const bool configureCalled =
                mStateChangedCondition.wait_for(lock, WAIT_TIMEOUT, [this]() REQUIRES(mLock) {
                    return mConfigureWasCalled;
                });
        if (!configureCalled) {
            FAIL() << "Expected configure() to have been called.";
        }
        mConfigureWasCalled = false;
    }

    void assertResetWasCalled() {
        std::unique_lock<std::mutex> lock(mLock);
        base::ScopedLockAssertion assumeLocked(mLock);
        const bool resetCalled =
                mStateChangedCondition.wait_for(lock, WAIT_TIMEOUT, [this]() REQUIRES(mLock) {
                    return mResetWasCalled;
                });
        if (!resetCalled) {
            FAIL() << "Expected reset() to have been called.";
        }
        mResetWasCalled = false;
    }

    void assertProcessWasCalled(RawEvent* outLastEvent = nullptr) {
        std::unique_lock<std::mutex> lock(mLock);
        base::ScopedLockAssertion assumeLocked(mLock);
        const bool processCalled =
                mStateChangedCondition.wait_for(lock, WAIT_TIMEOUT, [this]() REQUIRES(mLock) {
                    return mProcessWasCalled;
                });
        if (!processCalled) {
            FAIL() << "Expected process() to have been called.";
        }
        if (outLastEvent) {
            *outLastEvent = mLastEvent;
        }
        mProcessWasCalled = false;
    }

    void setKeyCodeState(int32_t keyCode, int32_t state) {
        mKeyCodeStates.replaceValueFor(keyCode, state);
    }

    void setScanCodeState(int32_t scanCode, int32_t state) {
        mScanCodeStates.replaceValueFor(scanCode, state);
    }

    void setSwitchState(int32_t switchCode, int32_t state) {
        mSwitchStates.replaceValueFor(switchCode, state);
    }

    void addSupportedKeyCode(int32_t keyCode) {
        mSupportedKeyCodes.push_back(keyCode);
    }

private:
    virtual uint32_t getSources() {
        return mSources;
    }

    virtual void populateDeviceInfo(InputDeviceInfo* deviceInfo) {
        InputMapper::populateDeviceInfo(deviceInfo);

        if (mKeyboardType != AINPUT_KEYBOARD_TYPE_NONE) {
            deviceInfo->setKeyboardType(mKeyboardType);
        }
    }

    virtual void configure(nsecs_t, const InputReaderConfiguration* config, uint32_t changes) {
        std::scoped_lock<std::mutex> lock(mLock);
        mConfigureWasCalled = true;

        // Find the associated viewport if exist.
        const std::optional<uint8_t> displayPort = getDeviceContext().getAssociatedDisplayPort();
        if (displayPort && (changes & InputReaderConfiguration::CHANGE_DISPLAY_INFO)) {
            mViewport = config->getDisplayViewportByPort(*displayPort);
        }

        mStateChangedCondition.notify_all();
    }

    virtual void reset(nsecs_t) {
        std::scoped_lock<std::mutex> lock(mLock);
        mResetWasCalled = true;
        mStateChangedCondition.notify_all();
    }

    virtual void process(const RawEvent* rawEvent) {
        std::scoped_lock<std::mutex> lock(mLock);
        mLastEvent = *rawEvent;
        mProcessWasCalled = true;
        mStateChangedCondition.notify_all();
    }

    virtual int32_t getKeyCodeState(uint32_t, int32_t keyCode) {
        ssize_t index = mKeyCodeStates.indexOfKey(keyCode);
        return index >= 0 ? mKeyCodeStates.valueAt(index) : AKEY_STATE_UNKNOWN;
    }

    virtual int32_t getScanCodeState(uint32_t, int32_t scanCode) {
        ssize_t index = mScanCodeStates.indexOfKey(scanCode);
        return index >= 0 ? mScanCodeStates.valueAt(index) : AKEY_STATE_UNKNOWN;
    }

    virtual int32_t getSwitchState(uint32_t, int32_t switchCode) {
        ssize_t index = mSwitchStates.indexOfKey(switchCode);
        return index >= 0 ? mSwitchStates.valueAt(index) : AKEY_STATE_UNKNOWN;
    }

    virtual bool markSupportedKeyCodes(uint32_t, size_t numCodes,
            const int32_t* keyCodes, uint8_t* outFlags) {
        bool result = false;
        for (size_t i = 0; i < numCodes; i++) {
            for (size_t j = 0; j < mSupportedKeyCodes.size(); j++) {
                if (keyCodes[i] == mSupportedKeyCodes[j]) {
                    outFlags[i] = 1;
                    result = true;
                }
            }
        }
        return result;
    }

    virtual int32_t getMetaState() {
        return mMetaState;
    }

    virtual void fadePointer() {
    }

    virtual std::optional<int32_t> getAssociatedDisplay() {
        if (mViewport) {
            return std::make_optional(mViewport->displayId);
        }
        return std::nullopt;
    }
};


// --- InstrumentedInputReader ---

class InstrumentedInputReader : public InputReader {
    std::shared_ptr<InputDevice> mNextDevice;

public:
    InstrumentedInputReader(std::shared_ptr<EventHubInterface> eventHub,
                            const sp<InputReaderPolicyInterface>& policy,
                            const sp<InputListenerInterface>& listener)
          : InputReader(eventHub, policy, listener), mNextDevice(nullptr) {}

    virtual ~InstrumentedInputReader() {}

    void setNextDevice(std::shared_ptr<InputDevice> device) { mNextDevice = device; }

    std::shared_ptr<InputDevice> newDevice(int32_t deviceId, const std::string& name,
                                           const std::string& location = "") {
        InputDeviceIdentifier identifier;
        identifier.name = name;
        identifier.location = location;
        int32_t generation = deviceId + 1;
        return std::make_shared<InputDevice>(&mContext, deviceId, generation, identifier);
    }

    // Make the protected loopOnce method accessible to tests.
    using InputReader::loopOnce;

protected:
    virtual std::shared_ptr<InputDevice> createDeviceLocked(
            int32_t eventHubId, const InputDeviceIdentifier& identifier) {
        if (mNextDevice) {
            std::shared_ptr<InputDevice> device(mNextDevice);
            mNextDevice = nullptr;
            return device;
        }
        return InputReader::createDeviceLocked(eventHubId, identifier);
    }

    friend class InputReaderTest;
};

// --- InputReaderPolicyTest ---
class InputReaderPolicyTest : public testing::Test {
protected:
    sp<FakeInputReaderPolicy> mFakePolicy;

    virtual void SetUp() override { mFakePolicy = new FakeInputReaderPolicy(); }
    virtual void TearDown() override { mFakePolicy.clear(); }
};

/**
 * Check that empty set of viewports is an acceptable configuration.
 * Also try to get internal viewport two different ways - by type and by uniqueId.
 *
 * There will be confusion if two viewports with empty uniqueId and identical type are present.
 * Such configuration is not currently allowed.
 */
TEST_F(InputReaderPolicyTest, Viewports_GetCleared) {
    static const std::string uniqueId = "local:0";

    // We didn't add any viewports yet, so there shouldn't be any.
    std::optional<DisplayViewport> internalViewport =
            mFakePolicy->getDisplayViewportByType(ViewportType::VIEWPORT_INTERNAL);
    ASSERT_FALSE(internalViewport);

    // Add an internal viewport, then clear it
    mFakePolicy->addDisplayViewport(DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, uniqueId, NO_PORT, ViewportType::VIEWPORT_INTERNAL);

    // Check matching by uniqueId
    internalViewport = mFakePolicy->getDisplayViewportByUniqueId(uniqueId);
    ASSERT_TRUE(internalViewport);
    ASSERT_EQ(ViewportType::VIEWPORT_INTERNAL, internalViewport->type);

    // Check matching by viewport type
    internalViewport = mFakePolicy->getDisplayViewportByType(ViewportType::VIEWPORT_INTERNAL);
    ASSERT_TRUE(internalViewport);
    ASSERT_EQ(uniqueId, internalViewport->uniqueId);

    mFakePolicy->clearViewports();
    // Make sure nothing is found after clear
    internalViewport = mFakePolicy->getDisplayViewportByUniqueId(uniqueId);
    ASSERT_FALSE(internalViewport);
    internalViewport = mFakePolicy->getDisplayViewportByType(ViewportType::VIEWPORT_INTERNAL);
    ASSERT_FALSE(internalViewport);
}

TEST_F(InputReaderPolicyTest, Viewports_GetByType) {
    const std::string internalUniqueId = "local:0";
    const std::string externalUniqueId = "local:1";
    const std::string virtualUniqueId1 = "virtual:2";
    const std::string virtualUniqueId2 = "virtual:3";
    constexpr int32_t virtualDisplayId1 = 2;
    constexpr int32_t virtualDisplayId2 = 3;

    // Add an internal viewport
    mFakePolicy->addDisplayViewport(DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, internalUniqueId, NO_PORT, ViewportType::VIEWPORT_INTERNAL);
    // Add an external viewport
    mFakePolicy->addDisplayViewport(DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, externalUniqueId, NO_PORT, ViewportType::VIEWPORT_EXTERNAL);
    // Add an virtual viewport
    mFakePolicy->addDisplayViewport(virtualDisplayId1, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, virtualUniqueId1, NO_PORT, ViewportType::VIEWPORT_VIRTUAL);
    // Add another virtual viewport
    mFakePolicy->addDisplayViewport(virtualDisplayId2, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, virtualUniqueId2, NO_PORT, ViewportType::VIEWPORT_VIRTUAL);

    // Check matching by type for internal
    std::optional<DisplayViewport> internalViewport =
            mFakePolicy->getDisplayViewportByType(ViewportType::VIEWPORT_INTERNAL);
    ASSERT_TRUE(internalViewport);
    ASSERT_EQ(internalUniqueId, internalViewport->uniqueId);

    // Check matching by type for external
    std::optional<DisplayViewport> externalViewport =
            mFakePolicy->getDisplayViewportByType(ViewportType::VIEWPORT_EXTERNAL);
    ASSERT_TRUE(externalViewport);
    ASSERT_EQ(externalUniqueId, externalViewport->uniqueId);

    // Check matching by uniqueId for virtual viewport #1
    std::optional<DisplayViewport> virtualViewport1 =
            mFakePolicy->getDisplayViewportByUniqueId(virtualUniqueId1);
    ASSERT_TRUE(virtualViewport1);
    ASSERT_EQ(ViewportType::VIEWPORT_VIRTUAL, virtualViewport1->type);
    ASSERT_EQ(virtualUniqueId1, virtualViewport1->uniqueId);
    ASSERT_EQ(virtualDisplayId1, virtualViewport1->displayId);

    // Check matching by uniqueId for virtual viewport #2
    std::optional<DisplayViewport> virtualViewport2 =
            mFakePolicy->getDisplayViewportByUniqueId(virtualUniqueId2);
    ASSERT_TRUE(virtualViewport2);
    ASSERT_EQ(ViewportType::VIEWPORT_VIRTUAL, virtualViewport2->type);
    ASSERT_EQ(virtualUniqueId2, virtualViewport2->uniqueId);
    ASSERT_EQ(virtualDisplayId2, virtualViewport2->displayId);
}


/**
 * We can have 2 viewports of the same kind. We can distinguish them by uniqueId, and confirm
 * that lookup works by checking display id.
 * Check that 2 viewports of each kind is possible, for all existing viewport types.
 */
TEST_F(InputReaderPolicyTest, Viewports_TwoOfSameType) {
    const std::string uniqueId1 = "uniqueId1";
    const std::string uniqueId2 = "uniqueId2";
    constexpr int32_t displayId1 = 2;
    constexpr int32_t displayId2 = 3;

    std::vector<ViewportType> types = {ViewportType::VIEWPORT_INTERNAL,
            ViewportType::VIEWPORT_EXTERNAL, ViewportType::VIEWPORT_VIRTUAL};
    for (const ViewportType& type : types) {
        mFakePolicy->clearViewports();
        // Add a viewport
        mFakePolicy->addDisplayViewport(displayId1, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, uniqueId1, NO_PORT, type);
        // Add another viewport
        mFakePolicy->addDisplayViewport(displayId2, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, uniqueId2, NO_PORT, type);

        // Check that correct display viewport was returned by comparing the display IDs.
        std::optional<DisplayViewport> viewport1 =
                mFakePolicy->getDisplayViewportByUniqueId(uniqueId1);
        ASSERT_TRUE(viewport1);
        ASSERT_EQ(displayId1, viewport1->displayId);
        ASSERT_EQ(type, viewport1->type);

        std::optional<DisplayViewport> viewport2 =
                mFakePolicy->getDisplayViewportByUniqueId(uniqueId2);
        ASSERT_TRUE(viewport2);
        ASSERT_EQ(displayId2, viewport2->displayId);
        ASSERT_EQ(type, viewport2->type);

        // When there are multiple viewports of the same kind, and uniqueId is not specified
        // in the call to getDisplayViewport, then that situation is not supported.
        // The viewports can be stored in any order, so we cannot rely on the order, since that
        // is just implementation detail.
        // However, we can check that it still returns *a* viewport, we just cannot assert
        // which one specifically is returned.
        std::optional<DisplayViewport> someViewport = mFakePolicy->getDisplayViewportByType(type);
        ASSERT_TRUE(someViewport);
    }
}

/**
 * Check getDisplayViewportByPort
 */
TEST_F(InputReaderPolicyTest, Viewports_GetByPort) {
    constexpr ViewportType type = ViewportType::VIEWPORT_EXTERNAL;
    const std::string uniqueId1 = "uniqueId1";
    const std::string uniqueId2 = "uniqueId2";
    constexpr int32_t displayId1 = 1;
    constexpr int32_t displayId2 = 2;
    const uint8_t hdmi1 = 0;
    const uint8_t hdmi2 = 1;
    const uint8_t hdmi3 = 2;

    mFakePolicy->clearViewports();
    // Add a viewport that's associated with some display port that's not of interest.
    mFakePolicy->addDisplayViewport(displayId1, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, uniqueId1, hdmi3, type);
    // Add another viewport, connected to HDMI1 port
    mFakePolicy->addDisplayViewport(displayId2, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, uniqueId2, hdmi1, type);

    // Check that correct display viewport was returned by comparing the display ports.
    std::optional<DisplayViewport> hdmi1Viewport = mFakePolicy->getDisplayViewportByPort(hdmi1);
    ASSERT_TRUE(hdmi1Viewport);
    ASSERT_EQ(displayId2, hdmi1Viewport->displayId);
    ASSERT_EQ(uniqueId2, hdmi1Viewport->uniqueId);

    // Check that we can still get the same viewport using the uniqueId
    hdmi1Viewport = mFakePolicy->getDisplayViewportByUniqueId(uniqueId2);
    ASSERT_TRUE(hdmi1Viewport);
    ASSERT_EQ(displayId2, hdmi1Viewport->displayId);
    ASSERT_EQ(uniqueId2, hdmi1Viewport->uniqueId);
    ASSERT_EQ(type, hdmi1Viewport->type);

    // Check that we cannot find a port with "HDMI2", because we never added one
    std::optional<DisplayViewport> hdmi2Viewport = mFakePolicy->getDisplayViewportByPort(hdmi2);
    ASSERT_FALSE(hdmi2Viewport);
}

// --- InputReaderTest ---

class InputReaderTest : public testing::Test {
protected:
    sp<TestInputListener> mFakeListener;
    sp<FakeInputReaderPolicy> mFakePolicy;
    std::shared_ptr<FakeEventHub> mFakeEventHub;
    std::unique_ptr<InstrumentedInputReader> mReader;

    virtual void SetUp() override {
        mFakeEventHub = std::make_unique<FakeEventHub>();
        mFakePolicy = new FakeInputReaderPolicy();
        mFakeListener = new TestInputListener();

        mReader = std::make_unique<InstrumentedInputReader>(mFakeEventHub, mFakePolicy,
                                                            mFakeListener);
    }

    virtual void TearDown() override {
        mFakeListener.clear();
        mFakePolicy.clear();
    }

    void addDevice(int32_t eventHubId, const std::string& name, uint32_t classes,
                   const PropertyMap* configuration) {
        mFakeEventHub->addDevice(eventHubId, name, classes);

        if (configuration) {
            mFakeEventHub->addConfigurationMap(eventHubId, configuration);
        }
        mFakeEventHub->finishDeviceScan();
        mReader->loopOnce();
        mReader->loopOnce();
        ASSERT_NO_FATAL_FAILURE(mFakePolicy->assertInputDevicesChanged());
        ASSERT_NO_FATAL_FAILURE(mFakeEventHub->assertQueueIsEmpty());
    }

    void disableDevice(int32_t deviceId) {
        mFakePolicy->addDisabledDevice(deviceId);
        mReader->requestRefreshConfiguration(InputReaderConfiguration::CHANGE_ENABLED_STATE);
    }

    void enableDevice(int32_t deviceId) {
        mFakePolicy->removeDisabledDevice(deviceId);
        mReader->requestRefreshConfiguration(InputReaderConfiguration::CHANGE_ENABLED_STATE);
    }

    FakeInputMapper& addDeviceWithFakeInputMapper(int32_t deviceId, int32_t eventHubId,
                                                  const std::string& name, uint32_t classes,
                                                  uint32_t sources,
                                                  const PropertyMap* configuration) {
        std::shared_ptr<InputDevice> device = mReader->newDevice(deviceId, name);
        FakeInputMapper& mapper = device->addMapper<FakeInputMapper>(eventHubId, sources);
        mReader->setNextDevice(device);
        addDevice(eventHubId, name, classes, configuration);
        return mapper;
    }
};

TEST_F(InputReaderTest, GetInputDevices) {
    ASSERT_NO_FATAL_FAILURE(addDevice(1, "keyboard",
            INPUT_DEVICE_CLASS_KEYBOARD, nullptr));
    ASSERT_NO_FATAL_FAILURE(addDevice(2, "ignored",
            0, nullptr)); // no classes so device will be ignored

    std::vector<InputDeviceInfo> inputDevices;
    mReader->getInputDevices(inputDevices);
    ASSERT_EQ(1U, inputDevices.size());
    ASSERT_EQ(END_RESERVED_ID + 1, inputDevices[0].getId());
    ASSERT_STREQ("keyboard", inputDevices[0].getIdentifier().name.c_str());
    ASSERT_EQ(AINPUT_KEYBOARD_TYPE_NON_ALPHABETIC, inputDevices[0].getKeyboardType());
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, inputDevices[0].getSources());
    ASSERT_EQ(size_t(0), inputDevices[0].getMotionRanges().size());

    // Should also have received a notification describing the new input devices.
    inputDevices = mFakePolicy->getInputDevices();
    ASSERT_EQ(1U, inputDevices.size());
    ASSERT_EQ(END_RESERVED_ID + 1, inputDevices[0].getId());
    ASSERT_STREQ("keyboard", inputDevices[0].getIdentifier().name.c_str());
    ASSERT_EQ(AINPUT_KEYBOARD_TYPE_NON_ALPHABETIC, inputDevices[0].getKeyboardType());
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, inputDevices[0].getSources());
    ASSERT_EQ(size_t(0), inputDevices[0].getMotionRanges().size());
}

TEST_F(InputReaderTest, WhenEnabledChanges_SendsDeviceResetNotification) {
    constexpr int32_t deviceId = END_RESERVED_ID + 1000;
    constexpr uint32_t deviceClass = INPUT_DEVICE_CLASS_KEYBOARD;
    constexpr int32_t eventHubId = 1;
    std::shared_ptr<InputDevice> device = mReader->newDevice(deviceId, "fake");
    // Must add at least one mapper or the device will be ignored!
    device->addMapper<FakeInputMapper>(eventHubId, AINPUT_SOURCE_KEYBOARD);
    mReader->setNextDevice(device);
    ASSERT_NO_FATAL_FAILURE(addDevice(eventHubId, "fake", deviceClass, nullptr));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyConfigurationChangedWasCalled(nullptr));

    NotifyDeviceResetArgs resetArgs;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    ASSERT_EQ(deviceId, resetArgs.deviceId);

    ASSERT_EQ(device->isEnabled(), true);
    disableDevice(deviceId);
    mReader->loopOnce();

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    ASSERT_EQ(deviceId, resetArgs.deviceId);
    ASSERT_EQ(device->isEnabled(), false);

    disableDevice(deviceId);
    mReader->loopOnce();
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasNotCalled());
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyConfigurationChangedWasNotCalled());
    ASSERT_EQ(device->isEnabled(), false);

    enableDevice(deviceId);
    mReader->loopOnce();
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    ASSERT_EQ(deviceId, resetArgs.deviceId);
    ASSERT_EQ(device->isEnabled(), true);
}

TEST_F(InputReaderTest, GetKeyCodeState_ForwardsRequestsToMappers) {
    constexpr int32_t deviceId = END_RESERVED_ID + 1000;
    constexpr uint32_t deviceClass = INPUT_DEVICE_CLASS_KEYBOARD;
    constexpr int32_t eventHubId = 1;
    FakeInputMapper& mapper =
            addDeviceWithFakeInputMapper(deviceId, eventHubId, "fake", deviceClass,
                                         AINPUT_SOURCE_KEYBOARD, nullptr);
    mapper.setKeyCodeState(AKEYCODE_A, AKEY_STATE_DOWN);

    ASSERT_EQ(AKEY_STATE_UNKNOWN, mReader->getKeyCodeState(0,
            AINPUT_SOURCE_ANY, AKEYCODE_A))
            << "Should return unknown when the device id is >= 0 but unknown.";

    ASSERT_EQ(AKEY_STATE_UNKNOWN,
              mReader->getKeyCodeState(deviceId, AINPUT_SOURCE_TRACKBALL, AKEYCODE_A))
            << "Should return unknown when the device id is valid but the sources are not "
               "supported by the device.";

    ASSERT_EQ(AKEY_STATE_DOWN,
              mReader->getKeyCodeState(deviceId, AINPUT_SOURCE_KEYBOARD | AINPUT_SOURCE_TRACKBALL,
                                       AKEYCODE_A))
            << "Should return value provided by mapper when device id is valid and the device "
               "supports some of the sources.";

    ASSERT_EQ(AKEY_STATE_UNKNOWN, mReader->getKeyCodeState(-1,
            AINPUT_SOURCE_TRACKBALL, AKEYCODE_A))
            << "Should return unknown when the device id is < 0 but the sources are not supported by any device.";

    ASSERT_EQ(AKEY_STATE_DOWN, mReader->getKeyCodeState(-1,
            AINPUT_SOURCE_KEYBOARD | AINPUT_SOURCE_TRACKBALL, AKEYCODE_A))
            << "Should return value provided by mapper when device id is < 0 and one of the devices supports some of the sources.";
}

TEST_F(InputReaderTest, GetScanCodeState_ForwardsRequestsToMappers) {
    constexpr int32_t deviceId = END_RESERVED_ID + 1000;
    constexpr uint32_t deviceClass = INPUT_DEVICE_CLASS_KEYBOARD;
    constexpr int32_t eventHubId = 1;
    FakeInputMapper& mapper =
            addDeviceWithFakeInputMapper(deviceId, eventHubId, "fake", deviceClass,
                                         AINPUT_SOURCE_KEYBOARD, nullptr);
    mapper.setScanCodeState(KEY_A, AKEY_STATE_DOWN);

    ASSERT_EQ(AKEY_STATE_UNKNOWN, mReader->getScanCodeState(0,
            AINPUT_SOURCE_ANY, KEY_A))
            << "Should return unknown when the device id is >= 0 but unknown.";

    ASSERT_EQ(AKEY_STATE_UNKNOWN,
              mReader->getScanCodeState(deviceId, AINPUT_SOURCE_TRACKBALL, KEY_A))
            << "Should return unknown when the device id is valid but the sources are not "
               "supported by the device.";

    ASSERT_EQ(AKEY_STATE_DOWN,
              mReader->getScanCodeState(deviceId, AINPUT_SOURCE_KEYBOARD | AINPUT_SOURCE_TRACKBALL,
                                        KEY_A))
            << "Should return value provided by mapper when device id is valid and the device "
               "supports some of the sources.";

    ASSERT_EQ(AKEY_STATE_UNKNOWN, mReader->getScanCodeState(-1,
            AINPUT_SOURCE_TRACKBALL, KEY_A))
            << "Should return unknown when the device id is < 0 but the sources are not supported by any device.";

    ASSERT_EQ(AKEY_STATE_DOWN, mReader->getScanCodeState(-1,
            AINPUT_SOURCE_KEYBOARD | AINPUT_SOURCE_TRACKBALL, KEY_A))
            << "Should return value provided by mapper when device id is < 0 and one of the devices supports some of the sources.";
}

TEST_F(InputReaderTest, GetSwitchState_ForwardsRequestsToMappers) {
    constexpr int32_t deviceId = END_RESERVED_ID + 1000;
    constexpr uint32_t deviceClass = INPUT_DEVICE_CLASS_KEYBOARD;
    constexpr int32_t eventHubId = 1;
    FakeInputMapper& mapper =
            addDeviceWithFakeInputMapper(deviceId, eventHubId, "fake", deviceClass,
                                         AINPUT_SOURCE_KEYBOARD, nullptr);
    mapper.setSwitchState(SW_LID, AKEY_STATE_DOWN);

    ASSERT_EQ(AKEY_STATE_UNKNOWN, mReader->getSwitchState(0,
            AINPUT_SOURCE_ANY, SW_LID))
            << "Should return unknown when the device id is >= 0 but unknown.";

    ASSERT_EQ(AKEY_STATE_UNKNOWN,
              mReader->getSwitchState(deviceId, AINPUT_SOURCE_TRACKBALL, SW_LID))
            << "Should return unknown when the device id is valid but the sources are not "
               "supported by the device.";

    ASSERT_EQ(AKEY_STATE_DOWN,
              mReader->getSwitchState(deviceId, AINPUT_SOURCE_KEYBOARD | AINPUT_SOURCE_TRACKBALL,
                                      SW_LID))
            << "Should return value provided by mapper when device id is valid and the device "
               "supports some of the sources.";

    ASSERT_EQ(AKEY_STATE_UNKNOWN, mReader->getSwitchState(-1,
            AINPUT_SOURCE_TRACKBALL, SW_LID))
            << "Should return unknown when the device id is < 0 but the sources are not supported by any device.";

    ASSERT_EQ(AKEY_STATE_DOWN, mReader->getSwitchState(-1,
            AINPUT_SOURCE_KEYBOARD | AINPUT_SOURCE_TRACKBALL, SW_LID))
            << "Should return value provided by mapper when device id is < 0 and one of the devices supports some of the sources.";
}

TEST_F(InputReaderTest, MarkSupportedKeyCodes_ForwardsRequestsToMappers) {
    constexpr int32_t deviceId = END_RESERVED_ID + 1000;
    constexpr uint32_t deviceClass = INPUT_DEVICE_CLASS_KEYBOARD;
    constexpr int32_t eventHubId = 1;
    FakeInputMapper& mapper =
            addDeviceWithFakeInputMapper(deviceId, eventHubId, "fake", deviceClass,
                                         AINPUT_SOURCE_KEYBOARD, nullptr);

    mapper.addSupportedKeyCode(AKEYCODE_A);
    mapper.addSupportedKeyCode(AKEYCODE_B);

    const int32_t keyCodes[4] = { AKEYCODE_A, AKEYCODE_B, AKEYCODE_1, AKEYCODE_2 };
    uint8_t flags[4] = { 0, 0, 0, 1 };

    ASSERT_FALSE(mReader->hasKeys(0, AINPUT_SOURCE_ANY, 4, keyCodes, flags))
            << "Should return false when device id is >= 0 but unknown.";
    ASSERT_TRUE(!flags[0] && !flags[1] && !flags[2] && !flags[3]);

    flags[3] = 1;
    ASSERT_FALSE(mReader->hasKeys(deviceId, AINPUT_SOURCE_TRACKBALL, 4, keyCodes, flags))
            << "Should return false when device id is valid but the sources are not supported by "
               "the device.";
    ASSERT_TRUE(!flags[0] && !flags[1] && !flags[2] && !flags[3]);

    flags[3] = 1;
    ASSERT_TRUE(mReader->hasKeys(deviceId, AINPUT_SOURCE_KEYBOARD | AINPUT_SOURCE_TRACKBALL, 4,
                                 keyCodes, flags))
            << "Should return value provided by mapper when device id is valid and the device "
               "supports some of the sources.";
    ASSERT_TRUE(flags[0] && flags[1] && !flags[2] && !flags[3]);

    flags[3] = 1;
    ASSERT_FALSE(mReader->hasKeys(-1, AINPUT_SOURCE_TRACKBALL, 4, keyCodes, flags))
            << "Should return false when the device id is < 0 but the sources are not supported by any device.";
    ASSERT_TRUE(!flags[0] && !flags[1] && !flags[2] && !flags[3]);

    flags[3] = 1;
    ASSERT_TRUE(mReader->hasKeys(-1, AINPUT_SOURCE_KEYBOARD | AINPUT_SOURCE_TRACKBALL, 4, keyCodes, flags))
            << "Should return value provided by mapper when device id is < 0 and one of the devices supports some of the sources.";
    ASSERT_TRUE(flags[0] && flags[1] && !flags[2] && !flags[3]);
}

TEST_F(InputReaderTest, LoopOnce_WhenDeviceScanFinished_SendsConfigurationChanged) {
    constexpr int32_t eventHubId = 1;
    addDevice(eventHubId, "ignored", INPUT_DEVICE_CLASS_KEYBOARD, nullptr);

    NotifyConfigurationChangedArgs args;

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyConfigurationChangedWasCalled(&args));
    ASSERT_EQ(ARBITRARY_TIME, args.eventTime);
}

TEST_F(InputReaderTest, LoopOnce_ForwardsRawEventsToMappers) {
    constexpr int32_t deviceId = END_RESERVED_ID + 1000;
    constexpr uint32_t deviceClass = INPUT_DEVICE_CLASS_KEYBOARD;
    constexpr int32_t eventHubId = 1;
    FakeInputMapper& mapper =
            addDeviceWithFakeInputMapper(deviceId, eventHubId, "fake", deviceClass,
                                         AINPUT_SOURCE_KEYBOARD, nullptr);

    mFakeEventHub->enqueueEvent(0, eventHubId, EV_KEY, KEY_A, 1);
    mReader->loopOnce();
    ASSERT_NO_FATAL_FAILURE(mFakeEventHub->assertQueueIsEmpty());

    RawEvent event;
    ASSERT_NO_FATAL_FAILURE(mapper.assertProcessWasCalled(&event));
    ASSERT_EQ(0, event.when);
    ASSERT_EQ(eventHubId, event.deviceId);
    ASSERT_EQ(EV_KEY, event.type);
    ASSERT_EQ(KEY_A, event.code);
    ASSERT_EQ(1, event.value);
}

TEST_F(InputReaderTest, DeviceReset_RandomId) {
    constexpr int32_t deviceId = END_RESERVED_ID + 1000;
    constexpr uint32_t deviceClass = INPUT_DEVICE_CLASS_KEYBOARD;
    constexpr int32_t eventHubId = 1;
    std::shared_ptr<InputDevice> device = mReader->newDevice(deviceId, "fake");
    // Must add at least one mapper or the device will be ignored!
    device->addMapper<FakeInputMapper>(eventHubId, AINPUT_SOURCE_KEYBOARD);
    mReader->setNextDevice(device);
    ASSERT_NO_FATAL_FAILURE(addDevice(eventHubId, "fake", deviceClass, nullptr));

    NotifyDeviceResetArgs resetArgs;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    int32_t prevId = resetArgs.id;

    disableDevice(deviceId);
    mReader->loopOnce();
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    ASSERT_NE(prevId, resetArgs.id);
    prevId = resetArgs.id;

    enableDevice(deviceId);
    mReader->loopOnce();
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    ASSERT_NE(prevId, resetArgs.id);
    prevId = resetArgs.id;

    disableDevice(deviceId);
    mReader->loopOnce();
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    ASSERT_NE(prevId, resetArgs.id);
    prevId = resetArgs.id;
}

TEST_F(InputReaderTest, DeviceReset_GenerateIdWithInputReaderSource) {
    constexpr int32_t deviceId = 1;
    constexpr uint32_t deviceClass = INPUT_DEVICE_CLASS_KEYBOARD;
    constexpr int32_t eventHubId = 1;
    std::shared_ptr<InputDevice> device = mReader->newDevice(deviceId, "fake");
    // Must add at least one mapper or the device will be ignored!
    device->addMapper<FakeInputMapper>(eventHubId, AINPUT_SOURCE_KEYBOARD);
    mReader->setNextDevice(device);
    ASSERT_NO_FATAL_FAILURE(addDevice(deviceId, "fake", deviceClass, nullptr));

    NotifyDeviceResetArgs resetArgs;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    ASSERT_EQ(IdGenerator::Source::INPUT_READER, IdGenerator::getSource(resetArgs.id));
}

TEST_F(InputReaderTest, Device_CanDispatchToDisplay) {
    constexpr int32_t deviceId = END_RESERVED_ID + 1000;
    constexpr uint32_t deviceClass = INPUT_DEVICE_CLASS_KEYBOARD;
    constexpr int32_t eventHubId = 1;
    const char* DEVICE_LOCATION = "USB1";
    std::shared_ptr<InputDevice> device = mReader->newDevice(deviceId, "fake", DEVICE_LOCATION);
    FakeInputMapper& mapper =
            device->addMapper<FakeInputMapper>(eventHubId, AINPUT_SOURCE_TOUCHSCREEN);
    mReader->setNextDevice(device);

    const uint8_t hdmi1 = 1;

    // Associated touch screen with second display.
    mFakePolicy->addInputPortAssociation(DEVICE_LOCATION, hdmi1);

    // Add default and second display.
    mFakePolicy->clearViewports();
    mFakePolicy->addDisplayViewport(DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, "local:0", NO_PORT, ViewportType::VIEWPORT_INTERNAL);
    mFakePolicy->addDisplayViewport(SECONDARY_DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, "local:1", hdmi1, ViewportType::VIEWPORT_EXTERNAL);
    mReader->requestRefreshConfiguration(InputReaderConfiguration::CHANGE_DISPLAY_INFO);
    mReader->loopOnce();

    // Add the device, and make sure all of the callbacks are triggered.
    // The device is added after the input port associations are processed since
    // we do not yet support dynamic device-to-display associations.
    ASSERT_NO_FATAL_FAILURE(addDevice(eventHubId, "fake", deviceClass, nullptr));
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyConfigurationChangedWasCalled());
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled());
    ASSERT_NO_FATAL_FAILURE(mapper.assertConfigureWasCalled());

    // Device should only dispatch to the specified display.
    ASSERT_EQ(deviceId, device->getId());
    ASSERT_FALSE(mReader->canDispatchToDisplay(deviceId, DISPLAY_ID));
    ASSERT_TRUE(mReader->canDispatchToDisplay(deviceId, SECONDARY_DISPLAY_ID));

    // Can't dispatch event from a disabled device.
    disableDevice(deviceId);
    mReader->loopOnce();
    ASSERT_FALSE(mReader->canDispatchToDisplay(deviceId, SECONDARY_DISPLAY_ID));
}

// --- InputReaderIntegrationTest ---

// These tests create and interact with the InputReader only through its interface.
// The InputReader is started during SetUp(), which starts its processing in its own
// thread. The tests use linux uinput to emulate input devices.
// NOTE: Interacting with the physical device while these tests are running may cause
// the tests to fail.
class InputReaderIntegrationTest : public testing::Test {
protected:
    sp<TestInputListener> mTestListener;
    sp<FakeInputReaderPolicy> mFakePolicy;
    sp<InputReaderInterface> mReader;

    virtual void SetUp() override {
        mFakePolicy = new FakeInputReaderPolicy();
        mTestListener = new TestInputListener(2000ms /*eventHappenedTimeout*/,
                                              30ms /*eventDidNotHappenTimeout*/);

        mReader = new InputReader(std::make_shared<EventHub>(), mFakePolicy, mTestListener);
        ASSERT_EQ(mReader->start(), OK);

        // Since this test is run on a real device, all the input devices connected
        // to the test device will show up in mReader. We wait for those input devices to
        // show up before beginning the tests.
        ASSERT_NO_FATAL_FAILURE(mFakePolicy->assertInputDevicesChanged());
        ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyConfigurationChangedWasCalled());
    }

    virtual void TearDown() override {
        ASSERT_EQ(mReader->stop(), OK);
        mTestListener.clear();
        mFakePolicy.clear();
    }
};

TEST_F(InputReaderIntegrationTest, TestInvalidDevice) {
    // An invalid input device that is only used for this test.
    class InvalidUinputDevice : public UinputDevice {
    public:
        InvalidUinputDevice() : UinputDevice("Invalid Device") {}

    private:
        void configureDevice(int fd, uinput_user_dev* device) override {}
    };

    const size_t numDevices = mFakePolicy->getInputDevices().size();

    // UinputDevice does not set any event or key bits, so InputReader should not
    // consider it as a valid device.
    std::unique_ptr<UinputDevice> invalidDevice = createUinputDevice<InvalidUinputDevice>();
    ASSERT_NO_FATAL_FAILURE(mFakePolicy->assertInputDevicesNotChanged());
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyConfigurationChangedWasNotCalled());
    ASSERT_EQ(numDevices, mFakePolicy->getInputDevices().size());

    invalidDevice.reset();
    ASSERT_NO_FATAL_FAILURE(mFakePolicy->assertInputDevicesNotChanged());
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyConfigurationChangedWasNotCalled());
    ASSERT_EQ(numDevices, mFakePolicy->getInputDevices().size());
}

TEST_F(InputReaderIntegrationTest, AddNewDevice) {
    const size_t initialNumDevices = mFakePolicy->getInputDevices().size();

    std::unique_ptr<UinputHomeKey> keyboard = createUinputDevice<UinputHomeKey>();
    ASSERT_NO_FATAL_FAILURE(mFakePolicy->assertInputDevicesChanged());
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyConfigurationChangedWasCalled());
    ASSERT_EQ(initialNumDevices + 1, mFakePolicy->getInputDevices().size());

    // Find the test device by its name.
    std::vector<InputDeviceInfo> inputDevices;
    mReader->getInputDevices(inputDevices);
    InputDeviceInfo* keyboardInfo = nullptr;
    const char* keyboardName = keyboard->getName();
    for (unsigned int i = 0; i < initialNumDevices + 1; i++) {
        if (!strcmp(inputDevices[i].getIdentifier().name.c_str(), keyboardName)) {
            keyboardInfo = &inputDevices[i];
            break;
        }
    }
    ASSERT_NE(keyboardInfo, nullptr);
    ASSERT_EQ(AINPUT_KEYBOARD_TYPE_NON_ALPHABETIC, keyboardInfo->getKeyboardType());
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, keyboardInfo->getSources());
    ASSERT_EQ(0U, keyboardInfo->getMotionRanges().size());

    keyboard.reset();
    ASSERT_NO_FATAL_FAILURE(mFakePolicy->assertInputDevicesChanged());
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyConfigurationChangedWasCalled());
    ASSERT_EQ(initialNumDevices, mFakePolicy->getInputDevices().size());
}

TEST_F(InputReaderIntegrationTest, SendsEventsToInputListener) {
    std::unique_ptr<UinputHomeKey> keyboard = createUinputDevice<UinputHomeKey>();
    ASSERT_NO_FATAL_FAILURE(mFakePolicy->assertInputDevicesChanged());

    NotifyConfigurationChangedArgs configChangedArgs;
    ASSERT_NO_FATAL_FAILURE(
            mTestListener->assertNotifyConfigurationChangedWasCalled(&configChangedArgs));
    int32_t prevId = configChangedArgs.id;
    nsecs_t prevTimestamp = configChangedArgs.eventTime;

    NotifyKeyArgs keyArgs;
    keyboard->pressAndReleaseHomeKey();
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_NE(prevId, keyArgs.id);
    prevId = keyArgs.id;
    ASSERT_LE(prevTimestamp, keyArgs.eventTime);
    prevTimestamp = keyArgs.eventTime;

    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_NE(prevId, keyArgs.id);
    ASSERT_LE(prevTimestamp, keyArgs.eventTime);
}

/**
 * The Steam controller sends BTN_GEAR_DOWN and BTN_GEAR_UP for the two "paddle" buttons
 * on the back. In this test, we make sure that BTN_GEAR_DOWN / BTN_WHEEL and BTN_GEAR_UP
 * are passed to the listener.
 */
static_assert(BTN_GEAR_DOWN == BTN_WHEEL);
TEST_F(InputReaderIntegrationTest, SendsGearDownAndUpToInputListener) {
    std::unique_ptr<UinputSteamController> controller = createUinputDevice<UinputSteamController>();
    ASSERT_NO_FATAL_FAILURE(mFakePolicy->assertInputDevicesChanged());
    NotifyKeyArgs keyArgs;

    controller->pressAndReleaseKey(BTN_GEAR_DOWN);
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyKeyWasCalled(&keyArgs)); // ACTION_DOWN
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyKeyWasCalled(&keyArgs)); // ACTION_UP
    ASSERT_EQ(BTN_GEAR_DOWN, keyArgs.scanCode);

    controller->pressAndReleaseKey(BTN_GEAR_UP);
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyKeyWasCalled(&keyArgs)); // ACTION_DOWN
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyKeyWasCalled(&keyArgs)); // ACTION_UP
    ASSERT_EQ(BTN_GEAR_UP, keyArgs.scanCode);
}

// --- TouchProcessTest ---
class TouchIntegrationTest : public InputReaderIntegrationTest {
protected:
    static const int32_t FIRST_SLOT = 0;
    static const int32_t SECOND_SLOT = 1;
    static const int32_t FIRST_TRACKING_ID = 0;
    static const int32_t SECOND_TRACKING_ID = 1;
    const std::string UNIQUE_ID = "local:0";

    virtual void SetUp() override {
        InputReaderIntegrationTest::SetUp();
        // At least add an internal display.
        setDisplayInfoAndReconfigure(DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_ORIENTATION_0, UNIQUE_ID, NO_PORT,
                                     ViewportType::VIEWPORT_INTERNAL);

        mDevice = createUinputDevice<UinputTouchScreen>(Rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT));
        ASSERT_NO_FATAL_FAILURE(mFakePolicy->assertInputDevicesChanged());
        ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyConfigurationChangedWasCalled());
    }

    void setDisplayInfoAndReconfigure(int32_t displayId, int32_t width, int32_t height,
                                      int32_t orientation, const std::string& uniqueId,
                                      std::optional<uint8_t> physicalPort,
                                      ViewportType viewportType) {
        mFakePolicy->addDisplayViewport(displayId, width, height, orientation, uniqueId,
                                        physicalPort, viewportType);
        mReader->requestRefreshConfiguration(InputReaderConfiguration::CHANGE_DISPLAY_INFO);
    }

    std::unique_ptr<UinputTouchScreen> mDevice;
};

TEST_F(TouchIntegrationTest, InputEvent_ProcessSingleTouch) {
    NotifyMotionArgs args;
    const Point centerPoint = mDevice->getCenterPoint();

    // ACTION_DOWN
    mDevice->sendDown(centerPoint);
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, args.action);

    // ACTION_MOVE
    mDevice->sendMove(centerPoint + Point(1, 1));
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, args.action);

    // ACTION_UP
    mDevice->sendUp();
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, args.action);
}

TEST_F(TouchIntegrationTest, InputEvent_ProcessMultiTouch) {
    NotifyMotionArgs args;
    const Point centerPoint = mDevice->getCenterPoint();

    // ACTION_DOWN
    mDevice->sendDown(centerPoint);
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, args.action);

    // ACTION_POINTER_DOWN (Second slot)
    const Point secondPoint = centerPoint + Point(100, 100);
    mDevice->sendSlot(SECOND_SLOT);
    mDevice->sendTrackingId(SECOND_TRACKING_ID);
    mDevice->sendDown(secondPoint + Point(1, 1));
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_DOWN | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
              args.action);

    // ACTION_MOVE (Second slot)
    mDevice->sendMove(secondPoint);
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, args.action);

    // ACTION_POINTER_UP (Second slot)
    mDevice->sendUp();
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_UP | (0 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
              args.action);

    // ACTION_UP
    mDevice->sendSlot(FIRST_SLOT);
    mDevice->sendUp();
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, args.action);
}

TEST_F(TouchIntegrationTest, InputEvent_ProcessPalm) {
    NotifyMotionArgs args;
    const Point centerPoint = mDevice->getCenterPoint();

    // ACTION_DOWN
    mDevice->sendDown(centerPoint);
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, args.action);

    // ACTION_POINTER_DOWN (Second slot)
    const Point secondPoint = centerPoint + Point(100, 100);
    mDevice->sendSlot(SECOND_SLOT);
    mDevice->sendTrackingId(SECOND_TRACKING_ID);
    mDevice->sendDown(secondPoint);
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_DOWN | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
              args.action);

    // ACTION_MOVE (Second slot)
    mDevice->sendMove(secondPoint + Point(1, 1));
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, args.action);

    // Send MT_TOOL_PALM, which indicates that the touch IC has determined this to be a grip event.
    // Expect to receive ACTION_CANCEL, to abort the entire gesture.
    mDevice->sendToolType(MT_TOOL_PALM);
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_CANCEL, args.action);

    // ACTION_POINTER_UP (Second slot)
    mDevice->sendUp();

    // ACTION_UP
    mDevice->sendSlot(FIRST_SLOT);
    mDevice->sendUp();

    // Expect no event received after abort the entire gesture.
    ASSERT_NO_FATAL_FAILURE(mTestListener->assertNotifyMotionWasNotCalled());
}

// --- InputDeviceTest ---
class InputDeviceTest : public testing::Test {
protected:
    static const char* DEVICE_NAME;
    static const char* DEVICE_LOCATION;
    static const int32_t DEVICE_ID;
    static const int32_t DEVICE_GENERATION;
    static const int32_t DEVICE_CONTROLLER_NUMBER;
    static const uint32_t DEVICE_CLASSES;
    static const int32_t EVENTHUB_ID;

    std::shared_ptr<FakeEventHub> mFakeEventHub;
    sp<FakeInputReaderPolicy> mFakePolicy;
    sp<TestInputListener> mFakeListener;
    FakeInputReaderContext* mFakeContext;

    std::shared_ptr<InputDevice> mDevice;

    virtual void SetUp() override {
        mFakeEventHub = std::make_unique<FakeEventHub>();
        mFakePolicy = new FakeInputReaderPolicy();
        mFakeListener = new TestInputListener();
        mFakeContext = new FakeInputReaderContext(mFakeEventHub, mFakePolicy, mFakeListener);

        mFakeEventHub->addDevice(EVENTHUB_ID, DEVICE_NAME, 0);
        InputDeviceIdentifier identifier;
        identifier.name = DEVICE_NAME;
        identifier.location = DEVICE_LOCATION;
        mDevice = std::make_shared<InputDevice>(mFakeContext, DEVICE_ID, DEVICE_GENERATION,
                                                identifier);
    }

    virtual void TearDown() override {
        mDevice = nullptr;
        delete mFakeContext;
        mFakeListener.clear();
        mFakePolicy.clear();
    }
};

const char* InputDeviceTest::DEVICE_NAME = "device";
const char* InputDeviceTest::DEVICE_LOCATION = "USB1";
const int32_t InputDeviceTest::DEVICE_ID = END_RESERVED_ID + 1000;
const int32_t InputDeviceTest::DEVICE_GENERATION = 2;
const int32_t InputDeviceTest::DEVICE_CONTROLLER_NUMBER = 0;
const uint32_t InputDeviceTest::DEVICE_CLASSES = INPUT_DEVICE_CLASS_KEYBOARD
        | INPUT_DEVICE_CLASS_TOUCH | INPUT_DEVICE_CLASS_JOYSTICK;
const int32_t InputDeviceTest::EVENTHUB_ID = 1;

TEST_F(InputDeviceTest, ImmutableProperties) {
    ASSERT_EQ(DEVICE_ID, mDevice->getId());
    ASSERT_STREQ(DEVICE_NAME, mDevice->getName().c_str());
    ASSERT_EQ(0U, mDevice->getClasses());
}

TEST_F(InputDeviceTest, WhenDeviceCreated_EnabledIsFalse) {
    ASSERT_EQ(mDevice->isEnabled(), false);
}

TEST_F(InputDeviceTest, WhenNoMappersAreRegistered_DeviceIsIgnored) {
    // Configuration.
    InputReaderConfiguration config;
    mDevice->configure(ARBITRARY_TIME, &config, 0);

    // Reset.
    mDevice->reset(ARBITRARY_TIME);

    NotifyDeviceResetArgs resetArgs;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    ASSERT_EQ(ARBITRARY_TIME, resetArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, resetArgs.deviceId);

    // Metadata.
    ASSERT_TRUE(mDevice->isIgnored());
    ASSERT_EQ(AINPUT_SOURCE_UNKNOWN, mDevice->getSources());

    InputDeviceInfo info;
    mDevice->getDeviceInfo(&info);
    ASSERT_EQ(DEVICE_ID, info.getId());
    ASSERT_STREQ(DEVICE_NAME, info.getIdentifier().name.c_str());
    ASSERT_EQ(AINPUT_KEYBOARD_TYPE_NONE, info.getKeyboardType());
    ASSERT_EQ(AINPUT_SOURCE_UNKNOWN, info.getSources());

    // State queries.
    ASSERT_EQ(0, mDevice->getMetaState());

    ASSERT_EQ(AKEY_STATE_UNKNOWN, mDevice->getKeyCodeState(AINPUT_SOURCE_KEYBOARD, 0))
            << "Ignored device should return unknown key code state.";
    ASSERT_EQ(AKEY_STATE_UNKNOWN, mDevice->getScanCodeState(AINPUT_SOURCE_KEYBOARD, 0))
            << "Ignored device should return unknown scan code state.";
    ASSERT_EQ(AKEY_STATE_UNKNOWN, mDevice->getSwitchState(AINPUT_SOURCE_KEYBOARD, 0))
            << "Ignored device should return unknown switch state.";

    const int32_t keyCodes[2] = { AKEYCODE_A, AKEYCODE_B };
    uint8_t flags[2] = { 0, 1 };
    ASSERT_FALSE(mDevice->markSupportedKeyCodes(AINPUT_SOURCE_KEYBOARD, 2, keyCodes, flags))
            << "Ignored device should never mark any key codes.";
    ASSERT_EQ(0, flags[0]) << "Flag for unsupported key should be unchanged.";
    ASSERT_EQ(1, flags[1]) << "Flag for unsupported key should be unchanged.";
}

TEST_F(InputDeviceTest, WhenMappersAreRegistered_DeviceIsNotIgnoredAndForwardsRequestsToMappers) {
    // Configuration.
    mFakeEventHub->addConfigurationProperty(EVENTHUB_ID, String8("key"), String8("value"));

    FakeInputMapper& mapper1 =
            mDevice->addMapper<FakeInputMapper>(EVENTHUB_ID, AINPUT_SOURCE_KEYBOARD);
    mapper1.setKeyboardType(AINPUT_KEYBOARD_TYPE_ALPHABETIC);
    mapper1.setMetaState(AMETA_ALT_ON);
    mapper1.addSupportedKeyCode(AKEYCODE_A);
    mapper1.addSupportedKeyCode(AKEYCODE_B);
    mapper1.setKeyCodeState(AKEYCODE_A, AKEY_STATE_DOWN);
    mapper1.setKeyCodeState(AKEYCODE_B, AKEY_STATE_UP);
    mapper1.setScanCodeState(2, AKEY_STATE_DOWN);
    mapper1.setScanCodeState(3, AKEY_STATE_UP);
    mapper1.setSwitchState(4, AKEY_STATE_DOWN);

    FakeInputMapper& mapper2 =
            mDevice->addMapper<FakeInputMapper>(EVENTHUB_ID, AINPUT_SOURCE_TOUCHSCREEN);
    mapper2.setMetaState(AMETA_SHIFT_ON);

    InputReaderConfiguration config;
    mDevice->configure(ARBITRARY_TIME, &config, 0);

    String8 propertyValue;
    ASSERT_TRUE(mDevice->getConfiguration().tryGetProperty(String8("key"), propertyValue))
            << "Device should have read configuration during configuration phase.";
    ASSERT_STREQ("value", propertyValue.string());

    ASSERT_NO_FATAL_FAILURE(mapper1.assertConfigureWasCalled());
    ASSERT_NO_FATAL_FAILURE(mapper2.assertConfigureWasCalled());

    // Reset
    mDevice->reset(ARBITRARY_TIME);
    ASSERT_NO_FATAL_FAILURE(mapper1.assertResetWasCalled());
    ASSERT_NO_FATAL_FAILURE(mapper2.assertResetWasCalled());

    NotifyDeviceResetArgs resetArgs;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    ASSERT_EQ(ARBITRARY_TIME, resetArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, resetArgs.deviceId);

    // Metadata.
    ASSERT_FALSE(mDevice->isIgnored());
    ASSERT_EQ(uint32_t(AINPUT_SOURCE_KEYBOARD | AINPUT_SOURCE_TOUCHSCREEN), mDevice->getSources());

    InputDeviceInfo info;
    mDevice->getDeviceInfo(&info);
    ASSERT_EQ(DEVICE_ID, info.getId());
    ASSERT_STREQ(DEVICE_NAME, info.getIdentifier().name.c_str());
    ASSERT_EQ(AINPUT_KEYBOARD_TYPE_ALPHABETIC, info.getKeyboardType());
    ASSERT_EQ(uint32_t(AINPUT_SOURCE_KEYBOARD | AINPUT_SOURCE_TOUCHSCREEN), info.getSources());

    // State queries.
    ASSERT_EQ(AMETA_ALT_ON | AMETA_SHIFT_ON, mDevice->getMetaState())
            << "Should query mappers and combine meta states.";

    ASSERT_EQ(AKEY_STATE_UNKNOWN, mDevice->getKeyCodeState(AINPUT_SOURCE_TRACKBALL, AKEYCODE_A))
            << "Should return unknown key code state when source not supported.";
    ASSERT_EQ(AKEY_STATE_UNKNOWN, mDevice->getScanCodeState(AINPUT_SOURCE_TRACKBALL, AKEYCODE_A))
            << "Should return unknown scan code state when source not supported.";
    ASSERT_EQ(AKEY_STATE_UNKNOWN, mDevice->getSwitchState(AINPUT_SOURCE_TRACKBALL, AKEYCODE_A))
            << "Should return unknown switch state when source not supported.";

    ASSERT_EQ(AKEY_STATE_DOWN, mDevice->getKeyCodeState(AINPUT_SOURCE_KEYBOARD, AKEYCODE_A))
            << "Should query mapper when source is supported.";
    ASSERT_EQ(AKEY_STATE_UP, mDevice->getScanCodeState(AINPUT_SOURCE_KEYBOARD, 3))
            << "Should query mapper when source is supported.";
    ASSERT_EQ(AKEY_STATE_DOWN, mDevice->getSwitchState(AINPUT_SOURCE_KEYBOARD, 4))
            << "Should query mapper when source is supported.";

    const int32_t keyCodes[4] = { AKEYCODE_A, AKEYCODE_B, AKEYCODE_1, AKEYCODE_2 };
    uint8_t flags[4] = { 0, 0, 0, 1 };
    ASSERT_FALSE(mDevice->markSupportedKeyCodes(AINPUT_SOURCE_TRACKBALL, 4, keyCodes, flags))
            << "Should do nothing when source is unsupported.";
    ASSERT_EQ(0, flags[0]) << "Flag should be unchanged when source is unsupported.";
    ASSERT_EQ(0, flags[1]) << "Flag should be unchanged when source is unsupported.";
    ASSERT_EQ(0, flags[2]) << "Flag should be unchanged when source is unsupported.";
    ASSERT_EQ(1, flags[3]) << "Flag should be unchanged when source is unsupported.";

    ASSERT_TRUE(mDevice->markSupportedKeyCodes(AINPUT_SOURCE_KEYBOARD, 4, keyCodes, flags))
            << "Should query mapper when source is supported.";
    ASSERT_EQ(1, flags[0]) << "Flag for supported key should be set.";
    ASSERT_EQ(1, flags[1]) << "Flag for supported key should be set.";
    ASSERT_EQ(0, flags[2]) << "Flag for unsupported key should be unchanged.";
    ASSERT_EQ(1, flags[3]) << "Flag for unsupported key should be unchanged.";

    // Event handling.
    RawEvent event;
    event.deviceId = EVENTHUB_ID;
    mDevice->process(&event, 1);

    ASSERT_NO_FATAL_FAILURE(mapper1.assertProcessWasCalled());
    ASSERT_NO_FATAL_FAILURE(mapper2.assertProcessWasCalled());
}

// A single input device is associated with a specific display. Check that:
// 1. Device is disabled if the viewport corresponding to the associated display is not found
// 2. Device is disabled when setEnabled API is called
TEST_F(InputDeviceTest, Configure_AssignsDisplayPort) {
    mDevice->addMapper<FakeInputMapper>(EVENTHUB_ID, AINPUT_SOURCE_TOUCHSCREEN);

    // First Configuration.
    mDevice->configure(ARBITRARY_TIME, mFakePolicy->getReaderConfiguration(), 0);

    // Device should be enabled by default.
    ASSERT_TRUE(mDevice->isEnabled());

    // Prepare associated info.
    constexpr uint8_t hdmi = 1;
    const std::string UNIQUE_ID = "local:1";

    mFakePolicy->addInputPortAssociation(DEVICE_LOCATION, hdmi);
    mDevice->configure(ARBITRARY_TIME, mFakePolicy->getReaderConfiguration(),
                       InputReaderConfiguration::CHANGE_DISPLAY_INFO);
    // Device should be disabled because it is associated with a specific display via
    // input port <-> display port association, but the corresponding display is not found
    ASSERT_FALSE(mDevice->isEnabled());

    // Prepare displays.
    mFakePolicy->addDisplayViewport(SECONDARY_DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                    DISPLAY_ORIENTATION_0, UNIQUE_ID, hdmi,
                                    ViewportType::VIEWPORT_INTERNAL);
    mDevice->configure(ARBITRARY_TIME, mFakePolicy->getReaderConfiguration(),
                       InputReaderConfiguration::CHANGE_DISPLAY_INFO);
    ASSERT_TRUE(mDevice->isEnabled());

    // Device should be disabled after set disable.
    mFakePolicy->addDisabledDevice(mDevice->getId());
    mDevice->configure(ARBITRARY_TIME, mFakePolicy->getReaderConfiguration(),
                       InputReaderConfiguration::CHANGE_ENABLED_STATE);
    ASSERT_FALSE(mDevice->isEnabled());

    // Device should still be disabled even found the associated display.
    mDevice->configure(ARBITRARY_TIME, mFakePolicy->getReaderConfiguration(),
                       InputReaderConfiguration::CHANGE_DISPLAY_INFO);
    ASSERT_FALSE(mDevice->isEnabled());
}

// --- InputMapperTest ---

class InputMapperTest : public testing::Test {
protected:
    static const char* DEVICE_NAME;
    static const char* DEVICE_LOCATION;
    static const int32_t DEVICE_ID;
    static const int32_t DEVICE_GENERATION;
    static const int32_t DEVICE_CONTROLLER_NUMBER;
    static const uint32_t DEVICE_CLASSES;
    static const int32_t EVENTHUB_ID;

    std::shared_ptr<FakeEventHub> mFakeEventHub;
    sp<FakeInputReaderPolicy> mFakePolicy;
    sp<TestInputListener> mFakeListener;
    FakeInputReaderContext* mFakeContext;
    InputDevice* mDevice;

    virtual void SetUp(uint32_t classes) {
        mFakeEventHub = std::make_unique<FakeEventHub>();
        mFakePolicy = new FakeInputReaderPolicy();
        mFakeListener = new TestInputListener();
        mFakeContext = new FakeInputReaderContext(mFakeEventHub, mFakePolicy, mFakeListener);
        InputDeviceIdentifier identifier;
        identifier.name = DEVICE_NAME;
        identifier.location = DEVICE_LOCATION;
        mDevice = new InputDevice(mFakeContext, DEVICE_ID, DEVICE_GENERATION, identifier);

        mFakeEventHub->addDevice(EVENTHUB_ID, DEVICE_NAME, classes);
    }

    virtual void SetUp() override { SetUp(DEVICE_CLASSES); }

    virtual void TearDown() override {
        delete mDevice;
        delete mFakeContext;
        mFakeListener.clear();
        mFakePolicy.clear();
    }

    void addConfigurationProperty(const char* key, const char* value) {
        mFakeEventHub->addConfigurationProperty(EVENTHUB_ID, String8(key), String8(value));
    }

    void configureDevice(uint32_t changes) {
        if (!changes || (changes & InputReaderConfiguration::CHANGE_DISPLAY_INFO)) {
            mFakeContext->updatePointerDisplay();
        }
        mDevice->configure(ARBITRARY_TIME, mFakePolicy->getReaderConfiguration(), changes);
    }

    template <class T, typename... Args>
    T& addMapperAndConfigure(Args... args) {
        T& mapper = mDevice->addMapper<T>(EVENTHUB_ID, args...);
        configureDevice(0);
        mDevice->reset(ARBITRARY_TIME);
        return mapper;
    }

    void setDisplayInfoAndReconfigure(int32_t displayId, int32_t width, int32_t height,
            int32_t orientation, const std::string& uniqueId,
            std::optional<uint8_t> physicalPort, ViewportType viewportType) {
        mFakePolicy->addDisplayViewport(
                displayId, width, height, orientation, uniqueId, physicalPort, viewportType);
        configureDevice(InputReaderConfiguration::CHANGE_DISPLAY_INFO);
    }

    void clearViewports() {
        mFakePolicy->clearViewports();
    }

    static void process(InputMapper& mapper, nsecs_t when, int32_t type, int32_t code,
                        int32_t value) {
        RawEvent event;
        event.when = when;
        event.deviceId = mapper.getDeviceContext().getEventHubId();
        event.type = type;
        event.code = code;
        event.value = value;
        mapper.process(&event);
    }

    static void assertMotionRange(const InputDeviceInfo& info,
            int32_t axis, uint32_t source, float min, float max, float flat, float fuzz) {
        const InputDeviceInfo::MotionRange* range = info.getMotionRange(axis, source);
        ASSERT_TRUE(range != nullptr) << "Axis: " << axis << " Source: " << source;
        ASSERT_EQ(axis, range->axis) << "Axis: " << axis << " Source: " << source;
        ASSERT_EQ(source, range->source) << "Axis: " << axis << " Source: " << source;
        ASSERT_NEAR(min, range->min, EPSILON) << "Axis: " << axis << " Source: " << source;
        ASSERT_NEAR(max, range->max, EPSILON) << "Axis: " << axis << " Source: " << source;
        ASSERT_NEAR(flat, range->flat, EPSILON) << "Axis: " << axis << " Source: " << source;
        ASSERT_NEAR(fuzz, range->fuzz, EPSILON) << "Axis: " << axis << " Source: " << source;
    }

    static void assertPointerCoords(const PointerCoords& coords,
            float x, float y, float pressure, float size,
            float touchMajor, float touchMinor, float toolMajor, float toolMinor,
            float orientation, float distance) {
        ASSERT_NEAR(x, coords.getAxisValue(AMOTION_EVENT_AXIS_X), 1);
        ASSERT_NEAR(y, coords.getAxisValue(AMOTION_EVENT_AXIS_Y), 1);
        ASSERT_NEAR(pressure, coords.getAxisValue(AMOTION_EVENT_AXIS_PRESSURE), EPSILON);
        ASSERT_NEAR(size, coords.getAxisValue(AMOTION_EVENT_AXIS_SIZE), EPSILON);
        ASSERT_NEAR(touchMajor, coords.getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR), 1);
        ASSERT_NEAR(touchMinor, coords.getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR), 1);
        ASSERT_NEAR(toolMajor, coords.getAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR), 1);
        ASSERT_NEAR(toolMinor, coords.getAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR), 1);
        ASSERT_NEAR(orientation, coords.getAxisValue(AMOTION_EVENT_AXIS_ORIENTATION), EPSILON);
        ASSERT_NEAR(distance, coords.getAxisValue(AMOTION_EVENT_AXIS_DISTANCE), EPSILON);
    }

    static void assertPosition(const sp<FakePointerController>& controller, float x, float y) {
        float actualX, actualY;
        controller->getPosition(&actualX, &actualY);
        ASSERT_NEAR(x, actualX, 1);
        ASSERT_NEAR(y, actualY, 1);
    }
};

const char* InputMapperTest::DEVICE_NAME = "device";
const char* InputMapperTest::DEVICE_LOCATION = "USB1";
const int32_t InputMapperTest::DEVICE_ID = END_RESERVED_ID + 1000;
const int32_t InputMapperTest::DEVICE_GENERATION = 2;
const int32_t InputMapperTest::DEVICE_CONTROLLER_NUMBER = 0;
const uint32_t InputMapperTest::DEVICE_CLASSES = 0; // not needed for current tests
const int32_t InputMapperTest::EVENTHUB_ID = 1;

// --- SwitchInputMapperTest ---

class SwitchInputMapperTest : public InputMapperTest {
protected:
};

TEST_F(SwitchInputMapperTest, GetSources) {
    SwitchInputMapper& mapper = addMapperAndConfigure<SwitchInputMapper>();

    ASSERT_EQ(uint32_t(AINPUT_SOURCE_SWITCH), mapper.getSources());
}

TEST_F(SwitchInputMapperTest, GetSwitchState) {
    SwitchInputMapper& mapper = addMapperAndConfigure<SwitchInputMapper>();

    mFakeEventHub->setSwitchState(EVENTHUB_ID, SW_LID, 1);
    ASSERT_EQ(1, mapper.getSwitchState(AINPUT_SOURCE_ANY, SW_LID));

    mFakeEventHub->setSwitchState(EVENTHUB_ID, SW_LID, 0);
    ASSERT_EQ(0, mapper.getSwitchState(AINPUT_SOURCE_ANY, SW_LID));
}

TEST_F(SwitchInputMapperTest, Process) {
    SwitchInputMapper& mapper = addMapperAndConfigure<SwitchInputMapper>();

    process(mapper, ARBITRARY_TIME, EV_SW, SW_LID, 1);
    process(mapper, ARBITRARY_TIME, EV_SW, SW_JACK_PHYSICAL_INSERT, 1);
    process(mapper, ARBITRARY_TIME, EV_SW, SW_HEADPHONE_INSERT, 0);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);

    NotifySwitchArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifySwitchWasCalled(&args));
    ASSERT_EQ(ARBITRARY_TIME, args.eventTime);
    ASSERT_EQ((1U << SW_LID) | (1U << SW_JACK_PHYSICAL_INSERT), args.switchValues);
    ASSERT_EQ((1U << SW_LID) | (1U << SW_JACK_PHYSICAL_INSERT) | (1 << SW_HEADPHONE_INSERT),
            args.switchMask);
    ASSERT_EQ(uint32_t(0), args.policyFlags);
}


// --- KeyboardInputMapperTest ---

class KeyboardInputMapperTest : public InputMapperTest {
protected:
    const std::string UNIQUE_ID = "local:0";

    void prepareDisplay(int32_t orientation);

    void testDPadKeyRotation(KeyboardInputMapper& mapper, int32_t originalScanCode,
                             int32_t originalKeyCode, int32_t rotatedKeyCode,
                             int32_t displayId = ADISPLAY_ID_NONE);
};

/* Similar to setDisplayInfoAndReconfigure, but pre-populates all parameters except for the
 * orientation.
 */
void KeyboardInputMapperTest::prepareDisplay(int32_t orientation) {
    setDisplayInfoAndReconfigure(DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            orientation, UNIQUE_ID, NO_PORT, ViewportType::VIEWPORT_INTERNAL);
}

void KeyboardInputMapperTest::testDPadKeyRotation(KeyboardInputMapper& mapper,
                                                  int32_t originalScanCode, int32_t originalKeyCode,
                                                  int32_t rotatedKeyCode, int32_t displayId) {
    NotifyKeyArgs args;

    process(mapper, ARBITRARY_TIME, EV_KEY, originalScanCode, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, args.action);
    ASSERT_EQ(originalScanCode, args.scanCode);
    ASSERT_EQ(rotatedKeyCode, args.keyCode);
    ASSERT_EQ(displayId, args.displayId);

    process(mapper, ARBITRARY_TIME, EV_KEY, originalScanCode, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, args.action);
    ASSERT_EQ(originalScanCode, args.scanCode);
    ASSERT_EQ(rotatedKeyCode, args.keyCode);
    ASSERT_EQ(displayId, args.displayId);
}

TEST_F(KeyboardInputMapperTest, GetSources) {
    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, mapper.getSources());
}

TEST_F(KeyboardInputMapperTest, Process_SimpleKeyPress) {
    const int32_t USAGE_A = 0x070004;
    const int32_t USAGE_UNKNOWN = 0x07ffff;
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_HOME, 0, AKEYCODE_HOME, POLICY_FLAG_WAKE);
    mFakeEventHub->addKey(EVENTHUB_ID, 0, USAGE_A, AKEYCODE_A, POLICY_FLAG_WAKE);

    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    // Key down by scan code.
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_HOME, 1);
    NotifyKeyArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, args.source);
    ASSERT_EQ(ARBITRARY_TIME, args.eventTime);
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, args.action);
    ASSERT_EQ(AKEYCODE_HOME, args.keyCode);
    ASSERT_EQ(KEY_HOME, args.scanCode);
    ASSERT_EQ(AMETA_NONE, args.metaState);
    ASSERT_EQ(AKEY_EVENT_FLAG_FROM_SYSTEM, args.flags);
    ASSERT_EQ(POLICY_FLAG_WAKE, args.policyFlags);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);

    // Key up by scan code.
    process(mapper, ARBITRARY_TIME + 1, EV_KEY, KEY_HOME, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, args.source);
    ASSERT_EQ(ARBITRARY_TIME + 1, args.eventTime);
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, args.action);
    ASSERT_EQ(AKEYCODE_HOME, args.keyCode);
    ASSERT_EQ(KEY_HOME, args.scanCode);
    ASSERT_EQ(AMETA_NONE, args.metaState);
    ASSERT_EQ(AKEY_EVENT_FLAG_FROM_SYSTEM, args.flags);
    ASSERT_EQ(POLICY_FLAG_WAKE, args.policyFlags);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);

    // Key down by usage code.
    process(mapper, ARBITRARY_TIME, EV_MSC, MSC_SCAN, USAGE_A);
    process(mapper, ARBITRARY_TIME, EV_KEY, 0, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, args.source);
    ASSERT_EQ(ARBITRARY_TIME, args.eventTime);
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, args.action);
    ASSERT_EQ(AKEYCODE_A, args.keyCode);
    ASSERT_EQ(0, args.scanCode);
    ASSERT_EQ(AMETA_NONE, args.metaState);
    ASSERT_EQ(AKEY_EVENT_FLAG_FROM_SYSTEM, args.flags);
    ASSERT_EQ(POLICY_FLAG_WAKE, args.policyFlags);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);

    // Key up by usage code.
    process(mapper, ARBITRARY_TIME, EV_MSC, MSC_SCAN, USAGE_A);
    process(mapper, ARBITRARY_TIME + 1, EV_KEY, 0, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, args.source);
    ASSERT_EQ(ARBITRARY_TIME + 1, args.eventTime);
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, args.action);
    ASSERT_EQ(AKEYCODE_A, args.keyCode);
    ASSERT_EQ(0, args.scanCode);
    ASSERT_EQ(AMETA_NONE, args.metaState);
    ASSERT_EQ(AKEY_EVENT_FLAG_FROM_SYSTEM, args.flags);
    ASSERT_EQ(POLICY_FLAG_WAKE, args.policyFlags);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);

    // Key down with unknown scan code or usage code.
    process(mapper, ARBITRARY_TIME, EV_MSC, MSC_SCAN, USAGE_UNKNOWN);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_UNKNOWN, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, args.source);
    ASSERT_EQ(ARBITRARY_TIME, args.eventTime);
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, args.action);
    ASSERT_EQ(0, args.keyCode);
    ASSERT_EQ(KEY_UNKNOWN, args.scanCode);
    ASSERT_EQ(AMETA_NONE, args.metaState);
    ASSERT_EQ(AKEY_EVENT_FLAG_FROM_SYSTEM, args.flags);
    ASSERT_EQ(0U, args.policyFlags);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);

    // Key up with unknown scan code or usage code.
    process(mapper, ARBITRARY_TIME, EV_MSC, MSC_SCAN, USAGE_UNKNOWN);
    process(mapper, ARBITRARY_TIME + 1, EV_KEY, KEY_UNKNOWN, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, args.source);
    ASSERT_EQ(ARBITRARY_TIME + 1, args.eventTime);
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, args.action);
    ASSERT_EQ(0, args.keyCode);
    ASSERT_EQ(KEY_UNKNOWN, args.scanCode);
    ASSERT_EQ(AMETA_NONE, args.metaState);
    ASSERT_EQ(AKEY_EVENT_FLAG_FROM_SYSTEM, args.flags);
    ASSERT_EQ(0U, args.policyFlags);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);
}

TEST_F(KeyboardInputMapperTest, Process_ShouldUpdateMetaState) {
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_LEFTSHIFT, 0, AKEYCODE_SHIFT_LEFT, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_A, 0, AKEYCODE_A, 0);

    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    // Initial metastate.
    ASSERT_EQ(AMETA_NONE, mapper.getMetaState());

    // Metakey down.
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_LEFTSHIFT, 1);
    NotifyKeyArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, args.metaState);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, mapper.getMetaState());
    ASSERT_NO_FATAL_FAILURE(mFakeContext->assertUpdateGlobalMetaStateWasCalled());

    // Key down.
    process(mapper, ARBITRARY_TIME + 1, EV_KEY, KEY_A, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, args.metaState);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, mapper.getMetaState());

    // Key up.
    process(mapper, ARBITRARY_TIME + 2, EV_KEY, KEY_A, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, args.metaState);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, mapper.getMetaState());

    // Metakey up.
    process(mapper, ARBITRARY_TIME + 3, EV_KEY, KEY_LEFTSHIFT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(AMETA_NONE, args.metaState);
    ASSERT_EQ(AMETA_NONE, mapper.getMetaState());
    ASSERT_NO_FATAL_FAILURE(mFakeContext->assertUpdateGlobalMetaStateWasCalled());
}

TEST_F(KeyboardInputMapperTest, Process_WhenNotOrientationAware_ShouldNotRotateDPad) {
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_UP, 0, AKEYCODE_DPAD_UP, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_RIGHT, 0, AKEYCODE_DPAD_RIGHT, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_DOWN, 0, AKEYCODE_DPAD_DOWN, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_LEFT, 0, AKEYCODE_DPAD_LEFT, 0);

    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    prepareDisplay(DISPLAY_ORIENTATION_90);
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper,
            KEY_UP, AKEYCODE_DPAD_UP, AKEYCODE_DPAD_UP));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper,
            KEY_RIGHT, AKEYCODE_DPAD_RIGHT, AKEYCODE_DPAD_RIGHT));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper,
            KEY_DOWN, AKEYCODE_DPAD_DOWN, AKEYCODE_DPAD_DOWN));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper,
            KEY_LEFT, AKEYCODE_DPAD_LEFT, AKEYCODE_DPAD_LEFT));
}

TEST_F(KeyboardInputMapperTest, Process_WhenOrientationAware_ShouldRotateDPad) {
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_UP, 0, AKEYCODE_DPAD_UP, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_RIGHT, 0, AKEYCODE_DPAD_RIGHT, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_DOWN, 0, AKEYCODE_DPAD_DOWN, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_LEFT, 0, AKEYCODE_DPAD_LEFT, 0);

    addConfigurationProperty("keyboard.orientationAware", "1");
    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    prepareDisplay(DISPLAY_ORIENTATION_0);
    ASSERT_NO_FATAL_FAILURE(
            testDPadKeyRotation(mapper, KEY_UP, AKEYCODE_DPAD_UP, AKEYCODE_DPAD_UP, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_RIGHT, AKEYCODE_DPAD_RIGHT,
                                                AKEYCODE_DPAD_RIGHT, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_DOWN, AKEYCODE_DPAD_DOWN,
                                                AKEYCODE_DPAD_DOWN, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_LEFT, AKEYCODE_DPAD_LEFT,
                                                AKEYCODE_DPAD_LEFT, DISPLAY_ID));

    clearViewports();
    prepareDisplay(DISPLAY_ORIENTATION_90);
    ASSERT_NO_FATAL_FAILURE(
            testDPadKeyRotation(mapper, KEY_UP, AKEYCODE_DPAD_UP, AKEYCODE_DPAD_LEFT, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_RIGHT, AKEYCODE_DPAD_RIGHT,
                                                AKEYCODE_DPAD_UP, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_DOWN, AKEYCODE_DPAD_DOWN,
                                                AKEYCODE_DPAD_RIGHT, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_LEFT, AKEYCODE_DPAD_LEFT,
                                                AKEYCODE_DPAD_DOWN, DISPLAY_ID));

    clearViewports();
    prepareDisplay(DISPLAY_ORIENTATION_180);
    ASSERT_NO_FATAL_FAILURE(
            testDPadKeyRotation(mapper, KEY_UP, AKEYCODE_DPAD_UP, AKEYCODE_DPAD_DOWN, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_RIGHT, AKEYCODE_DPAD_RIGHT,
                                                AKEYCODE_DPAD_LEFT, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_DOWN, AKEYCODE_DPAD_DOWN,
                                                AKEYCODE_DPAD_UP, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_LEFT, AKEYCODE_DPAD_LEFT,
                                                AKEYCODE_DPAD_RIGHT, DISPLAY_ID));

    clearViewports();
    prepareDisplay(DISPLAY_ORIENTATION_270);
    ASSERT_NO_FATAL_FAILURE(
            testDPadKeyRotation(mapper, KEY_UP, AKEYCODE_DPAD_UP, AKEYCODE_DPAD_RIGHT, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_RIGHT, AKEYCODE_DPAD_RIGHT,
                                                AKEYCODE_DPAD_DOWN, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_DOWN, AKEYCODE_DPAD_DOWN,
                                                AKEYCODE_DPAD_LEFT, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_LEFT, AKEYCODE_DPAD_LEFT,
                                                AKEYCODE_DPAD_UP, DISPLAY_ID));

    // Special case: if orientation changes while key is down, we still emit the same keycode
    // in the key up as we did in the key down.
    NotifyKeyArgs args;
    clearViewports();
    prepareDisplay(DISPLAY_ORIENTATION_270);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_UP, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, args.action);
    ASSERT_EQ(KEY_UP, args.scanCode);
    ASSERT_EQ(AKEYCODE_DPAD_RIGHT, args.keyCode);

    clearViewports();
    prepareDisplay(DISPLAY_ORIENTATION_180);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_UP, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, args.action);
    ASSERT_EQ(KEY_UP, args.scanCode);
    ASSERT_EQ(AKEYCODE_DPAD_RIGHT, args.keyCode);
}

TEST_F(KeyboardInputMapperTest, DisplayIdConfigurationChange_NotOrientationAware) {
    // If the keyboard is not orientation aware,
    // key events should not be associated with a specific display id
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_UP, 0, AKEYCODE_DPAD_UP, 0);

    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);
    NotifyKeyArgs args;

    // Display id should be ADISPLAY_ID_NONE without any display configuration.
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_UP, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_UP, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(ADISPLAY_ID_NONE, args.displayId);

    prepareDisplay(DISPLAY_ORIENTATION_0);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_UP, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_UP, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(ADISPLAY_ID_NONE, args.displayId);
}

TEST_F(KeyboardInputMapperTest, DisplayIdConfigurationChange_OrientationAware) {
    // If the keyboard is orientation aware,
    // key events should be associated with the internal viewport
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_UP, 0, AKEYCODE_DPAD_UP, 0);

    addConfigurationProperty("keyboard.orientationAware", "1");
    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);
    NotifyKeyArgs args;

    // Display id should be ADISPLAY_ID_NONE without any display configuration.
    // ^--- already checked by the previous test

    setDisplayInfoAndReconfigure(DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_ORIENTATION_0,
            UNIQUE_ID, NO_PORT, ViewportType::VIEWPORT_INTERNAL);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_UP, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_UP, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(DISPLAY_ID, args.displayId);

    constexpr int32_t newDisplayId = 2;
    clearViewports();
    setDisplayInfoAndReconfigure(newDisplayId, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_ORIENTATION_0,
            UNIQUE_ID, NO_PORT, ViewportType::VIEWPORT_INTERNAL);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_UP, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_UP, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(newDisplayId, args.displayId);
}

TEST_F(KeyboardInputMapperTest, GetKeyCodeState) {
    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    mFakeEventHub->setKeyCodeState(EVENTHUB_ID, AKEYCODE_A, 1);
    ASSERT_EQ(1, mapper.getKeyCodeState(AINPUT_SOURCE_ANY, AKEYCODE_A));

    mFakeEventHub->setKeyCodeState(EVENTHUB_ID, AKEYCODE_A, 0);
    ASSERT_EQ(0, mapper.getKeyCodeState(AINPUT_SOURCE_ANY, AKEYCODE_A));
}

TEST_F(KeyboardInputMapperTest, GetScanCodeState) {
    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    mFakeEventHub->setScanCodeState(EVENTHUB_ID, KEY_A, 1);
    ASSERT_EQ(1, mapper.getScanCodeState(AINPUT_SOURCE_ANY, KEY_A));

    mFakeEventHub->setScanCodeState(EVENTHUB_ID, KEY_A, 0);
    ASSERT_EQ(0, mapper.getScanCodeState(AINPUT_SOURCE_ANY, KEY_A));
}

TEST_F(KeyboardInputMapperTest, MarkSupportedKeyCodes) {
    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    mFakeEventHub->addKey(EVENTHUB_ID, KEY_A, 0, AKEYCODE_A, 0);

    const int32_t keyCodes[2] = { AKEYCODE_A, AKEYCODE_B };
    uint8_t flags[2] = { 0, 0 };
    ASSERT_TRUE(mapper.markSupportedKeyCodes(AINPUT_SOURCE_ANY, 1, keyCodes, flags));
    ASSERT_TRUE(flags[0]);
    ASSERT_FALSE(flags[1]);
}

TEST_F(KeyboardInputMapperTest, Process_LockedKeysShouldToggleMetaStateAndLeds) {
    mFakeEventHub->addLed(EVENTHUB_ID, LED_CAPSL, true /*initially on*/);
    mFakeEventHub->addLed(EVENTHUB_ID, LED_NUML, false /*initially off*/);
    mFakeEventHub->addLed(EVENTHUB_ID, LED_SCROLLL, false /*initially off*/);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_CAPSLOCK, 0, AKEYCODE_CAPS_LOCK, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_NUMLOCK, 0, AKEYCODE_NUM_LOCK, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_SCROLLLOCK, 0, AKEYCODE_SCROLL_LOCK, 0);

    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    // Initialization should have turned all of the lights off.
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_CAPSL));
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_NUML));
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_SCROLLL));

    // Toggle caps lock on.
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_CAPSLOCK, 1);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_CAPSLOCK, 0);
    ASSERT_TRUE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_CAPSL));
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_NUML));
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_SCROLLL));
    ASSERT_EQ(AMETA_CAPS_LOCK_ON, mapper.getMetaState());

    // Toggle num lock on.
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_NUMLOCK, 1);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_NUMLOCK, 0);
    ASSERT_TRUE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_CAPSL));
    ASSERT_TRUE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_NUML));
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_SCROLLL));
    ASSERT_EQ(AMETA_CAPS_LOCK_ON | AMETA_NUM_LOCK_ON, mapper.getMetaState());

    // Toggle caps lock off.
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_CAPSLOCK, 1);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_CAPSLOCK, 0);
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_CAPSL));
    ASSERT_TRUE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_NUML));
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_SCROLLL));
    ASSERT_EQ(AMETA_NUM_LOCK_ON, mapper.getMetaState());

    // Toggle scroll lock on.
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_SCROLLLOCK, 1);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_SCROLLLOCK, 0);
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_CAPSL));
    ASSERT_TRUE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_NUML));
    ASSERT_TRUE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_SCROLLL));
    ASSERT_EQ(AMETA_NUM_LOCK_ON | AMETA_SCROLL_LOCK_ON, mapper.getMetaState());

    // Toggle num lock off.
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_NUMLOCK, 1);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_NUMLOCK, 0);
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_CAPSL));
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_NUML));
    ASSERT_TRUE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_SCROLLL));
    ASSERT_EQ(AMETA_SCROLL_LOCK_ON, mapper.getMetaState());

    // Toggle scroll lock off.
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_SCROLLLOCK, 1);
    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_SCROLLLOCK, 0);
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_CAPSL));
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_NUML));
    ASSERT_FALSE(mFakeEventHub->getLedState(EVENTHUB_ID, LED_SCROLLL));
    ASSERT_EQ(AMETA_NONE, mapper.getMetaState());
}

TEST_F(KeyboardInputMapperTest, Configure_AssignsDisplayPort) {
    // keyboard 1.
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_UP, 0, AKEYCODE_DPAD_UP, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_RIGHT, 0, AKEYCODE_DPAD_RIGHT, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_DOWN, 0, AKEYCODE_DPAD_DOWN, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_LEFT, 0, AKEYCODE_DPAD_LEFT, 0);

    // keyboard 2.
    const std::string USB2 = "USB2";
    constexpr int32_t SECOND_DEVICE_ID = DEVICE_ID + 1;
    constexpr int32_t SECOND_EVENTHUB_ID = EVENTHUB_ID + 1;
    InputDeviceIdentifier identifier;
    identifier.name = "KEYBOARD2";
    identifier.location = USB2;
    std::unique_ptr<InputDevice> device2 =
            std::make_unique<InputDevice>(mFakeContext, SECOND_DEVICE_ID, DEVICE_GENERATION,
                                          identifier);
    mFakeEventHub->addDevice(SECOND_EVENTHUB_ID, DEVICE_NAME, 0 /*classes*/);
    mFakeEventHub->addKey(SECOND_EVENTHUB_ID, KEY_UP, 0, AKEYCODE_DPAD_UP, 0);
    mFakeEventHub->addKey(SECOND_EVENTHUB_ID, KEY_RIGHT, 0, AKEYCODE_DPAD_RIGHT, 0);
    mFakeEventHub->addKey(SECOND_EVENTHUB_ID, KEY_DOWN, 0, AKEYCODE_DPAD_DOWN, 0);
    mFakeEventHub->addKey(SECOND_EVENTHUB_ID, KEY_LEFT, 0, AKEYCODE_DPAD_LEFT, 0);

    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    KeyboardInputMapper& mapper2 =
            device2->addMapper<KeyboardInputMapper>(SECOND_EVENTHUB_ID, AINPUT_SOURCE_KEYBOARD,
                                                    AINPUT_KEYBOARD_TYPE_ALPHABETIC);
    device2->configure(ARBITRARY_TIME, mFakePolicy->getReaderConfiguration(), 0 /*changes*/);
    device2->reset(ARBITRARY_TIME);

    // Prepared displays and associated info.
    constexpr uint8_t hdmi1 = 0;
    constexpr uint8_t hdmi2 = 1;
    const std::string SECONDARY_UNIQUE_ID = "local:1";

    mFakePolicy->addInputPortAssociation(DEVICE_LOCATION, hdmi1);
    mFakePolicy->addInputPortAssociation(USB2, hdmi2);

    // No associated display viewport found, should disable the device.
    device2->configure(ARBITRARY_TIME, mFakePolicy->getReaderConfiguration(),
                       InputReaderConfiguration::CHANGE_DISPLAY_INFO);
    ASSERT_FALSE(device2->isEnabled());

    // Prepare second display.
    constexpr int32_t newDisplayId = 2;
    setDisplayInfoAndReconfigure(DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_ORIENTATION_0,
                                 UNIQUE_ID, hdmi1, ViewportType::VIEWPORT_INTERNAL);
    setDisplayInfoAndReconfigure(newDisplayId, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_ORIENTATION_0,
                                 SECONDARY_UNIQUE_ID, hdmi2, ViewportType::VIEWPORT_EXTERNAL);
    // Default device will reconfigure above, need additional reconfiguration for another device.
    device2->configure(ARBITRARY_TIME, mFakePolicy->getReaderConfiguration(),
                       InputReaderConfiguration::CHANGE_DISPLAY_INFO);

    // Device should be enabled after the associated display is found.
    ASSERT_TRUE(mDevice->isEnabled());
    ASSERT_TRUE(device2->isEnabled());

    // Test pad key events
    ASSERT_NO_FATAL_FAILURE(
            testDPadKeyRotation(mapper, KEY_UP, AKEYCODE_DPAD_UP, AKEYCODE_DPAD_UP, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_RIGHT, AKEYCODE_DPAD_RIGHT,
                                                AKEYCODE_DPAD_RIGHT, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_DOWN, AKEYCODE_DPAD_DOWN,
                                                AKEYCODE_DPAD_DOWN, DISPLAY_ID));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper, KEY_LEFT, AKEYCODE_DPAD_LEFT,
                                                AKEYCODE_DPAD_LEFT, DISPLAY_ID));

    ASSERT_NO_FATAL_FAILURE(
            testDPadKeyRotation(mapper2, KEY_UP, AKEYCODE_DPAD_UP, AKEYCODE_DPAD_UP, newDisplayId));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper2, KEY_RIGHT, AKEYCODE_DPAD_RIGHT,
                                                AKEYCODE_DPAD_RIGHT, newDisplayId));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper2, KEY_DOWN, AKEYCODE_DPAD_DOWN,
                                                AKEYCODE_DPAD_DOWN, newDisplayId));
    ASSERT_NO_FATAL_FAILURE(testDPadKeyRotation(mapper2, KEY_LEFT, AKEYCODE_DPAD_LEFT,
                                                AKEYCODE_DPAD_LEFT, newDisplayId));
}

// --- KeyboardInputMapperTest_ExternalDevice ---

class KeyboardInputMapperTest_ExternalDevice : public InputMapperTest {
protected:
    virtual void SetUp() override {
        InputMapperTest::SetUp(DEVICE_CLASSES | INPUT_DEVICE_CLASS_EXTERNAL);
    }
};

TEST_F(KeyboardInputMapperTest_ExternalDevice, WakeBehavior) {
    // For external devices, non-media keys will trigger wake on key down. Media keys need to be
    // marked as WAKE in the keylayout file to trigger wake.

    mFakeEventHub->addKey(EVENTHUB_ID, KEY_HOME, 0, AKEYCODE_HOME, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_PLAY, 0, AKEYCODE_MEDIA_PLAY, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_PLAYPAUSE, 0, AKEYCODE_MEDIA_PLAY_PAUSE,
                          POLICY_FLAG_WAKE);

    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_HOME, 1);
    NotifyKeyArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(POLICY_FLAG_WAKE, args.policyFlags);

    process(mapper, ARBITRARY_TIME + 1, EV_KEY, KEY_HOME, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(uint32_t(0), args.policyFlags);

    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_PLAY, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(uint32_t(0), args.policyFlags);

    process(mapper, ARBITRARY_TIME + 1, EV_KEY, KEY_PLAY, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(uint32_t(0), args.policyFlags);

    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_PLAYPAUSE, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(POLICY_FLAG_WAKE, args.policyFlags);

    process(mapper, ARBITRARY_TIME + 1, EV_KEY, KEY_PLAYPAUSE, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(POLICY_FLAG_WAKE, args.policyFlags);
}

TEST_F(KeyboardInputMapperTest_ExternalDevice, DoNotWakeByDefaultBehavior) {
    // Tv Remote key's wake behavior is prescribed by the keylayout file.

    mFakeEventHub->addKey(EVENTHUB_ID, KEY_HOME, 0, AKEYCODE_HOME, POLICY_FLAG_WAKE);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_DOWN, 0, AKEYCODE_DPAD_DOWN, 0);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_PLAY, 0, AKEYCODE_MEDIA_PLAY, POLICY_FLAG_WAKE);

    addConfigurationProperty("keyboard.doNotWakeByDefault", "1");
    KeyboardInputMapper& mapper =
            addMapperAndConfigure<KeyboardInputMapper>(AINPUT_SOURCE_KEYBOARD,
                                                       AINPUT_KEYBOARD_TYPE_ALPHABETIC);

    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_HOME, 1);
    NotifyKeyArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(POLICY_FLAG_WAKE, args.policyFlags);

    process(mapper, ARBITRARY_TIME + 1, EV_KEY, KEY_HOME, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(POLICY_FLAG_WAKE, args.policyFlags);

    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_DOWN, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(uint32_t(0), args.policyFlags);

    process(mapper, ARBITRARY_TIME + 1, EV_KEY, KEY_DOWN, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(uint32_t(0), args.policyFlags);

    process(mapper, ARBITRARY_TIME, EV_KEY, KEY_PLAY, 1);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(POLICY_FLAG_WAKE, args.policyFlags);

    process(mapper, ARBITRARY_TIME + 1, EV_KEY, KEY_PLAY, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(POLICY_FLAG_WAKE, args.policyFlags);
}

// --- CursorInputMapperTest ---

class CursorInputMapperTest : public InputMapperTest {
protected:
    static const int32_t TRACKBALL_MOVEMENT_THRESHOLD;

    sp<FakePointerController> mFakePointerController;

    virtual void SetUp() override {
        InputMapperTest::SetUp();

        mFakePointerController = new FakePointerController();
        mFakePolicy->setPointerController(mDevice->getId(), mFakePointerController);
    }

    void testMotionRotation(CursorInputMapper& mapper, int32_t originalX, int32_t originalY,
                            int32_t rotatedX, int32_t rotatedY);

    void prepareDisplay(int32_t orientation) {
        const std::string uniqueId = "local:0";
        const ViewportType viewportType = ViewportType::VIEWPORT_INTERNAL;
        setDisplayInfoAndReconfigure(DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                orientation, uniqueId, NO_PORT, viewportType);
    }
};

const int32_t CursorInputMapperTest::TRACKBALL_MOVEMENT_THRESHOLD = 6;

void CursorInputMapperTest::testMotionRotation(CursorInputMapper& mapper, int32_t originalX,
                                               int32_t originalY, int32_t rotatedX,
                                               int32_t rotatedY) {
    NotifyMotionArgs args;

    process(mapper, ARBITRARY_TIME, EV_REL, REL_X, originalX);
    process(mapper, ARBITRARY_TIME, EV_REL, REL_Y, originalY);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            float(rotatedX) / TRACKBALL_MOVEMENT_THRESHOLD,
            float(rotatedY) / TRACKBALL_MOVEMENT_THRESHOLD,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
}

TEST_F(CursorInputMapperTest, WhenModeIsPointer_GetSources_ReturnsMouse) {
    addConfigurationProperty("cursor.mode", "pointer");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    ASSERT_EQ(AINPUT_SOURCE_MOUSE, mapper.getSources());
}

TEST_F(CursorInputMapperTest, WhenModeIsNavigation_GetSources_ReturnsTrackball) {
    addConfigurationProperty("cursor.mode", "navigation");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    ASSERT_EQ(AINPUT_SOURCE_TRACKBALL, mapper.getSources());
}

TEST_F(CursorInputMapperTest, WhenModeIsPointer_PopulateDeviceInfo_ReturnsRangeFromPointerController) {
    addConfigurationProperty("cursor.mode", "pointer");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    InputDeviceInfo info;
    mapper.populateDeviceInfo(&info);

    // Initially there may not be a valid motion range.
    ASSERT_EQ(nullptr, info.getMotionRange(AINPUT_MOTION_RANGE_X, AINPUT_SOURCE_MOUSE));
    ASSERT_EQ(nullptr, info.getMotionRange(AINPUT_MOTION_RANGE_Y, AINPUT_SOURCE_MOUSE));
    ASSERT_NO_FATAL_FAILURE(assertMotionRange(info,
            AINPUT_MOTION_RANGE_PRESSURE, AINPUT_SOURCE_MOUSE, 0.0f, 1.0f, 0.0f, 0.0f));

    // When the bounds are set, then there should be a valid motion range.
    mFakePointerController->setBounds(1, 2, 800 - 1, 480 - 1);

    InputDeviceInfo info2;
    mapper.populateDeviceInfo(&info2);

    ASSERT_NO_FATAL_FAILURE(assertMotionRange(info2,
            AINPUT_MOTION_RANGE_X, AINPUT_SOURCE_MOUSE,
            1, 800 - 1, 0.0f, 0.0f));
    ASSERT_NO_FATAL_FAILURE(assertMotionRange(info2,
            AINPUT_MOTION_RANGE_Y, AINPUT_SOURCE_MOUSE,
            2, 480 - 1, 0.0f, 0.0f));
    ASSERT_NO_FATAL_FAILURE(assertMotionRange(info2,
            AINPUT_MOTION_RANGE_PRESSURE, AINPUT_SOURCE_MOUSE,
            0.0f, 1.0f, 0.0f, 0.0f));
}

TEST_F(CursorInputMapperTest, WhenModeIsNavigation_PopulateDeviceInfo_ReturnsScaledRange) {
    addConfigurationProperty("cursor.mode", "navigation");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    InputDeviceInfo info;
    mapper.populateDeviceInfo(&info);

    ASSERT_NO_FATAL_FAILURE(assertMotionRange(info,
            AINPUT_MOTION_RANGE_X, AINPUT_SOURCE_TRACKBALL,
            -1.0f, 1.0f, 0.0f, 1.0f / TRACKBALL_MOVEMENT_THRESHOLD));
    ASSERT_NO_FATAL_FAILURE(assertMotionRange(info,
            AINPUT_MOTION_RANGE_Y, AINPUT_SOURCE_TRACKBALL,
            -1.0f, 1.0f, 0.0f, 1.0f / TRACKBALL_MOVEMENT_THRESHOLD));
    ASSERT_NO_FATAL_FAILURE(assertMotionRange(info,
            AINPUT_MOTION_RANGE_PRESSURE, AINPUT_SOURCE_TRACKBALL,
            0.0f, 1.0f, 0.0f, 0.0f));
}

TEST_F(CursorInputMapperTest, Process_ShouldSetAllFieldsAndIncludeGlobalMetaState) {
    addConfigurationProperty("cursor.mode", "navigation");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    mFakeContext->setGlobalMetaState(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON);

    NotifyMotionArgs args;

    // Button press.
    // Mostly testing non x/y behavior here so we don't need to check again elsewhere.
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_MOUSE, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(ARBITRARY_TIME, args.eventTime);
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TRACKBALL, args.source);
    ASSERT_EQ(uint32_t(0), args.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, args.action);
    ASSERT_EQ(0, args.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, args.metaState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_PRIMARY, args.buttonState);
    ASSERT_EQ(0, args.edgeFlags);
    ASSERT_EQ(uint32_t(1), args.pointerCount);
    ASSERT_EQ(0, args.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_MOUSE, args.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_EQ(TRACKBALL_MOVEMENT_THRESHOLD, args.xPrecision);
    ASSERT_EQ(TRACKBALL_MOVEMENT_THRESHOLD, args.yPrecision);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(ARBITRARY_TIME, args.eventTime);
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TRACKBALL, args.source);
    ASSERT_EQ(uint32_t(0), args.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, args.action);
    ASSERT_EQ(0, args.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, args.metaState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_PRIMARY, args.buttonState);
    ASSERT_EQ(0, args.edgeFlags);
    ASSERT_EQ(uint32_t(1), args.pointerCount);
    ASSERT_EQ(0, args.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_MOUSE, args.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_EQ(TRACKBALL_MOVEMENT_THRESHOLD, args.xPrecision);
    ASSERT_EQ(TRACKBALL_MOVEMENT_THRESHOLD, args.yPrecision);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);

    // Button release.  Should have same down time.
    process(mapper, ARBITRARY_TIME + 1, EV_KEY, BTN_MOUSE, 0);
    process(mapper, ARBITRARY_TIME + 1, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(ARBITRARY_TIME + 1, args.eventTime);
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TRACKBALL, args.source);
    ASSERT_EQ(uint32_t(0), args.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, args.action);
    ASSERT_EQ(0, args.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, args.metaState);
    ASSERT_EQ(0, args.buttonState);
    ASSERT_EQ(0, args.edgeFlags);
    ASSERT_EQ(uint32_t(1), args.pointerCount);
    ASSERT_EQ(0, args.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_MOUSE, args.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_EQ(TRACKBALL_MOVEMENT_THRESHOLD, args.xPrecision);
    ASSERT_EQ(TRACKBALL_MOVEMENT_THRESHOLD, args.yPrecision);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(ARBITRARY_TIME + 1, args.eventTime);
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TRACKBALL, args.source);
    ASSERT_EQ(uint32_t(0), args.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, args.action);
    ASSERT_EQ(0, args.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, args.metaState);
    ASSERT_EQ(0, args.buttonState);
    ASSERT_EQ(0, args.edgeFlags);
    ASSERT_EQ(uint32_t(1), args.pointerCount);
    ASSERT_EQ(0, args.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_MOUSE, args.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_EQ(TRACKBALL_MOVEMENT_THRESHOLD, args.xPrecision);
    ASSERT_EQ(TRACKBALL_MOVEMENT_THRESHOLD, args.yPrecision);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);
}

TEST_F(CursorInputMapperTest, Process_ShouldHandleIndependentXYUpdates) {
    addConfigurationProperty("cursor.mode", "navigation");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    NotifyMotionArgs args;

    // Motion in X but not Y.
    process(mapper, ARBITRARY_TIME, EV_REL, REL_X, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            1.0f / TRACKBALL_MOVEMENT_THRESHOLD, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    // Motion in Y but not X.
    process(mapper, ARBITRARY_TIME, EV_REL, REL_Y, -2);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, -2.0f / TRACKBALL_MOVEMENT_THRESHOLD, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
}

TEST_F(CursorInputMapperTest, Process_ShouldHandleIndependentButtonUpdates) {
    addConfigurationProperty("cursor.mode", "navigation");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    NotifyMotionArgs args;

    // Button press.
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_MOUSE, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    // Button release.
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_MOUSE, 0);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
}

TEST_F(CursorInputMapperTest, Process_ShouldHandleCombinedXYAndButtonUpdates) {
    addConfigurationProperty("cursor.mode", "navigation");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    NotifyMotionArgs args;

    // Combined X, Y and Button.
    process(mapper, ARBITRARY_TIME, EV_REL, REL_X, 1);
    process(mapper, ARBITRARY_TIME, EV_REL, REL_Y, -2);
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_MOUSE, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            1.0f / TRACKBALL_MOVEMENT_THRESHOLD, -2.0f / TRACKBALL_MOVEMENT_THRESHOLD,
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            1.0f / TRACKBALL_MOVEMENT_THRESHOLD, -2.0f / TRACKBALL_MOVEMENT_THRESHOLD,
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    // Move X, Y a bit while pressed.
    process(mapper, ARBITRARY_TIME, EV_REL, REL_X, 2);
    process(mapper, ARBITRARY_TIME, EV_REL, REL_Y, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            2.0f / TRACKBALL_MOVEMENT_THRESHOLD, 1.0f / TRACKBALL_MOVEMENT_THRESHOLD,
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    // Release Button.
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_MOUSE, 0);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
}

TEST_F(CursorInputMapperTest, Process_WhenNotOrientationAware_ShouldNotRotateMotions) {
    addConfigurationProperty("cursor.mode", "navigation");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    prepareDisplay(DISPLAY_ORIENTATION_90);
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  0,  1,  0,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1,  1,  1,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1,  0,  1,  0));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1, -1,  1, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  0, -1,  0, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1, -1, -1, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1,  0, -1,  0));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1,  1, -1,  1));
}

TEST_F(CursorInputMapperTest, Process_WhenOrientationAware_ShouldRotateMotions) {
    addConfigurationProperty("cursor.mode", "navigation");
    addConfigurationProperty("cursor.orientationAware", "1");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    prepareDisplay(DISPLAY_ORIENTATION_0);
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  0,  1,  0,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1,  1,  1,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1,  0,  1,  0));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1, -1,  1, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  0, -1,  0, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1, -1, -1, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1,  0, -1,  0));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1,  1, -1,  1));

    prepareDisplay(DISPLAY_ORIENTATION_90);
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  0,  1,  1,  0));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1,  1,  1, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1,  0,  0, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1, -1, -1, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  0, -1, -1,  0));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1, -1, -1,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1,  0,  0,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1,  1,  1,  1));

    prepareDisplay(DISPLAY_ORIENTATION_180);
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  0,  1,  0, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1,  1, -1, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1,  0, -1,  0));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1, -1, -1,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  0, -1,  0,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1, -1,  1,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1,  0,  1,  0));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1,  1,  1, -1));

    prepareDisplay(DISPLAY_ORIENTATION_270);
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  0,  1, -1,  0));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1,  1, -1,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1,  0,  0,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  1, -1,  1,  1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper,  0, -1,  1,  0));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1, -1,  1, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1,  0,  0, -1));
    ASSERT_NO_FATAL_FAILURE(testMotionRotation(mapper, -1,  1, -1, -1));
}

TEST_F(CursorInputMapperTest, Process_ShouldHandleAllButtons) {
    addConfigurationProperty("cursor.mode", "pointer");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    mFakePointerController->setBounds(0, 0, 800 - 1, 480 - 1);
    mFakePointerController->setPosition(100, 200);
    mFakePointerController->setButtonState(0);

    NotifyMotionArgs motionArgs;
    NotifyKeyArgs keyArgs;

    // press BTN_LEFT, release BTN_LEFT
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_LEFT, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_PRIMARY, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_PRIMARY, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_PRIMARY, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_PRIMARY, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_LEFT, 0);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    // press BTN_RIGHT + BTN_MIDDLE, release BTN_RIGHT, release BTN_MIDDLE
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_RIGHT, 1);
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_MIDDLE, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_SECONDARY | AMOTION_EVENT_BUTTON_TERTIARY,
            motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_SECONDARY | AMOTION_EVENT_BUTTON_TERTIARY,
            mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_TERTIARY, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_SECONDARY | AMOTION_EVENT_BUTTON_TERTIARY,
            mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_SECONDARY | AMOTION_EVENT_BUTTON_TERTIARY,
            motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_SECONDARY | AMOTION_EVENT_BUTTON_TERTIARY,
            mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_RIGHT, 0);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_TERTIARY, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_TERTIARY, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_TERTIARY, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_TERTIARY, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_MIDDLE, 0);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_MIDDLE, 0);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    // press BTN_BACK, release BTN_BACK
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_BACK, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_BACK, 0);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());

    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    // press BTN_SIDE, release BTN_SIDE
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_SIDE, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_SIDE, 0);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    // press BTN_FORWARD, release BTN_FORWARD
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_FORWARD, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_FORWARD, 0);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);

    // press BTN_EXTRA, release BTN_EXTRA
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_EXTRA, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_EXTRA, 0);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, mFakePointerController->getButtonState());
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            100.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);
}

TEST_F(CursorInputMapperTest, Process_WhenModeIsPointer_ShouldMoveThePointerAround) {
    addConfigurationProperty("cursor.mode", "pointer");
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    mFakePointerController->setBounds(0, 0, 800 - 1, 480 - 1);
    mFakePointerController->setPosition(100, 200);
    mFakePointerController->setButtonState(0);

    NotifyMotionArgs args;

    process(mapper, ARBITRARY_TIME, EV_REL, REL_X, 10);
    process(mapper, ARBITRARY_TIME, EV_REL, REL_Y, 20);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AINPUT_SOURCE_MOUSE, args.source);
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            110.0f, 220.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_NO_FATAL_FAILURE(assertPosition(mFakePointerController, 110.0f, 220.0f));
}

TEST_F(CursorInputMapperTest, Process_PointerCapture) {
    addConfigurationProperty("cursor.mode", "pointer");
    mFakePolicy->setPointerCapture(true);
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    NotifyDeviceResetArgs resetArgs;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    ASSERT_EQ(ARBITRARY_TIME, resetArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, resetArgs.deviceId);

    mFakePointerController->setBounds(0, 0, 800 - 1, 480 - 1);
    mFakePointerController->setPosition(100, 200);
    mFakePointerController->setButtonState(0);

    NotifyMotionArgs args;

    // Move.
    process(mapper, ARBITRARY_TIME, EV_REL, REL_X, 10);
    process(mapper, ARBITRARY_TIME, EV_REL, REL_Y, 20);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AINPUT_SOURCE_MOUSE_RELATIVE, args.source);
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            10.0f, 20.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_NO_FATAL_FAILURE(assertPosition(mFakePointerController, 100.0f, 200.0f));

    // Button press.
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_MOUSE, 1);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AINPUT_SOURCE_MOUSE_RELATIVE, args.source);
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AINPUT_SOURCE_MOUSE_RELATIVE, args.source);
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    // Button release.
    process(mapper, ARBITRARY_TIME + 2, EV_KEY, BTN_MOUSE, 0);
    process(mapper, ARBITRARY_TIME + 2, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AINPUT_SOURCE_MOUSE_RELATIVE, args.source);
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AINPUT_SOURCE_MOUSE_RELATIVE, args.source);
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

    // Another move.
    process(mapper, ARBITRARY_TIME, EV_REL, REL_X, 30);
    process(mapper, ARBITRARY_TIME, EV_REL, REL_Y, 40);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AINPUT_SOURCE_MOUSE_RELATIVE, args.source);
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            30.0f, 40.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_NO_FATAL_FAILURE(assertPosition(mFakePointerController, 100.0f, 200.0f));

    // Disable pointer capture and check that the device generation got bumped
    // and events are generated the usual way.
    const uint32_t generation = mFakeContext->getGeneration();
    mFakePolicy->setPointerCapture(false);
    configureDevice(InputReaderConfiguration::CHANGE_POINTER_CAPTURE);
    ASSERT_TRUE(mFakeContext->getGeneration() != generation);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyDeviceResetWasCalled(&resetArgs));
    ASSERT_EQ(ARBITRARY_TIME, resetArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, resetArgs.deviceId);

    process(mapper, ARBITRARY_TIME, EV_REL, REL_X, 10);
    process(mapper, ARBITRARY_TIME, EV_REL, REL_Y, 20);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AINPUT_SOURCE_MOUSE, args.source);
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            110.0f, 220.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_NO_FATAL_FAILURE(assertPosition(mFakePointerController, 110.0f, 220.0f));
}

TEST_F(CursorInputMapperTest, Process_ShouldHandleDisplayId) {
    CursorInputMapper& mapper = addMapperAndConfigure<CursorInputMapper>();

    // Setup for second display.
    constexpr int32_t SECOND_DISPLAY_ID = 1;
    const std::string SECOND_DISPLAY_UNIQUE_ID = "local:1";
    mFakePolicy->addDisplayViewport(SECOND_DISPLAY_ID, 800, 480, DISPLAY_ORIENTATION_0,
                                    SECOND_DISPLAY_UNIQUE_ID, NO_PORT,
                                    ViewportType::VIEWPORT_EXTERNAL);
    mFakePolicy->setDefaultPointerDisplayId(SECOND_DISPLAY_ID);
    configureDevice(InputReaderConfiguration::CHANGE_DISPLAY_INFO);

    mFakePointerController->setBounds(0, 0, 800 - 1, 480 - 1);
    mFakePointerController->setPosition(100, 200);
    mFakePointerController->setButtonState(0);

    NotifyMotionArgs args;
    process(mapper, ARBITRARY_TIME, EV_REL, REL_X, 10);
    process(mapper, ARBITRARY_TIME, EV_REL, REL_Y, 20);
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AINPUT_SOURCE_MOUSE, args.source);
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, args.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            110.0f, 220.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_NO_FATAL_FAILURE(assertPosition(mFakePointerController, 110.0f, 220.0f));
    ASSERT_EQ(SECOND_DISPLAY_ID, args.displayId);
}

// --- TouchInputMapperTest ---

class TouchInputMapperTest : public InputMapperTest {
protected:
    static const int32_t RAW_X_MIN;
    static const int32_t RAW_X_MAX;
    static const int32_t RAW_Y_MIN;
    static const int32_t RAW_Y_MAX;
    static const int32_t RAW_TOUCH_MIN;
    static const int32_t RAW_TOUCH_MAX;
    static const int32_t RAW_TOOL_MIN;
    static const int32_t RAW_TOOL_MAX;
    static const int32_t RAW_PRESSURE_MIN;
    static const int32_t RAW_PRESSURE_MAX;
    static const int32_t RAW_ORIENTATION_MIN;
    static const int32_t RAW_ORIENTATION_MAX;
    static const int32_t RAW_DISTANCE_MIN;
    static const int32_t RAW_DISTANCE_MAX;
    static const int32_t RAW_TILT_MIN;
    static const int32_t RAW_TILT_MAX;
    static const int32_t RAW_ID_MIN;
    static const int32_t RAW_ID_MAX;
    static const int32_t RAW_SLOT_MIN;
    static const int32_t RAW_SLOT_MAX;
    static const float X_PRECISION;
    static const float Y_PRECISION;
    static const float X_PRECISION_VIRTUAL;
    static const float Y_PRECISION_VIRTUAL;

    static const float GEOMETRIC_SCALE;
    static const TouchAffineTransformation AFFINE_TRANSFORM;

    static const VirtualKeyDefinition VIRTUAL_KEYS[2];

    const std::string UNIQUE_ID = "local:0";
    const std::string SECONDARY_UNIQUE_ID = "local:1";

    enum Axes {
        POSITION = 1 << 0,
        TOUCH = 1 << 1,
        TOOL = 1 << 2,
        PRESSURE = 1 << 3,
        ORIENTATION = 1 << 4,
        MINOR = 1 << 5,
        ID = 1 << 6,
        DISTANCE = 1 << 7,
        TILT = 1 << 8,
        SLOT = 1 << 9,
        TOOL_TYPE = 1 << 10,
    };

    void prepareDisplay(int32_t orientation, std::optional<uint8_t> port = NO_PORT);
    void prepareSecondaryDisplay(ViewportType type, std::optional<uint8_t> port = NO_PORT);
    void prepareVirtualDisplay(int32_t orientation);
    void prepareVirtualKeys();
    void prepareLocationCalibration();
    int32_t toRawX(float displayX);
    int32_t toRawY(float displayY);
    float toCookedX(float rawX, float rawY);
    float toCookedY(float rawX, float rawY);
    float toDisplayX(int32_t rawX);
    float toDisplayX(int32_t rawX, int32_t displayWidth);
    float toDisplayY(int32_t rawY);
    float toDisplayY(int32_t rawY, int32_t displayHeight);

};

const int32_t TouchInputMapperTest::RAW_X_MIN = 25;
const int32_t TouchInputMapperTest::RAW_X_MAX = 1019;
const int32_t TouchInputMapperTest::RAW_Y_MIN = 30;
const int32_t TouchInputMapperTest::RAW_Y_MAX = 1009;
const int32_t TouchInputMapperTest::RAW_TOUCH_MIN = 0;
const int32_t TouchInputMapperTest::RAW_TOUCH_MAX = 31;
const int32_t TouchInputMapperTest::RAW_TOOL_MIN = 0;
const int32_t TouchInputMapperTest::RAW_TOOL_MAX = 15;
const int32_t TouchInputMapperTest::RAW_PRESSURE_MIN = 0;
const int32_t TouchInputMapperTest::RAW_PRESSURE_MAX = 255;
const int32_t TouchInputMapperTest::RAW_ORIENTATION_MIN = -7;
const int32_t TouchInputMapperTest::RAW_ORIENTATION_MAX = 7;
const int32_t TouchInputMapperTest::RAW_DISTANCE_MIN = 0;
const int32_t TouchInputMapperTest::RAW_DISTANCE_MAX = 7;
const int32_t TouchInputMapperTest::RAW_TILT_MIN = 0;
const int32_t TouchInputMapperTest::RAW_TILT_MAX = 150;
const int32_t TouchInputMapperTest::RAW_ID_MIN = 0;
const int32_t TouchInputMapperTest::RAW_ID_MAX = 9;
const int32_t TouchInputMapperTest::RAW_SLOT_MIN = 0;
const int32_t TouchInputMapperTest::RAW_SLOT_MAX = 9;
const float TouchInputMapperTest::X_PRECISION = float(RAW_X_MAX - RAW_X_MIN + 1) / DISPLAY_WIDTH;
const float TouchInputMapperTest::Y_PRECISION = float(RAW_Y_MAX - RAW_Y_MIN + 1) / DISPLAY_HEIGHT;
const float TouchInputMapperTest::X_PRECISION_VIRTUAL =
        float(RAW_X_MAX - RAW_X_MIN + 1) / VIRTUAL_DISPLAY_WIDTH;
const float TouchInputMapperTest::Y_PRECISION_VIRTUAL =
        float(RAW_Y_MAX - RAW_Y_MIN + 1) / VIRTUAL_DISPLAY_HEIGHT;
const TouchAffineTransformation TouchInputMapperTest::AFFINE_TRANSFORM =
        TouchAffineTransformation(1, -2, 3, -4, 5, -6);

const float TouchInputMapperTest::GEOMETRIC_SCALE =
        avg(float(DISPLAY_WIDTH) / (RAW_X_MAX - RAW_X_MIN + 1),
                float(DISPLAY_HEIGHT) / (RAW_Y_MAX - RAW_Y_MIN + 1));

const VirtualKeyDefinition TouchInputMapperTest::VIRTUAL_KEYS[2] = {
        { KEY_HOME, 60, DISPLAY_HEIGHT + 15, 20, 20 },
        { KEY_MENU, DISPLAY_HEIGHT - 60, DISPLAY_WIDTH + 15, 20, 20 },
};

void TouchInputMapperTest::prepareDisplay(int32_t orientation, std::optional<uint8_t> port) {
    setDisplayInfoAndReconfigure(DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT, orientation,
            UNIQUE_ID, port, ViewportType::VIEWPORT_INTERNAL);
}

void TouchInputMapperTest::prepareSecondaryDisplay(ViewportType type, std::optional<uint8_t> port) {
    setDisplayInfoAndReconfigure(SECONDARY_DISPLAY_ID, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_ORIENTATION_0, SECONDARY_UNIQUE_ID, port, type);
}

void TouchInputMapperTest::prepareVirtualDisplay(int32_t orientation) {
    setDisplayInfoAndReconfigure(VIRTUAL_DISPLAY_ID, VIRTUAL_DISPLAY_WIDTH,
        VIRTUAL_DISPLAY_HEIGHT, orientation,
        VIRTUAL_DISPLAY_UNIQUE_ID, NO_PORT, ViewportType::VIEWPORT_VIRTUAL);
}

void TouchInputMapperTest::prepareVirtualKeys() {
    mFakeEventHub->addVirtualKeyDefinition(EVENTHUB_ID, VIRTUAL_KEYS[0]);
    mFakeEventHub->addVirtualKeyDefinition(EVENTHUB_ID, VIRTUAL_KEYS[1]);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_HOME, 0, AKEYCODE_HOME, POLICY_FLAG_WAKE);
    mFakeEventHub->addKey(EVENTHUB_ID, KEY_MENU, 0, AKEYCODE_MENU, POLICY_FLAG_WAKE);
}

void TouchInputMapperTest::prepareLocationCalibration() {
    mFakePolicy->setTouchAffineTransformation(AFFINE_TRANSFORM);
}

int32_t TouchInputMapperTest::toRawX(float displayX) {
    return int32_t(displayX * (RAW_X_MAX - RAW_X_MIN + 1) / DISPLAY_WIDTH + RAW_X_MIN);
}

int32_t TouchInputMapperTest::toRawY(float displayY) {
    return int32_t(displayY * (RAW_Y_MAX - RAW_Y_MIN + 1) / DISPLAY_HEIGHT + RAW_Y_MIN);
}

float TouchInputMapperTest::toCookedX(float rawX, float rawY) {
    AFFINE_TRANSFORM.applyTo(rawX, rawY);
    return rawX;
}

float TouchInputMapperTest::toCookedY(float rawX, float rawY) {
    AFFINE_TRANSFORM.applyTo(rawX, rawY);
    return rawY;
}

float TouchInputMapperTest::toDisplayX(int32_t rawX) {
    return toDisplayX(rawX, DISPLAY_WIDTH);
}

float TouchInputMapperTest::toDisplayX(int32_t rawX, int32_t displayWidth) {
    return float(rawX - RAW_X_MIN) * displayWidth / (RAW_X_MAX - RAW_X_MIN + 1);
}

float TouchInputMapperTest::toDisplayY(int32_t rawY) {
    return toDisplayY(rawY, DISPLAY_HEIGHT);
}

float TouchInputMapperTest::toDisplayY(int32_t rawY, int32_t displayHeight) {
    return float(rawY - RAW_Y_MIN) * displayHeight / (RAW_Y_MAX - RAW_Y_MIN + 1);
}


// --- SingleTouchInputMapperTest ---

class SingleTouchInputMapperTest : public TouchInputMapperTest {
protected:
    void prepareButtons();
    void prepareAxes(int axes);

    void processDown(SingleTouchInputMapper& mapper, int32_t x, int32_t y);
    void processMove(SingleTouchInputMapper& mapper, int32_t x, int32_t y);
    void processUp(SingleTouchInputMapper& mappery);
    void processPressure(SingleTouchInputMapper& mapper, int32_t pressure);
    void processToolMajor(SingleTouchInputMapper& mapper, int32_t toolMajor);
    void processDistance(SingleTouchInputMapper& mapper, int32_t distance);
    void processTilt(SingleTouchInputMapper& mapper, int32_t tiltX, int32_t tiltY);
    void processKey(SingleTouchInputMapper& mapper, int32_t code, int32_t value);
    void processSync(SingleTouchInputMapper& mapper);
};

void SingleTouchInputMapperTest::prepareButtons() {
    mFakeEventHub->addKey(EVENTHUB_ID, BTN_TOUCH, 0, AKEYCODE_UNKNOWN, 0);
}

void SingleTouchInputMapperTest::prepareAxes(int axes) {
    if (axes & POSITION) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_X, RAW_X_MIN, RAW_X_MAX, 0, 0);
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_Y, RAW_Y_MIN, RAW_Y_MAX, 0, 0);
    }
    if (axes & PRESSURE) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_PRESSURE, RAW_PRESSURE_MIN,
                                       RAW_PRESSURE_MAX, 0, 0);
    }
    if (axes & TOOL) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_TOOL_WIDTH, RAW_TOOL_MIN, RAW_TOOL_MAX, 0,
                                       0);
    }
    if (axes & DISTANCE) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_DISTANCE, RAW_DISTANCE_MIN,
                                       RAW_DISTANCE_MAX, 0, 0);
    }
    if (axes & TILT) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_TILT_X, RAW_TILT_MIN, RAW_TILT_MAX, 0, 0);
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_TILT_Y, RAW_TILT_MIN, RAW_TILT_MAX, 0, 0);
    }
}

void SingleTouchInputMapperTest::processDown(SingleTouchInputMapper& mapper, int32_t x, int32_t y) {
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_TOUCH, 1);
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_X, x);
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_Y, y);
}

void SingleTouchInputMapperTest::processMove(SingleTouchInputMapper& mapper, int32_t x, int32_t y) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_X, x);
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_Y, y);
}

void SingleTouchInputMapperTest::processUp(SingleTouchInputMapper& mapper) {
    process(mapper, ARBITRARY_TIME, EV_KEY, BTN_TOUCH, 0);
}

void SingleTouchInputMapperTest::processPressure(SingleTouchInputMapper& mapper, int32_t pressure) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_PRESSURE, pressure);
}

void SingleTouchInputMapperTest::processToolMajor(SingleTouchInputMapper& mapper,
                                                  int32_t toolMajor) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_TOOL_WIDTH, toolMajor);
}

void SingleTouchInputMapperTest::processDistance(SingleTouchInputMapper& mapper, int32_t distance) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_DISTANCE, distance);
}

void SingleTouchInputMapperTest::processTilt(SingleTouchInputMapper& mapper, int32_t tiltX,
                                             int32_t tiltY) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_TILT_X, tiltX);
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_TILT_Y, tiltY);
}

void SingleTouchInputMapperTest::processKey(SingleTouchInputMapper& mapper, int32_t code,
                                            int32_t value) {
    process(mapper, ARBITRARY_TIME, EV_KEY, code, value);
}

void SingleTouchInputMapperTest::processSync(SingleTouchInputMapper& mapper) {
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
}

TEST_F(SingleTouchInputMapperTest, GetSources_WhenDeviceTypeIsNotSpecifiedAndNotACursor_ReturnsPointer) {
    prepareButtons();
    prepareAxes(POSITION);
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    ASSERT_EQ(AINPUT_SOURCE_MOUSE, mapper.getSources());
}

TEST_F(SingleTouchInputMapperTest, GetSources_WhenDeviceTypeIsNotSpecifiedAndIsACursor_ReturnsTouchPad) {
    mFakeEventHub->addRelativeAxis(EVENTHUB_ID, REL_X);
    mFakeEventHub->addRelativeAxis(EVENTHUB_ID, REL_Y);
    prepareButtons();
    prepareAxes(POSITION);
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    ASSERT_EQ(AINPUT_SOURCE_TOUCHPAD, mapper.getSources());
}

TEST_F(SingleTouchInputMapperTest, GetSources_WhenDeviceTypeIsTouchPad_ReturnsTouchPad) {
    prepareButtons();
    prepareAxes(POSITION);
    addConfigurationProperty("touch.deviceType", "touchPad");
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    ASSERT_EQ(AINPUT_SOURCE_TOUCHPAD, mapper.getSources());
}

TEST_F(SingleTouchInputMapperTest, GetSources_WhenDeviceTypeIsTouchScreen_ReturnsTouchScreen) {
    prepareButtons();
    prepareAxes(POSITION);
    addConfigurationProperty("touch.deviceType", "touchScreen");
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, mapper.getSources());
}

TEST_F(SingleTouchInputMapperTest, GetKeyCodeState) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION);
    prepareVirtualKeys();
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    // Unknown key.
    ASSERT_EQ(AKEY_STATE_UNKNOWN, mapper.getKeyCodeState(AINPUT_SOURCE_ANY, AKEYCODE_A));

    // Virtual key is down.
    int32_t x = toRawX(VIRTUAL_KEYS[0].centerX);
    int32_t y = toRawY(VIRTUAL_KEYS[0].centerY);
    processDown(mapper, x, y);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled());

    ASSERT_EQ(AKEY_STATE_VIRTUAL, mapper.getKeyCodeState(AINPUT_SOURCE_ANY, AKEYCODE_HOME));

    // Virtual key is up.
    processUp(mapper);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled());

    ASSERT_EQ(AKEY_STATE_UP, mapper.getKeyCodeState(AINPUT_SOURCE_ANY, AKEYCODE_HOME));
}

TEST_F(SingleTouchInputMapperTest, GetScanCodeState) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION);
    prepareVirtualKeys();
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    // Unknown key.
    ASSERT_EQ(AKEY_STATE_UNKNOWN, mapper.getScanCodeState(AINPUT_SOURCE_ANY, KEY_A));

    // Virtual key is down.
    int32_t x = toRawX(VIRTUAL_KEYS[0].centerX);
    int32_t y = toRawY(VIRTUAL_KEYS[0].centerY);
    processDown(mapper, x, y);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled());

    ASSERT_EQ(AKEY_STATE_VIRTUAL, mapper.getScanCodeState(AINPUT_SOURCE_ANY, KEY_HOME));

    // Virtual key is up.
    processUp(mapper);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled());

    ASSERT_EQ(AKEY_STATE_UP, mapper.getScanCodeState(AINPUT_SOURCE_ANY, KEY_HOME));
}

TEST_F(SingleTouchInputMapperTest, MarkSupportedKeyCodes) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION);
    prepareVirtualKeys();
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    const int32_t keys[2] = { AKEYCODE_HOME, AKEYCODE_A };
    uint8_t flags[2] = { 0, 0 };
    ASSERT_TRUE(mapper.markSupportedKeyCodes(AINPUT_SOURCE_ANY, 2, keys, flags));
    ASSERT_TRUE(flags[0]);
    ASSERT_FALSE(flags[1]);
}

TEST_F(SingleTouchInputMapperTest, Process_WhenVirtualKeyIsPressedAndReleasedNormally_SendsKeyDownAndKeyUp) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION);
    prepareVirtualKeys();
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    mFakeContext->setGlobalMetaState(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON);

    NotifyKeyArgs args;

    // Press virtual key.
    int32_t x = toRawX(VIRTUAL_KEYS[0].centerX);
    int32_t y = toRawY(VIRTUAL_KEYS[0].centerY);
    processDown(mapper, x, y);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(ARBITRARY_TIME, args.eventTime);
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, args.source);
    ASSERT_EQ(POLICY_FLAG_VIRTUAL, args.policyFlags);
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, args.action);
    ASSERT_EQ(AKEY_EVENT_FLAG_FROM_SYSTEM | AKEY_EVENT_FLAG_VIRTUAL_HARD_KEY, args.flags);
    ASSERT_EQ(AKEYCODE_HOME, args.keyCode);
    ASSERT_EQ(KEY_HOME, args.scanCode);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, args.metaState);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);

    // Release virtual key.
    processUp(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&args));
    ASSERT_EQ(ARBITRARY_TIME, args.eventTime);
    ASSERT_EQ(DEVICE_ID, args.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, args.source);
    ASSERT_EQ(POLICY_FLAG_VIRTUAL, args.policyFlags);
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, args.action);
    ASSERT_EQ(AKEY_EVENT_FLAG_FROM_SYSTEM | AKEY_EVENT_FLAG_VIRTUAL_HARD_KEY, args.flags);
    ASSERT_EQ(AKEYCODE_HOME, args.keyCode);
    ASSERT_EQ(KEY_HOME, args.scanCode);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, args.metaState);
    ASSERT_EQ(ARBITRARY_TIME, args.downTime);

    // Should not have sent any motions.
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasNotCalled());
}

TEST_F(SingleTouchInputMapperTest, Process_WhenVirtualKeyIsPressedAndMovedOutOfBounds_SendsKeyDownAndKeyCancel) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION);
    prepareVirtualKeys();
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    mFakeContext->setGlobalMetaState(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON);

    NotifyKeyArgs keyArgs;

    // Press virtual key.
    int32_t x = toRawX(VIRTUAL_KEYS[0].centerX);
    int32_t y = toRawY(VIRTUAL_KEYS[0].centerY);
    processDown(mapper, x, y);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(ARBITRARY_TIME, keyArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, keyArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, keyArgs.source);
    ASSERT_EQ(POLICY_FLAG_VIRTUAL, keyArgs.policyFlags);
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEY_EVENT_FLAG_FROM_SYSTEM | AKEY_EVENT_FLAG_VIRTUAL_HARD_KEY, keyArgs.flags);
    ASSERT_EQ(AKEYCODE_HOME, keyArgs.keyCode);
    ASSERT_EQ(KEY_HOME, keyArgs.scanCode);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, keyArgs.metaState);
    ASSERT_EQ(ARBITRARY_TIME, keyArgs.downTime);

    // Move out of bounds.  This should generate a cancel and a pointer down since we moved
    // into the display area.
    y -= 100;
    processMove(mapper, x, y);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(ARBITRARY_TIME, keyArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, keyArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_KEYBOARD, keyArgs.source);
    ASSERT_EQ(POLICY_FLAG_VIRTUAL, keyArgs.policyFlags);
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEY_EVENT_FLAG_FROM_SYSTEM | AKEY_EVENT_FLAG_VIRTUAL_HARD_KEY
            | AKEY_EVENT_FLAG_CANCELED, keyArgs.flags);
    ASSERT_EQ(AKEYCODE_HOME, keyArgs.keyCode);
    ASSERT_EQ(KEY_HOME, keyArgs.scanCode);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, keyArgs.metaState);
    ASSERT_EQ(ARBITRARY_TIME, keyArgs.downTime);

    NotifyMotionArgs motionArgs;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x), toDisplayY(y), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Keep moving out of bounds.  Should generate a pointer move.
    y -= 50;
    processMove(mapper, x, y);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x), toDisplayY(y), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Release out of bounds.  Should generate a pointer up.
    processUp(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x), toDisplayY(y), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Should not have sent any more keys or motions.
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasNotCalled());
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());
}

TEST_F(SingleTouchInputMapperTest, Process_WhenTouchStartsOutsideDisplayAndMovesIn_SendsDownAsTouchEntersDisplay) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION);
    prepareVirtualKeys();
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    mFakeContext->setGlobalMetaState(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON);

    NotifyMotionArgs motionArgs;

    // Initially go down out of bounds.
    int32_t x = -10;
    int32_t y = -10;
    processDown(mapper, x, y);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());

    // Move into the display area.  Should generate a pointer down.
    x = 50;
    y = 75;
    processMove(mapper, x, y);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x), toDisplayY(y), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Release.  Should generate a pointer up.
    processUp(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x), toDisplayY(y), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Should not have sent any more keys or motions.
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasNotCalled());
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());
}

TEST_F(SingleTouchInputMapperTest, Process_NormalSingleTouchGesture_VirtualDisplay) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    addConfigurationProperty("touch.displayId", VIRTUAL_DISPLAY_UNIQUE_ID);

    prepareVirtualDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION);
    prepareVirtualKeys();
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    mFakeContext->setGlobalMetaState(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON);

    NotifyMotionArgs motionArgs;

    // Down.
    int32_t x = 100;
    int32_t y = 125;
    processDown(mapper, x, y);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(VIRTUAL_DISPLAY_ID, motionArgs.displayId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x, VIRTUAL_DISPLAY_WIDTH), toDisplayY(y, VIRTUAL_DISPLAY_HEIGHT),
            1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION_VIRTUAL, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION_VIRTUAL, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Move.
    x += 50;
    y += 75;
    processMove(mapper, x, y);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(VIRTUAL_DISPLAY_ID, motionArgs.displayId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x, VIRTUAL_DISPLAY_WIDTH), toDisplayY(y, VIRTUAL_DISPLAY_HEIGHT),
            1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION_VIRTUAL, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION_VIRTUAL, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Up.
    processUp(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(VIRTUAL_DISPLAY_ID, motionArgs.displayId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x, VIRTUAL_DISPLAY_WIDTH), toDisplayY(y, VIRTUAL_DISPLAY_HEIGHT),
            1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION_VIRTUAL, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION_VIRTUAL, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Should not have sent any more keys or motions.
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasNotCalled());
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());
}

TEST_F(SingleTouchInputMapperTest, Process_NormalSingleTouchGesture) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION);
    prepareVirtualKeys();
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    mFakeContext->setGlobalMetaState(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON);

    NotifyMotionArgs motionArgs;

    // Down.
    int32_t x = 100;
    int32_t y = 125;
    processDown(mapper, x, y);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x), toDisplayY(y), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Move.
    x += 50;
    y += 75;
    processMove(mapper, x, y);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x), toDisplayY(y), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Up.
    processUp(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x), toDisplayY(y), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Should not have sent any more keys or motions.
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasNotCalled());
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());
}

TEST_F(SingleTouchInputMapperTest, Process_WhenNotOrientationAware_DoesNotRotateMotions) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareButtons();
    prepareAxes(POSITION);
    addConfigurationProperty("touch.orientationAware", "0");
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    NotifyMotionArgs args;

    // Rotation 90.
    prepareDisplay(DISPLAY_ORIENTATION_90);
    processDown(mapper, toRawX(50), toRawY(75));
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_NEAR(50, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_X), 1);
    ASSERT_NEAR(75, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_Y), 1);

    processUp(mapper);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled());
}

TEST_F(SingleTouchInputMapperTest, Process_WhenOrientationAware_RotatesMotions) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareButtons();
    prepareAxes(POSITION);
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    NotifyMotionArgs args;

    // Rotation 0.
    clearViewports();
    prepareDisplay(DISPLAY_ORIENTATION_0);
    processDown(mapper, toRawX(50), toRawY(75));
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_NEAR(50, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_X), 1);
    ASSERT_NEAR(75, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_Y), 1);

    processUp(mapper);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled());

    // Rotation 90.
    clearViewports();
    prepareDisplay(DISPLAY_ORIENTATION_90);
    processDown(mapper, RAW_X_MAX - toRawX(75) + RAW_X_MIN, toRawY(50));
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_NEAR(50, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_X), 1);
    ASSERT_NEAR(75, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_Y), 1);

    processUp(mapper);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled());

    // Rotation 180.
    clearViewports();
    prepareDisplay(DISPLAY_ORIENTATION_180);
    processDown(mapper, RAW_X_MAX - toRawX(50) + RAW_X_MIN, RAW_Y_MAX - toRawY(75) + RAW_Y_MIN);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_NEAR(50, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_X), 1);
    ASSERT_NEAR(75, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_Y), 1);

    processUp(mapper);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled());

    // Rotation 270.
    clearViewports();
    prepareDisplay(DISPLAY_ORIENTATION_270);
    processDown(mapper, toRawX(75), RAW_Y_MAX - toRawY(50) + RAW_Y_MIN);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_NEAR(50, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_X), 1);
    ASSERT_NEAR(75, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_Y), 1);

    processUp(mapper);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled());
}

TEST_F(SingleTouchInputMapperTest, Process_AllAxes_DefaultCalibration) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION | PRESSURE | TOOL | DISTANCE | TILT);
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    // These calculations are based on the input device calibration documentation.
    int32_t rawX = 100;
    int32_t rawY = 200;
    int32_t rawPressure = 10;
    int32_t rawToolMajor = 12;
    int32_t rawDistance = 2;
    int32_t rawTiltX = 30;
    int32_t rawTiltY = 110;

    float x = toDisplayX(rawX);
    float y = toDisplayY(rawY);
    float pressure = float(rawPressure) / RAW_PRESSURE_MAX;
    float size = float(rawToolMajor) / RAW_TOOL_MAX;
    float tool = float(rawToolMajor) * GEOMETRIC_SCALE;
    float distance = float(rawDistance);

    float tiltCenter = (RAW_TILT_MAX + RAW_TILT_MIN) * 0.5f;
    float tiltScale = M_PI / 180;
    float tiltXAngle = (rawTiltX - tiltCenter) * tiltScale;
    float tiltYAngle = (rawTiltY - tiltCenter) * tiltScale;
    float orientation = atan2f(-sinf(tiltXAngle), sinf(tiltYAngle));
    float tilt = acosf(cosf(tiltXAngle) * cosf(tiltYAngle));

    processDown(mapper, rawX, rawY);
    processPressure(mapper, rawPressure);
    processToolMajor(mapper, rawToolMajor);
    processDistance(mapper, rawDistance);
    processTilt(mapper, rawTiltX, rawTiltY);
    processSync(mapper);

    NotifyMotionArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            x, y, pressure, size, tool, tool, tool, tool, orientation, distance));
    ASSERT_EQ(tilt, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_TILT));
}

TEST_F(SingleTouchInputMapperTest, Process_XYAxes_AffineCalibration) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareLocationCalibration();
    prepareButtons();
    prepareAxes(POSITION);
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    int32_t rawX = 100;
    int32_t rawY = 200;

    float x = toDisplayX(toCookedX(rawX, rawY));
    float y = toDisplayY(toCookedY(rawX, rawY));

    processDown(mapper, rawX, rawY);
    processSync(mapper);

    NotifyMotionArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            x, y, 1, 0, 0, 0, 0, 0, 0, 0));
}

TEST_F(SingleTouchInputMapperTest, Process_ShouldHandleAllButtons) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION);
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    NotifyMotionArgs motionArgs;
    NotifyKeyArgs keyArgs;

    processDown(mapper, 100, 200);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    // press BTN_LEFT, release BTN_LEFT
    processKey(mapper, BTN_LEFT, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_PRIMARY, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_PRIMARY, motionArgs.buttonState);

    processKey(mapper, BTN_LEFT, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    // press BTN_RIGHT + BTN_MIDDLE, release BTN_RIGHT, release BTN_MIDDLE
    processKey(mapper, BTN_RIGHT, 1);
    processKey(mapper, BTN_MIDDLE, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_SECONDARY | AMOTION_EVENT_BUTTON_TERTIARY,
            motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_TERTIARY, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_SECONDARY | AMOTION_EVENT_BUTTON_TERTIARY,
            motionArgs.buttonState);

    processKey(mapper, BTN_RIGHT, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_TERTIARY, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_TERTIARY, motionArgs.buttonState);

    processKey(mapper, BTN_MIDDLE, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    // press BTN_BACK, release BTN_BACK
    processKey(mapper, BTN_BACK, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);

    processKey(mapper, BTN_BACK, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    // press BTN_SIDE, release BTN_SIDE
    processKey(mapper, BTN_SIDE, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);

    processKey(mapper, BTN_SIDE, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    // press BTN_FORWARD, release BTN_FORWARD
    processKey(mapper, BTN_FORWARD, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);

    processKey(mapper, BTN_FORWARD, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);

    // press BTN_EXTRA, release BTN_EXTRA
    processKey(mapper, BTN_EXTRA, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);

    processKey(mapper, BTN_EXTRA, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasNotCalled());

    // press BTN_STYLUS, release BTN_STYLUS
    processKey(mapper, BTN_STYLUS, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_STYLUS_PRIMARY, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_STYLUS_PRIMARY, motionArgs.buttonState);

    processKey(mapper, BTN_STYLUS, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    // press BTN_STYLUS2, release BTN_STYLUS2
    processKey(mapper, BTN_STYLUS2, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_STYLUS_SECONDARY, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_STYLUS_SECONDARY, motionArgs.buttonState);

    processKey(mapper, BTN_STYLUS2, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    // release touch
    processUp(mapper);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
}

TEST_F(SingleTouchInputMapperTest, Process_ShouldHandleAllToolTypes) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION);
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    NotifyMotionArgs motionArgs;

    // default tool type is finger
    processDown(mapper, 100, 200);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // eraser
    processKey(mapper, BTN_TOOL_RUBBER, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_ERASER, motionArgs.pointerProperties[0].toolType);

    // stylus
    processKey(mapper, BTN_TOOL_RUBBER, 0);
    processKey(mapper, BTN_TOOL_PEN, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_STYLUS, motionArgs.pointerProperties[0].toolType);

    // brush
    processKey(mapper, BTN_TOOL_PEN, 0);
    processKey(mapper, BTN_TOOL_BRUSH, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_STYLUS, motionArgs.pointerProperties[0].toolType);

    // pencil
    processKey(mapper, BTN_TOOL_BRUSH, 0);
    processKey(mapper, BTN_TOOL_PENCIL, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_STYLUS, motionArgs.pointerProperties[0].toolType);

    // air-brush
    processKey(mapper, BTN_TOOL_PENCIL, 0);
    processKey(mapper, BTN_TOOL_AIRBRUSH, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_STYLUS, motionArgs.pointerProperties[0].toolType);

    // mouse
    processKey(mapper, BTN_TOOL_AIRBRUSH, 0);
    processKey(mapper, BTN_TOOL_MOUSE, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_MOUSE, motionArgs.pointerProperties[0].toolType);

    // lens
    processKey(mapper, BTN_TOOL_MOUSE, 0);
    processKey(mapper, BTN_TOOL_LENS, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_MOUSE, motionArgs.pointerProperties[0].toolType);

    // double-tap
    processKey(mapper, BTN_TOOL_LENS, 0);
    processKey(mapper, BTN_TOOL_DOUBLETAP, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // triple-tap
    processKey(mapper, BTN_TOOL_DOUBLETAP, 0);
    processKey(mapper, BTN_TOOL_TRIPLETAP, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // quad-tap
    processKey(mapper, BTN_TOOL_TRIPLETAP, 0);
    processKey(mapper, BTN_TOOL_QUADTAP, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // finger
    processKey(mapper, BTN_TOOL_QUADTAP, 0);
    processKey(mapper, BTN_TOOL_FINGER, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // stylus trumps finger
    processKey(mapper, BTN_TOOL_PEN, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_STYLUS, motionArgs.pointerProperties[0].toolType);

    // eraser trumps stylus
    processKey(mapper, BTN_TOOL_RUBBER, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_ERASER, motionArgs.pointerProperties[0].toolType);

    // mouse trumps eraser
    processKey(mapper, BTN_TOOL_MOUSE, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_MOUSE, motionArgs.pointerProperties[0].toolType);

    // back to default tool type
    processKey(mapper, BTN_TOOL_MOUSE, 0);
    processKey(mapper, BTN_TOOL_RUBBER, 0);
    processKey(mapper, BTN_TOOL_PEN, 0);
    processKey(mapper, BTN_TOOL_FINGER, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
}

TEST_F(SingleTouchInputMapperTest, Process_WhenBtnTouchPresent_HoversIfItsValueIsZero) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION);
    mFakeEventHub->addKey(EVENTHUB_ID, BTN_TOOL_FINGER, 0, AKEYCODE_UNKNOWN, 0);
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    NotifyMotionArgs motionArgs;

    // initially hovering because BTN_TOUCH not sent yet, pressure defaults to 0
    processKey(mapper, BTN_TOOL_FINGER, 1);
    processMove(mapper, 100, 200);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_ENTER, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(100), toDisplayY(200), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(100), toDisplayY(200), 0, 0, 0, 0, 0, 0, 0, 0));

    // move a little
    processMove(mapper, 150, 250);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    // down when BTN_TOUCH is pressed, pressure defaults to 1
    processKey(mapper, BTN_TOUCH, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_EXIT, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 1, 0, 0, 0, 0, 0, 0, 0));

    // up when BTN_TOUCH is released, hover restored
    processKey(mapper, BTN_TOUCH, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 1, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_ENTER, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    // exit hover when pointer goes away
    processKey(mapper, BTN_TOOL_FINGER, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_EXIT, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));
}

TEST_F(SingleTouchInputMapperTest, Process_WhenAbsPressureIsPresent_HoversIfItsValueIsZero) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareButtons();
    prepareAxes(POSITION | PRESSURE);
    SingleTouchInputMapper& mapper = addMapperAndConfigure<SingleTouchInputMapper>();

    NotifyMotionArgs motionArgs;

    // initially hovering because pressure is 0
    processDown(mapper, 100, 200);
    processPressure(mapper, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_ENTER, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(100), toDisplayY(200), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(100), toDisplayY(200), 0, 0, 0, 0, 0, 0, 0, 0));

    // move a little
    processMove(mapper, 150, 250);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    // down when pressure is non-zero
    processPressure(mapper, RAW_PRESSURE_MAX);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_EXIT, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 1, 0, 0, 0, 0, 0, 0, 0));

    // up when pressure becomes 0, hover restored
    processPressure(mapper, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 1, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_ENTER, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    // exit hover when pointer goes away
    processUp(mapper);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_EXIT, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));
}

// --- MultiTouchInputMapperTest ---

class MultiTouchInputMapperTest : public TouchInputMapperTest {
protected:
    void prepareAxes(int axes);

    void processPosition(MultiTouchInputMapper& mapper, int32_t x, int32_t y);
    void processTouchMajor(MultiTouchInputMapper& mapper, int32_t touchMajor);
    void processTouchMinor(MultiTouchInputMapper& mapper, int32_t touchMinor);
    void processToolMajor(MultiTouchInputMapper& mapper, int32_t toolMajor);
    void processToolMinor(MultiTouchInputMapper& mapper, int32_t toolMinor);
    void processOrientation(MultiTouchInputMapper& mapper, int32_t orientation);
    void processPressure(MultiTouchInputMapper& mapper, int32_t pressure);
    void processDistance(MultiTouchInputMapper& mapper, int32_t distance);
    void processId(MultiTouchInputMapper& mapper, int32_t id);
    void processSlot(MultiTouchInputMapper& mapper, int32_t slot);
    void processToolType(MultiTouchInputMapper& mapper, int32_t toolType);
    void processKey(MultiTouchInputMapper& mapper, int32_t code, int32_t value);
    void processMTSync(MultiTouchInputMapper& mapper);
    void processSync(MultiTouchInputMapper& mapper);
};

void MultiTouchInputMapperTest::prepareAxes(int axes) {
    if (axes & POSITION) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_POSITION_X, RAW_X_MIN, RAW_X_MAX, 0, 0);
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_POSITION_Y, RAW_Y_MIN, RAW_Y_MAX, 0, 0);
    }
    if (axes & TOUCH) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_TOUCH_MAJOR, RAW_TOUCH_MIN,
                                       RAW_TOUCH_MAX, 0, 0);
        if (axes & MINOR) {
            mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_TOUCH_MINOR, RAW_TOUCH_MIN,
                                           RAW_TOUCH_MAX, 0, 0);
        }
    }
    if (axes & TOOL) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_WIDTH_MAJOR, RAW_TOOL_MIN, RAW_TOOL_MAX,
                                       0, 0);
        if (axes & MINOR) {
            mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_WIDTH_MINOR, RAW_TOOL_MAX,
                                           RAW_TOOL_MAX, 0, 0);
        }
    }
    if (axes & ORIENTATION) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_ORIENTATION, RAW_ORIENTATION_MIN,
                                       RAW_ORIENTATION_MAX, 0, 0);
    }
    if (axes & PRESSURE) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_PRESSURE, RAW_PRESSURE_MIN,
                                       RAW_PRESSURE_MAX, 0, 0);
    }
    if (axes & DISTANCE) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_DISTANCE, RAW_DISTANCE_MIN,
                                       RAW_DISTANCE_MAX, 0, 0);
    }
    if (axes & ID) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_TRACKING_ID, RAW_ID_MIN, RAW_ID_MAX, 0,
                                       0);
    }
    if (axes & SLOT) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_SLOT, RAW_SLOT_MIN, RAW_SLOT_MAX, 0, 0);
        mFakeEventHub->setAbsoluteAxisValue(EVENTHUB_ID, ABS_MT_SLOT, 0);
    }
    if (axes & TOOL_TYPE) {
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0);
    }
}

void MultiTouchInputMapperTest::processPosition(MultiTouchInputMapper& mapper, int32_t x,
                                                int32_t y) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_POSITION_X, x);
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_POSITION_Y, y);
}

void MultiTouchInputMapperTest::processTouchMajor(MultiTouchInputMapper& mapper,
                                                  int32_t touchMajor) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_TOUCH_MAJOR, touchMajor);
}

void MultiTouchInputMapperTest::processTouchMinor(MultiTouchInputMapper& mapper,
                                                  int32_t touchMinor) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_TOUCH_MINOR, touchMinor);
}

void MultiTouchInputMapperTest::processToolMajor(MultiTouchInputMapper& mapper, int32_t toolMajor) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_WIDTH_MAJOR, toolMajor);
}

void MultiTouchInputMapperTest::processToolMinor(MultiTouchInputMapper& mapper, int32_t toolMinor) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_WIDTH_MINOR, toolMinor);
}

void MultiTouchInputMapperTest::processOrientation(MultiTouchInputMapper& mapper,
                                                   int32_t orientation) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_ORIENTATION, orientation);
}

void MultiTouchInputMapperTest::processPressure(MultiTouchInputMapper& mapper, int32_t pressure) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_PRESSURE, pressure);
}

void MultiTouchInputMapperTest::processDistance(MultiTouchInputMapper& mapper, int32_t distance) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_DISTANCE, distance);
}

void MultiTouchInputMapperTest::processId(MultiTouchInputMapper& mapper, int32_t id) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_TRACKING_ID, id);
}

void MultiTouchInputMapperTest::processSlot(MultiTouchInputMapper& mapper, int32_t slot) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_SLOT, slot);
}

void MultiTouchInputMapperTest::processToolType(MultiTouchInputMapper& mapper, int32_t toolType) {
    process(mapper, ARBITRARY_TIME, EV_ABS, ABS_MT_TOOL_TYPE, toolType);
}

void MultiTouchInputMapperTest::processKey(MultiTouchInputMapper& mapper, int32_t code,
                                           int32_t value) {
    process(mapper, ARBITRARY_TIME, EV_KEY, code, value);
}

void MultiTouchInputMapperTest::processMTSync(MultiTouchInputMapper& mapper) {
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_MT_REPORT, 0);
}

void MultiTouchInputMapperTest::processSync(MultiTouchInputMapper& mapper) {
    process(mapper, ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
}

TEST_F(MultiTouchInputMapperTest, Process_NormalMultiTouchGesture_WithoutTrackingIds) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION);
    prepareVirtualKeys();
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    mFakeContext->setGlobalMetaState(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON);

    NotifyMotionArgs motionArgs;

    // Two fingers down at once.
    int32_t x1 = 100, y1 = 125, x2 = 300, y2 = 500;
    processPosition(mapper, x1, y1);
    processMTSync(mapper);
    processPosition(mapper, x2, y2);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_DOWN | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Move.
    x1 += 10; y1 += 15; x2 += 5; y2 -= 10;
    processPosition(mapper, x1, y1);
    processMTSync(mapper);
    processPosition(mapper, x2, y2);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // First finger up.
    x2 += 15; y2 -= 20;
    processPosition(mapper, x2, y2);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_UP | (0 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(1, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Move.
    x2 += 20; y2 -= 25;
    processPosition(mapper, x2, y2);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(1, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // New finger down.
    int32_t x3 = 700, y3 = 300;
    processPosition(mapper, x2, y2);
    processMTSync(mapper);
    processPosition(mapper, x3, y3);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_DOWN | (0 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Second finger up.
    x3 += 30; y3 -= 20;
    processPosition(mapper, x3, y3);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_UP | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Last finger up.
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.eventTime);
    ASSERT_EQ(DEVICE_ID, motionArgs.deviceId);
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, motionArgs.source);
    ASSERT_EQ(uint32_t(0), motionArgs.policyFlags);
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_EQ(0, motionArgs.flags);
    ASSERT_EQ(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON, motionArgs.metaState);
    ASSERT_EQ(0, motionArgs.buttonState);
    ASSERT_EQ(0, motionArgs.edgeFlags);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NEAR(X_PRECISION, motionArgs.xPrecision, EPSILON);
    ASSERT_NEAR(Y_PRECISION, motionArgs.yPrecision, EPSILON);
    ASSERT_EQ(ARBITRARY_TIME, motionArgs.downTime);

    // Should not have sent any more keys or motions.
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasNotCalled());
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());
}

TEST_F(MultiTouchInputMapperTest, Process_NormalMultiTouchGesture_WithTrackingIds) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | ID);
    prepareVirtualKeys();
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    mFakeContext->setGlobalMetaState(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON);

    NotifyMotionArgs motionArgs;

    // Two fingers down at once.
    int32_t x1 = 100, y1 = 125, x2 = 300, y2 = 500;
    processPosition(mapper, x1, y1);
    processId(mapper, 1);
    processMTSync(mapper);
    processPosition(mapper, x2, y2);
    processId(mapper, 2);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_DOWN | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    // Move.
    x1 += 10; y1 += 15; x2 += 5; y2 -= 10;
    processPosition(mapper, x1, y1);
    processId(mapper, 1);
    processMTSync(mapper);
    processPosition(mapper, x2, y2);
    processId(mapper, 2);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    // First finger up.
    x2 += 15; y2 -= 20;
    processPosition(mapper, x2, y2);
    processId(mapper, 2);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_UP | (0 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(1, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    // Move.
    x2 += 20; y2 -= 25;
    processPosition(mapper, x2, y2);
    processId(mapper, 2);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(1, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    // New finger down.
    int32_t x3 = 700, y3 = 300;
    processPosition(mapper, x2, y2);
    processId(mapper, 2);
    processMTSync(mapper);
    processPosition(mapper, x3, y3);
    processId(mapper, 3);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_DOWN | (0 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    // Second finger up.
    x3 += 30; y3 -= 20;
    processPosition(mapper, x3, y3);
    processId(mapper, 3);
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_UP | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));

    // Last finger up.
    processMTSync(mapper);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));

    // Should not have sent any more keys or motions.
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasNotCalled());
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());
}

TEST_F(MultiTouchInputMapperTest, Process_NormalMultiTouchGesture_WithSlots) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | ID | SLOT);
    prepareVirtualKeys();
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    mFakeContext->setGlobalMetaState(AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_ON);

    NotifyMotionArgs motionArgs;

    // Two fingers down at once.
    int32_t x1 = 100, y1 = 125, x2 = 300, y2 = 500;
    processPosition(mapper, x1, y1);
    processId(mapper, 1);
    processSlot(mapper, 1);
    processPosition(mapper, x2, y2);
    processId(mapper, 2);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_DOWN | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    // Move.
    x1 += 10; y1 += 15; x2 += 5; y2 -= 10;
    processSlot(mapper, 0);
    processPosition(mapper, x1, y1);
    processSlot(mapper, 1);
    processPosition(mapper, x2, y2);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    // First finger up.
    x2 += 15; y2 -= 20;
    processSlot(mapper, 0);
    processId(mapper, -1);
    processSlot(mapper, 1);
    processPosition(mapper, x2, y2);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_UP | (0 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x1), toDisplayY(y1), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(1, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    // Move.
    x2 += 20; y2 -= 25;
    processPosition(mapper, x2, y2);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(1, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    // New finger down.
    int32_t x3 = 700, y3 = 300;
    processPosition(mapper, x2, y2);
    processSlot(mapper, 0);
    processId(mapper, 3);
    processPosition(mapper, x3, y3);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_DOWN | (0 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    // Second finger up.
    x3 += 30; y3 -= 20;
    processSlot(mapper, 1);
    processId(mapper, -1);
    processSlot(mapper, 0);
    processPosition(mapper, x3, y3);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_UP | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            motionArgs.action);
    ASSERT_EQ(size_t(2), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_EQ(1, motionArgs.pointerProperties[1].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[1].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[1],
            toDisplayX(x2), toDisplayY(y2), 1, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));

    // Last finger up.
    processId(mapper, -1);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_EQ(size_t(1), motionArgs.pointerCount);
    ASSERT_EQ(0, motionArgs.pointerProperties[0].id);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(x3), toDisplayY(y3), 1, 0, 0, 0, 0, 0, 0, 0));

    // Should not have sent any more keys or motions.
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasNotCalled());
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());
}

TEST_F(MultiTouchInputMapperTest, Process_AllAxes_WithDefaultCalibration) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | TOUCH | TOOL | PRESSURE | ORIENTATION | ID | MINOR | DISTANCE);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    // These calculations are based on the input device calibration documentation.
    int32_t rawX = 100;
    int32_t rawY = 200;
    int32_t rawTouchMajor = 7;
    int32_t rawTouchMinor = 6;
    int32_t rawToolMajor = 9;
    int32_t rawToolMinor = 8;
    int32_t rawPressure = 11;
    int32_t rawDistance = 0;
    int32_t rawOrientation = 3;
    int32_t id = 5;

    float x = toDisplayX(rawX);
    float y = toDisplayY(rawY);
    float pressure = float(rawPressure) / RAW_PRESSURE_MAX;
    float size = avg(rawTouchMajor, rawTouchMinor) / RAW_TOUCH_MAX;
    float toolMajor = float(rawToolMajor) * GEOMETRIC_SCALE;
    float toolMinor = float(rawToolMinor) * GEOMETRIC_SCALE;
    float touchMajor = float(rawTouchMajor) * GEOMETRIC_SCALE;
    float touchMinor = float(rawTouchMinor) * GEOMETRIC_SCALE;
    float orientation = float(rawOrientation) / RAW_ORIENTATION_MAX * M_PI_2;
    float distance = float(rawDistance);

    processPosition(mapper, rawX, rawY);
    processTouchMajor(mapper, rawTouchMajor);
    processTouchMinor(mapper, rawTouchMinor);
    processToolMajor(mapper, rawToolMajor);
    processToolMinor(mapper, rawToolMinor);
    processPressure(mapper, rawPressure);
    processOrientation(mapper, rawOrientation);
    processDistance(mapper, rawDistance);
    processId(mapper, id);
    processMTSync(mapper);
    processSync(mapper);

    NotifyMotionArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(0, args.pointerProperties[0].id);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            x, y, pressure, size, touchMajor, touchMinor, toolMajor, toolMinor,
            orientation, distance));
}

TEST_F(MultiTouchInputMapperTest, Process_TouchAndToolAxes_GeometricCalibration) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | TOUCH | TOOL | MINOR);
    addConfigurationProperty("touch.size.calibration", "geometric");
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    // These calculations are based on the input device calibration documentation.
    int32_t rawX = 100;
    int32_t rawY = 200;
    int32_t rawTouchMajor = 140;
    int32_t rawTouchMinor = 120;
    int32_t rawToolMajor = 180;
    int32_t rawToolMinor = 160;

    float x = toDisplayX(rawX);
    float y = toDisplayY(rawY);
    float size = avg(rawTouchMajor, rawTouchMinor) / RAW_TOUCH_MAX;
    float toolMajor = float(rawToolMajor) * GEOMETRIC_SCALE;
    float toolMinor = float(rawToolMinor) * GEOMETRIC_SCALE;
    float touchMajor = float(rawTouchMajor) * GEOMETRIC_SCALE;
    float touchMinor = float(rawTouchMinor) * GEOMETRIC_SCALE;

    processPosition(mapper, rawX, rawY);
    processTouchMajor(mapper, rawTouchMajor);
    processTouchMinor(mapper, rawTouchMinor);
    processToolMajor(mapper, rawToolMajor);
    processToolMinor(mapper, rawToolMinor);
    processMTSync(mapper);
    processSync(mapper);

    NotifyMotionArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            x, y, 1.0f, size, touchMajor, touchMinor, toolMajor, toolMinor, 0, 0));
}

TEST_F(MultiTouchInputMapperTest, Process_TouchAndToolAxes_SummedLinearCalibration) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | TOUCH | TOOL);
    addConfigurationProperty("touch.size.calibration", "diameter");
    addConfigurationProperty("touch.size.scale", "10");
    addConfigurationProperty("touch.size.bias", "160");
    addConfigurationProperty("touch.size.isSummed", "1");
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    // These calculations are based on the input device calibration documentation.
    // Note: We only provide a single common touch/tool value because the device is assumed
    //       not to emit separate values for each pointer (isSummed = 1).
    int32_t rawX = 100;
    int32_t rawY = 200;
    int32_t rawX2 = 150;
    int32_t rawY2 = 250;
    int32_t rawTouchMajor = 5;
    int32_t rawToolMajor = 8;

    float x = toDisplayX(rawX);
    float y = toDisplayY(rawY);
    float x2 = toDisplayX(rawX2);
    float y2 = toDisplayY(rawY2);
    float size = float(rawTouchMajor) / 2 / RAW_TOUCH_MAX;
    float touch = float(rawTouchMajor) / 2 * 10.0f + 160.0f;
    float tool = float(rawToolMajor) / 2 * 10.0f + 160.0f;

    processPosition(mapper, rawX, rawY);
    processTouchMajor(mapper, rawTouchMajor);
    processToolMajor(mapper, rawToolMajor);
    processMTSync(mapper);
    processPosition(mapper, rawX2, rawY2);
    processTouchMajor(mapper, rawTouchMajor);
    processToolMajor(mapper, rawToolMajor);
    processMTSync(mapper);
    processSync(mapper);

    NotifyMotionArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, args.action);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_DOWN | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
            args.action);
    ASSERT_EQ(size_t(2), args.pointerCount);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            x, y, 1.0f, size, touch, touch, tool, tool, 0, 0));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[1],
            x2, y2, 1.0f, size, touch, touch, tool, tool, 0, 0));
}

TEST_F(MultiTouchInputMapperTest, Process_TouchAndToolAxes_AreaCalibration) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | TOUCH | TOOL);
    addConfigurationProperty("touch.size.calibration", "area");
    addConfigurationProperty("touch.size.scale", "43");
    addConfigurationProperty("touch.size.bias", "3");
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    // These calculations are based on the input device calibration documentation.
    int32_t rawX = 100;
    int32_t rawY = 200;
    int32_t rawTouchMajor = 5;
    int32_t rawToolMajor = 8;

    float x = toDisplayX(rawX);
    float y = toDisplayY(rawY);
    float size = float(rawTouchMajor) / RAW_TOUCH_MAX;
    float touch = sqrtf(rawTouchMajor) * 43.0f + 3.0f;
    float tool = sqrtf(rawToolMajor) * 43.0f + 3.0f;

    processPosition(mapper, rawX, rawY);
    processTouchMajor(mapper, rawTouchMajor);
    processToolMajor(mapper, rawToolMajor);
    processMTSync(mapper);
    processSync(mapper);

    NotifyMotionArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            x, y, 1.0f, size, touch, touch, tool, tool, 0, 0));
}

TEST_F(MultiTouchInputMapperTest, Process_PressureAxis_AmplitudeCalibration) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | PRESSURE);
    addConfigurationProperty("touch.pressure.calibration", "amplitude");
    addConfigurationProperty("touch.pressure.scale", "0.01");
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    InputDeviceInfo info;
    mapper.populateDeviceInfo(&info);
    ASSERT_NO_FATAL_FAILURE(assertMotionRange(info,
            AINPUT_MOTION_RANGE_PRESSURE, AINPUT_SOURCE_TOUCHSCREEN,
            0.0f, RAW_PRESSURE_MAX * 0.01, 0.0f, 0.0f));

    // These calculations are based on the input device calibration documentation.
    int32_t rawX = 100;
    int32_t rawY = 200;
    int32_t rawPressure = 60;

    float x = toDisplayX(rawX);
    float y = toDisplayY(rawY);
    float pressure = float(rawPressure) * 0.01f;

    processPosition(mapper, rawX, rawY);
    processPressure(mapper, rawPressure);
    processMTSync(mapper);
    processSync(mapper);

    NotifyMotionArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0],
            x, y, pressure, 0, 0, 0, 0, 0, 0, 0));
}

TEST_F(MultiTouchInputMapperTest, Process_ShouldHandleAllButtons) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | ID | SLOT);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    NotifyMotionArgs motionArgs;
    NotifyKeyArgs keyArgs;

    processId(mapper, 1);
    processPosition(mapper, 100, 200);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    // press BTN_LEFT, release BTN_LEFT
    processKey(mapper, BTN_LEFT, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_PRIMARY, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_PRIMARY, motionArgs.buttonState);

    processKey(mapper, BTN_LEFT, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    // press BTN_RIGHT + BTN_MIDDLE, release BTN_RIGHT, release BTN_MIDDLE
    processKey(mapper, BTN_RIGHT, 1);
    processKey(mapper, BTN_MIDDLE, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_SECONDARY | AMOTION_EVENT_BUTTON_TERTIARY,
            motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_TERTIARY, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_SECONDARY | AMOTION_EVENT_BUTTON_TERTIARY,
            motionArgs.buttonState);

    processKey(mapper, BTN_RIGHT, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_TERTIARY, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_TERTIARY, motionArgs.buttonState);

    processKey(mapper, BTN_MIDDLE, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    // press BTN_BACK, release BTN_BACK
    processKey(mapper, BTN_BACK, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);

    processKey(mapper, BTN_BACK, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    // press BTN_SIDE, release BTN_SIDE
    processKey(mapper, BTN_SIDE, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_BACK, motionArgs.buttonState);

    processKey(mapper, BTN_SIDE, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_BACK, keyArgs.keyCode);

    // press BTN_FORWARD, release BTN_FORWARD
    processKey(mapper, BTN_FORWARD, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);

    processKey(mapper, BTN_FORWARD, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);

    // press BTN_EXTRA, release BTN_EXTRA
    processKey(mapper, BTN_EXTRA, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_FORWARD, motionArgs.buttonState);

    processKey(mapper, BTN_EXTRA, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasCalled(&keyArgs));
    ASSERT_EQ(AKEY_EVENT_ACTION_UP, keyArgs.action);
    ASSERT_EQ(AKEYCODE_FORWARD, keyArgs.keyCode);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyKeyWasNotCalled());

    // press BTN_STYLUS, release BTN_STYLUS
    processKey(mapper, BTN_STYLUS, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_STYLUS_PRIMARY, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_STYLUS_PRIMARY, motionArgs.buttonState);

    processKey(mapper, BTN_STYLUS, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    // press BTN_STYLUS2, release BTN_STYLUS2
    processKey(mapper, BTN_STYLUS2, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_STYLUS_SECONDARY, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_PRESS, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_BUTTON_STYLUS_SECONDARY, motionArgs.buttonState);

    processKey(mapper, BTN_STYLUS2, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_BUTTON_RELEASE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);

    // release touch
    processId(mapper, -1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_EQ(0, motionArgs.buttonState);
}

TEST_F(MultiTouchInputMapperTest, Process_ShouldHandleAllToolTypes) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | ID | SLOT | TOOL_TYPE);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    NotifyMotionArgs motionArgs;

    // default tool type is finger
    processId(mapper, 1);
    processPosition(mapper, 100, 200);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // eraser
    processKey(mapper, BTN_TOOL_RUBBER, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_ERASER, motionArgs.pointerProperties[0].toolType);

    // stylus
    processKey(mapper, BTN_TOOL_RUBBER, 0);
    processKey(mapper, BTN_TOOL_PEN, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_STYLUS, motionArgs.pointerProperties[0].toolType);

    // brush
    processKey(mapper, BTN_TOOL_PEN, 0);
    processKey(mapper, BTN_TOOL_BRUSH, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_STYLUS, motionArgs.pointerProperties[0].toolType);

    // pencil
    processKey(mapper, BTN_TOOL_BRUSH, 0);
    processKey(mapper, BTN_TOOL_PENCIL, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_STYLUS, motionArgs.pointerProperties[0].toolType);

    // air-brush
    processKey(mapper, BTN_TOOL_PENCIL, 0);
    processKey(mapper, BTN_TOOL_AIRBRUSH, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_STYLUS, motionArgs.pointerProperties[0].toolType);

    // mouse
    processKey(mapper, BTN_TOOL_AIRBRUSH, 0);
    processKey(mapper, BTN_TOOL_MOUSE, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_MOUSE, motionArgs.pointerProperties[0].toolType);

    // lens
    processKey(mapper, BTN_TOOL_MOUSE, 0);
    processKey(mapper, BTN_TOOL_LENS, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_MOUSE, motionArgs.pointerProperties[0].toolType);

    // double-tap
    processKey(mapper, BTN_TOOL_LENS, 0);
    processKey(mapper, BTN_TOOL_DOUBLETAP, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // triple-tap
    processKey(mapper, BTN_TOOL_DOUBLETAP, 0);
    processKey(mapper, BTN_TOOL_TRIPLETAP, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // quad-tap
    processKey(mapper, BTN_TOOL_TRIPLETAP, 0);
    processKey(mapper, BTN_TOOL_QUADTAP, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // finger
    processKey(mapper, BTN_TOOL_QUADTAP, 0);
    processKey(mapper, BTN_TOOL_FINGER, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // stylus trumps finger
    processKey(mapper, BTN_TOOL_PEN, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_STYLUS, motionArgs.pointerProperties[0].toolType);

    // eraser trumps stylus
    processKey(mapper, BTN_TOOL_RUBBER, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_ERASER, motionArgs.pointerProperties[0].toolType);

    // mouse trumps eraser
    processKey(mapper, BTN_TOOL_MOUSE, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_MOUSE, motionArgs.pointerProperties[0].toolType);

    // MT tool type trumps BTN tool types: MT_TOOL_FINGER
    processToolType(mapper, MT_TOOL_FINGER); // this is the first time we send MT_TOOL_TYPE
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // MT tool type trumps BTN tool types: MT_TOOL_PEN
    processToolType(mapper, MT_TOOL_PEN);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_STYLUS, motionArgs.pointerProperties[0].toolType);

    // back to default tool type
    processToolType(mapper, -1); // use a deliberately undefined tool type, for testing
    processKey(mapper, BTN_TOOL_MOUSE, 0);
    processKey(mapper, BTN_TOOL_RUBBER, 0);
    processKey(mapper, BTN_TOOL_PEN, 0);
    processKey(mapper, BTN_TOOL_FINGER, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
}

TEST_F(MultiTouchInputMapperTest, Process_WhenBtnTouchPresent_HoversIfItsValueIsZero) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | ID | SLOT);
    mFakeEventHub->addKey(EVENTHUB_ID, BTN_TOUCH, 0, AKEYCODE_UNKNOWN, 0);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    NotifyMotionArgs motionArgs;

    // initially hovering because BTN_TOUCH not sent yet, pressure defaults to 0
    processId(mapper, 1);
    processPosition(mapper, 100, 200);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_ENTER, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(100), toDisplayY(200), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(100), toDisplayY(200), 0, 0, 0, 0, 0, 0, 0, 0));

    // move a little
    processPosition(mapper, 150, 250);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    // down when BTN_TOUCH is pressed, pressure defaults to 1
    processKey(mapper, BTN_TOUCH, 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_EXIT, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 1, 0, 0, 0, 0, 0, 0, 0));

    // up when BTN_TOUCH is released, hover restored
    processKey(mapper, BTN_TOUCH, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 1, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_ENTER, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    // exit hover when pointer goes away
    processId(mapper, -1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_EXIT, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));
}

TEST_F(MultiTouchInputMapperTest, Process_WhenAbsMTPressureIsPresent_HoversIfItsValueIsZero) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | ID | SLOT | PRESSURE);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    NotifyMotionArgs motionArgs;

    // initially hovering because pressure is 0
    processId(mapper, 1);
    processPosition(mapper, 100, 200);
    processPressure(mapper, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_ENTER, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(100), toDisplayY(200), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(100), toDisplayY(200), 0, 0, 0, 0, 0, 0, 0, 0));

    // move a little
    processPosition(mapper, 150, 250);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    // down when pressure becomes non-zero
    processPressure(mapper, RAW_PRESSURE_MAX);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_EXIT, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 1, 0, 0, 0, 0, 0, 0, 0));

    // up when pressure becomes 0, hover restored
    processPressure(mapper, 0);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 1, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_ENTER, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));

    // exit hover when pointer goes away
    processId(mapper, -1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_EXIT, motionArgs.action);
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(motionArgs.pointerCoords[0],
            toDisplayX(150), toDisplayY(250), 0, 0, 0, 0, 0, 0, 0, 0));
}

/**
 * Set the input device port <--> display port associations, and check that the
 * events are routed to the display that matches the display port.
 * This can be checked by looking at the displayId of the resulting NotifyMotionArgs.
 */
TEST_F(MultiTouchInputMapperTest, Configure_AssignsDisplayPort) {
    const std::string usb2 = "USB2";
    const uint8_t hdmi1 = 0;
    const uint8_t hdmi2 = 1;
    const std::string secondaryUniqueId = "uniqueId2";
    constexpr ViewportType type = ViewportType::VIEWPORT_EXTERNAL;

    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareAxes(POSITION);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    mFakePolicy->addInputPortAssociation(DEVICE_LOCATION, hdmi1);
    mFakePolicy->addInputPortAssociation(usb2, hdmi2);

    // We are intentionally not adding the viewport for display 1 yet. Since the port association
    // for this input device is specified, and the matching viewport is not present,
    // the input device should be disabled (at the mapper level).

    // Add viewport for display 2 on hdmi2
    prepareSecondaryDisplay(type, hdmi2);
    // Send a touch event
    processPosition(mapper, 100, 100);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());

    // Add viewport for display 1 on hdmi1
    prepareDisplay(DISPLAY_ORIENTATION_0, hdmi1);
    // Send a touch event again
    processPosition(mapper, 100, 100);
    processSync(mapper);

    NotifyMotionArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(DISPLAY_ID, args.displayId);
}

TEST_F(MultiTouchInputMapperTest, Process_Pointer_ShouldHandleDisplayId) {
    // Setup for second display.
    sp<FakePointerController> fakePointerController = new FakePointerController();
    fakePointerController->setBounds(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
    fakePointerController->setPosition(100, 200);
    fakePointerController->setButtonState(0);
    mFakePolicy->setPointerController(mDevice->getId(), fakePointerController);

    mFakePolicy->setDefaultPointerDisplayId(SECONDARY_DISPLAY_ID);
    prepareSecondaryDisplay(ViewportType::VIEWPORT_EXTERNAL);

    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    // Check source is mouse that would obtain the PointerController.
    ASSERT_EQ(AINPUT_SOURCE_MOUSE, mapper.getSources());

    NotifyMotionArgs motionArgs;
    processPosition(mapper, 100, 100);
    processSync(mapper);

    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_HOVER_MOVE, motionArgs.action);
    ASSERT_EQ(SECONDARY_DISPLAY_ID, motionArgs.displayId);
}

TEST_F(MultiTouchInputMapperTest, Process_Pointer_ShowTouches) {
    // Setup the first touch screen device.
    prepareAxes(POSITION | ID | SLOT);
    addConfigurationProperty("touch.deviceType", "touchScreen");
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    // Create the second touch screen device, and enable multi fingers.
    const std::string USB2 = "USB2";
    constexpr int32_t SECOND_DEVICE_ID = DEVICE_ID + 1;
    constexpr int32_t SECOND_EVENTHUB_ID = EVENTHUB_ID + 1;
    InputDeviceIdentifier identifier;
    identifier.name = "TOUCHSCREEN2";
    identifier.location = USB2;
    std::unique_ptr<InputDevice> device2 =
            std::make_unique<InputDevice>(mFakeContext, SECOND_DEVICE_ID, DEVICE_GENERATION,
                                          identifier);
    mFakeEventHub->addDevice(SECOND_EVENTHUB_ID, DEVICE_NAME, 0 /*classes*/);
    mFakeEventHub->addAbsoluteAxis(SECOND_EVENTHUB_ID, ABS_MT_POSITION_X, RAW_X_MIN, RAW_X_MAX,
                                   0 /*flat*/, 0 /*fuzz*/);
    mFakeEventHub->addAbsoluteAxis(SECOND_EVENTHUB_ID, ABS_MT_POSITION_Y, RAW_Y_MIN, RAW_Y_MAX,
                                   0 /*flat*/, 0 /*fuzz*/);
    mFakeEventHub->addAbsoluteAxis(SECOND_EVENTHUB_ID, ABS_MT_TRACKING_ID, RAW_ID_MIN, RAW_ID_MAX,
                                   0 /*flat*/, 0 /*fuzz*/);
    mFakeEventHub->addAbsoluteAxis(SECOND_EVENTHUB_ID, ABS_MT_SLOT, RAW_SLOT_MIN, RAW_SLOT_MAX,
                                   0 /*flat*/, 0 /*fuzz*/);
    mFakeEventHub->setAbsoluteAxisValue(SECOND_EVENTHUB_ID, ABS_MT_SLOT, 0 /*value*/);
    mFakeEventHub->addConfigurationProperty(SECOND_EVENTHUB_ID, String8("touch.deviceType"),
                                            String8("touchScreen"));

    // Setup the second touch screen device.
    MultiTouchInputMapper& mapper2 = device2->addMapper<MultiTouchInputMapper>(SECOND_EVENTHUB_ID);
    device2->configure(ARBITRARY_TIME, mFakePolicy->getReaderConfiguration(), 0 /*changes*/);
    device2->reset(ARBITRARY_TIME);

    // Setup PointerController.
    sp<FakePointerController> fakePointerController = new FakePointerController();
    mFakePolicy->setPointerController(mDevice->getId(), fakePointerController);
    mFakePolicy->setPointerController(SECOND_DEVICE_ID, fakePointerController);

    // Setup policy for associated displays and show touches.
    const uint8_t hdmi1 = 0;
    const uint8_t hdmi2 = 1;
    mFakePolicy->addInputPortAssociation(DEVICE_LOCATION, hdmi1);
    mFakePolicy->addInputPortAssociation(USB2, hdmi2);
    mFakePolicy->setShowTouches(true);

    // Create displays.
    prepareDisplay(DISPLAY_ORIENTATION_0, hdmi1);
    prepareSecondaryDisplay(ViewportType::VIEWPORT_EXTERNAL, hdmi2);

    // Default device will reconfigure above, need additional reconfiguration for another device.
    device2->configure(ARBITRARY_TIME, mFakePolicy->getReaderConfiguration(),
            InputReaderConfiguration::CHANGE_DISPLAY_INFO);

    // Two fingers down at default display.
    int32_t x1 = 100, y1 = 125, x2 = 300, y2 = 500;
    processPosition(mapper, x1, y1);
    processId(mapper, 1);
    processSlot(mapper, 1);
    processPosition(mapper, x2, y2);
    processId(mapper, 2);
    processSync(mapper);

    std::map<int32_t, std::vector<int32_t>>::const_iterator iter =
            fakePointerController->getSpots().find(DISPLAY_ID);
    ASSERT_TRUE(iter != fakePointerController->getSpots().end());
    ASSERT_EQ(size_t(2), iter->second.size());

    // Two fingers down at second display.
    processPosition(mapper2, x1, y1);
    processId(mapper2, 1);
    processSlot(mapper2, 1);
    processPosition(mapper2, x2, y2);
    processId(mapper2, 2);
    processSync(mapper2);

    iter = fakePointerController->getSpots().find(SECONDARY_DISPLAY_ID);
    ASSERT_TRUE(iter != fakePointerController->getSpots().end());
    ASSERT_EQ(size_t(2), iter->second.size());
}

TEST_F(MultiTouchInputMapperTest, VideoFrames_ReceivedByListener) {
    prepareAxes(POSITION);
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    NotifyMotionArgs motionArgs;
    // Unrotated video frame
    TouchVideoFrame frame(3, 2, {1, 2, 3, 4, 5, 6}, {1, 2});
    std::vector<TouchVideoFrame> frames{frame};
    mFakeEventHub->setVideoFrames({{EVENTHUB_ID, frames}});
    processPosition(mapper, 100, 200);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(frames, motionArgs.videoFrames);

    // Subsequent touch events should not have any videoframes
    // This is implemented separately in FakeEventHub,
    // but that should match the behaviour of TouchVideoDevice.
    processPosition(mapper, 200, 200);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(std::vector<TouchVideoFrame>(), motionArgs.videoFrames);
}

TEST_F(MultiTouchInputMapperTest, VideoFrames_AreRotated) {
    prepareAxes(POSITION);
    addConfigurationProperty("touch.deviceType", "touchScreen");
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();
    // Unrotated video frame
    TouchVideoFrame frame(3, 2, {1, 2, 3, 4, 5, 6}, {1, 2});
    NotifyMotionArgs motionArgs;

    // Test all 4 orientations
    for (int32_t orientation : {DISPLAY_ORIENTATION_0, DISPLAY_ORIENTATION_90,
             DISPLAY_ORIENTATION_180, DISPLAY_ORIENTATION_270}) {
        SCOPED_TRACE("Orientation " + StringPrintf("%i", orientation));
        clearViewports();
        prepareDisplay(orientation);
        std::vector<TouchVideoFrame> frames{frame};
        mFakeEventHub->setVideoFrames({{EVENTHUB_ID, frames}});
        processPosition(mapper, 100, 200);
        processSync(mapper);
        ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
        frames[0].rotate(orientation);
        ASSERT_EQ(frames, motionArgs.videoFrames);
    }
}

TEST_F(MultiTouchInputMapperTest, VideoFrames_MultipleFramesAreRotated) {
    prepareAxes(POSITION);
    addConfigurationProperty("touch.deviceType", "touchScreen");
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();
    // Unrotated video frames. There's no rule that they must all have the same dimensions,
    // so mix these.
    TouchVideoFrame frame1(3, 2, {1, 2, 3, 4, 5, 6}, {1, 2});
    TouchVideoFrame frame2(3, 3, {0, 1, 2, 3, 4, 5, 6, 7, 8}, {1, 3});
    TouchVideoFrame frame3(2, 2, {10, 20, 10, 0}, {1, 4});
    std::vector<TouchVideoFrame> frames{frame1, frame2, frame3};
    NotifyMotionArgs motionArgs;

    prepareDisplay(DISPLAY_ORIENTATION_90);
    mFakeEventHub->setVideoFrames({{EVENTHUB_ID, frames}});
    processPosition(mapper, 100, 200);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    std::for_each(frames.begin(), frames.end(),
            [](TouchVideoFrame& frame) { frame.rotate(DISPLAY_ORIENTATION_90); });
    ASSERT_EQ(frames, motionArgs.videoFrames);
}

/**
 * If we had defined port associations, but the viewport is not ready, the touch device would be
 * expected to be disabled, and it should be enabled after the viewport has found.
 */
TEST_F(MultiTouchInputMapperTest, Configure_EnabledForAssociatedDisplay) {
    constexpr uint8_t hdmi2 = 1;
    const std::string secondaryUniqueId = "uniqueId2";
    constexpr ViewportType type = ViewportType::VIEWPORT_EXTERNAL;

    mFakePolicy->addInputPortAssociation(DEVICE_LOCATION, hdmi2);

    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareAxes(POSITION);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    ASSERT_EQ(mDevice->isEnabled(), false);

    // Add display on hdmi2, the device should be enabled and can receive touch event.
    prepareSecondaryDisplay(type, hdmi2);
    ASSERT_EQ(mDevice->isEnabled(), true);

    // Send a touch event.
    processPosition(mapper, 100, 100);
    processSync(mapper);

    NotifyMotionArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_EQ(SECONDARY_DISPLAY_ID, args.displayId);
}



TEST_F(MultiTouchInputMapperTest, Process_ShouldHandleSingleTouch) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | ID | SLOT | TOOL_TYPE);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    NotifyMotionArgs motionArgs;

    constexpr int32_t x1 = 100, y1 = 200, x2 = 120, y2 = 220, x3 = 140, y3 = 240;
    // finger down
    processId(mapper, 1);
    processPosition(mapper, x1, y1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // finger move
    processId(mapper, 1);
    processPosition(mapper, x2, y2);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // finger up.
    processId(mapper, -1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_UP, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // new finger down
    processId(mapper, 1);
    processPosition(mapper, x3, y3);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
}

/**
 * Test touch should be canceled when received the MT_TOOL_PALM event, and the following MOVE and
 * UP events should be ignored.
 */
TEST_F(MultiTouchInputMapperTest, Process_ShouldHandlePalmToolType) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | ID | SLOT | TOOL_TYPE);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    NotifyMotionArgs motionArgs;

    // default tool type is finger
    constexpr int32_t x1 = 100, y1 = 200, x2 = 120, y2 = 220, x3 = 140, y3 = 240;
    processId(mapper, 1);
    processPosition(mapper, x1, y1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // Tool changed to MT_TOOL_PALM expect sending the cancel event.
    processToolType(mapper, MT_TOOL_PALM);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_CANCEL, motionArgs.action);

    // Ignore the following MOVE and UP events if had detect a palm event.
    processId(mapper, 1);
    processPosition(mapper, x2, y2);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());

    // finger up.
    processId(mapper, -1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());

    // new finger down
    processToolType(mapper, MT_TOOL_FINGER);
    processId(mapper, 1);
    processPosition(mapper, x3, y3);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
}

/**
 * Test multi-touch should be canceled when received the MT_TOOL_PALM event from some finger,
 * and could be allowed again after all non-MT_TOOL_PALM is release and the new point is
 * MT_TOOL_FINGER.
 */
TEST_F(MultiTouchInputMapperTest, Process_ShouldHandlePalmToolType2) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION | ID | SLOT | TOOL_TYPE);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    NotifyMotionArgs motionArgs;

    // default tool type is finger
    constexpr int32_t x1 = 100, y1 = 200, x2 = 120, y2 = 220, x3 = 140, y3 = 240;
    processId(mapper, 1);
    processPosition(mapper, x1, y1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // Second finger down.
    processSlot(mapper, 1);
    processPosition(mapper, x2, y2);
    processId(mapper, 2);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_POINTER_DOWN | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
              motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);

    // If the tool type of the first pointer changes to MT_TOOL_PALM,
    // the entire gesture should be aborted, so we expect to receive ACTION_CANCEL.
    processSlot(mapper, 0);
    processId(mapper, 1);
    processToolType(mapper, MT_TOOL_PALM);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_CANCEL, motionArgs.action);

    // Ignore the following MOVE and UP events if had detect a palm event.
    processSlot(mapper, 1);
    processId(mapper, 2);
    processPosition(mapper, x3, y3);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());

    // second finger up.
    processId(mapper, -1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());

    // first finger move, but still in palm
    processSlot(mapper, 0);
    processId(mapper, 1);
    processPosition(mapper, x1 - 1, y1 - 1);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());

    // second finger down, expect as new finger down.
    processSlot(mapper, 1);
    processId(mapper, 2);
    processPosition(mapper, x2, y2);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(AMOTION_EVENT_ACTION_DOWN, motionArgs.action);
    ASSERT_EQ(AMOTION_EVENT_TOOL_TYPE_FINGER, motionArgs.pointerProperties[0].toolType);
}

// --- MultiTouchInputMapperTest_ExternalDevice ---

class MultiTouchInputMapperTest_ExternalDevice : public MultiTouchInputMapperTest {
protected:
    virtual void SetUp() override {
        InputMapperTest::SetUp(DEVICE_CLASSES | INPUT_DEVICE_CLASS_EXTERNAL);
    }
};

/**
 * Expect fallback to internal viewport if device is external and external viewport is not present.
 */
TEST_F(MultiTouchInputMapperTest_ExternalDevice, Viewports_Fallback) {
    prepareAxes(POSITION);
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, mapper.getSources());

    NotifyMotionArgs motionArgs;

    // Expect the event to be sent to the internal viewport,
    // because an external viewport is not present.
    processPosition(mapper, 100, 100);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(ADISPLAY_ID_DEFAULT, motionArgs.displayId);

    // Expect the event to be sent to the external viewport if it is present.
    prepareSecondaryDisplay(ViewportType::VIEWPORT_EXTERNAL);
    processPosition(mapper, 100, 100);
    processSync(mapper);
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&motionArgs));
    ASSERT_EQ(SECONDARY_DISPLAY_ID, motionArgs.displayId);
}

/**
 * Test touch should not work if outside of surface.
 */
class MultiTouchInputMapperTest_SurfaceRange : public MultiTouchInputMapperTest {
protected:
    void halfDisplayToCenterHorizontal(int32_t orientation) {
        std::optional<DisplayViewport> internalViewport =
                mFakePolicy->getDisplayViewportByType(ViewportType::VIEWPORT_INTERNAL);

        // Half display to (width/4, 0, width * 3/4, height) to make display has offset.
        internalViewport->orientation = orientation;
        if (orientation == DISPLAY_ORIENTATION_90 || orientation == DISPLAY_ORIENTATION_270) {
            internalViewport->logicalLeft = 0;
            internalViewport->logicalTop = 0;
            internalViewport->logicalRight = DISPLAY_HEIGHT;
            internalViewport->logicalBottom = DISPLAY_WIDTH / 2;

            internalViewport->physicalLeft = 0;
            internalViewport->physicalTop = DISPLAY_WIDTH / 4;
            internalViewport->physicalRight = DISPLAY_HEIGHT;
            internalViewport->physicalBottom = DISPLAY_WIDTH * 3 / 4;

            internalViewport->deviceWidth = DISPLAY_HEIGHT;
            internalViewport->deviceHeight = DISPLAY_WIDTH;
        } else {
            internalViewport->logicalLeft = 0;
            internalViewport->logicalTop = 0;
            internalViewport->logicalRight = DISPLAY_WIDTH / 2;
            internalViewport->logicalBottom = DISPLAY_HEIGHT;

            internalViewport->physicalLeft = DISPLAY_WIDTH / 4;
            internalViewport->physicalTop = 0;
            internalViewport->physicalRight = DISPLAY_WIDTH * 3 / 4;
            internalViewport->physicalBottom = DISPLAY_HEIGHT;

            internalViewport->deviceWidth = DISPLAY_WIDTH;
            internalViewport->deviceHeight = DISPLAY_HEIGHT;
        }

        mFakePolicy->updateViewport(internalViewport.value());
        configureDevice(InputReaderConfiguration::CHANGE_DISPLAY_INFO);
    }

    void processPositionAndVerify(MultiTouchInputMapper& mapper, int32_t xInside, int32_t yInside,
                                  int32_t xOutside, int32_t yOutside, int32_t xExpected,
                                  int32_t yExpected) {
        // touch on outside area should not work.
        processPosition(mapper, toRawX(xOutside), toRawY(yOutside));
        processSync(mapper);
        ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasNotCalled());

        // touch on inside area should receive the event.
        NotifyMotionArgs args;
        processPosition(mapper, toRawX(xInside), toRawY(yInside));
        processSync(mapper);
        ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
        ASSERT_NEAR(xExpected, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_X), 1);
        ASSERT_NEAR(yExpected, args.pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_Y), 1);

        // Reset.
        mapper.reset(ARBITRARY_TIME);
    }
};

TEST_F(MultiTouchInputMapperTest_SurfaceRange, Viewports_SurfaceRange) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    // Touch on center of normal display should work.
    const int32_t x = DISPLAY_WIDTH / 4;
    const int32_t y = DISPLAY_HEIGHT / 2;
    processPosition(mapper, toRawX(x), toRawY(y));
    processSync(mapper);
    NotifyMotionArgs args;
    ASSERT_NO_FATAL_FAILURE(mFakeListener->assertNotifyMotionWasCalled(&args));
    ASSERT_NO_FATAL_FAILURE(assertPointerCoords(args.pointerCoords[0], x, y, 1.0f, 0.0f, 0.0f, 0.0f,
                                                0.0f, 0.0f, 0.0f, 0.0f));
    // Reset.
    mapper.reset(ARBITRARY_TIME);

    // Let physical display be different to device, and make surface and physical could be 1:1.
    halfDisplayToCenterHorizontal(DISPLAY_ORIENTATION_0);

    const int32_t xExpected = (x + 1) - (DISPLAY_WIDTH / 4);
    const int32_t yExpected = y;
    processPositionAndVerify(mapper, x - 1, y, x + 1, y, xExpected, yExpected);
}

TEST_F(MultiTouchInputMapperTest_SurfaceRange, Viewports_SurfaceRange_90) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    // Half display to (width/4, 0, width * 3/4, height) and rotate 90-degrees.
    halfDisplayToCenterHorizontal(DISPLAY_ORIENTATION_90);

    const int32_t x = DISPLAY_WIDTH / 4;
    const int32_t y = DISPLAY_HEIGHT / 2;

    // expect x/y = swap x/y then reverse y.
    const int32_t xExpected = y;
    const int32_t yExpected = (DISPLAY_WIDTH * 3 / 4) - (x + 1);
    processPositionAndVerify(mapper, x - 1, y, x + 1, y, xExpected, yExpected);
}

TEST_F(MultiTouchInputMapperTest_SurfaceRange, Viewports_SurfaceRange_270) {
    addConfigurationProperty("touch.deviceType", "touchScreen");
    prepareDisplay(DISPLAY_ORIENTATION_0);
    prepareAxes(POSITION);
    MultiTouchInputMapper& mapper = addMapperAndConfigure<MultiTouchInputMapper>();

    // Half display to (width/4, 0, width * 3/4, height) and rotate 270-degrees.
    halfDisplayToCenterHorizontal(DISPLAY_ORIENTATION_270);

    const int32_t x = DISPLAY_WIDTH / 4;
    const int32_t y = DISPLAY_HEIGHT / 2;

    // expect x/y = swap x/y then reverse x.
    constexpr int32_t xExpected = DISPLAY_HEIGHT - y;
    constexpr int32_t yExpected = (x + 1) - DISPLAY_WIDTH / 4;
    processPositionAndVerify(mapper, x - 1, y, x + 1, y, xExpected, yExpected);
}
} // namespace android
