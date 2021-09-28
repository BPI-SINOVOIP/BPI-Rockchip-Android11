/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "../Macros.h"

#include "JoystickInputMapper.h"

namespace android {

JoystickInputMapper::JoystickInputMapper(InputDeviceContext& deviceContext)
      : InputMapper(deviceContext) {}

JoystickInputMapper::~JoystickInputMapper() {}

uint32_t JoystickInputMapper::getSources() {
    return AINPUT_SOURCE_JOYSTICK;
}

void JoystickInputMapper::populateDeviceInfo(InputDeviceInfo* info) {
    InputMapper::populateDeviceInfo(info);

    for (size_t i = 0; i < mAxes.size(); i++) {
        const Axis& axis = mAxes.valueAt(i);
        addMotionRange(axis.axisInfo.axis, axis, info);

        if (axis.axisInfo.mode == AxisInfo::MODE_SPLIT) {
            addMotionRange(axis.axisInfo.highAxis, axis, info);
        }
    }
}

void JoystickInputMapper::addMotionRange(int32_t axisId, const Axis& axis, InputDeviceInfo* info) {
    info->addMotionRange(axisId, AINPUT_SOURCE_JOYSTICK, axis.min, axis.max, axis.flat, axis.fuzz,
                         axis.resolution);
    /* In order to ease the transition for developers from using the old axes
     * to the newer, more semantically correct axes, we'll continue to register
     * the old axes as duplicates of their corresponding new ones.  */
    int32_t compatAxis = getCompatAxis(axisId);
    if (compatAxis >= 0) {
        info->addMotionRange(compatAxis, AINPUT_SOURCE_JOYSTICK, axis.min, axis.max, axis.flat,
                             axis.fuzz, axis.resolution);
    }
}

/* A mapping from axes the joystick actually has to the axes that should be
 * artificially created for compatibility purposes.
 * Returns -1 if no compatibility axis is needed. */
int32_t JoystickInputMapper::getCompatAxis(int32_t axis) {
    switch (axis) {
        case AMOTION_EVENT_AXIS_LTRIGGER:
            return AMOTION_EVENT_AXIS_BRAKE;
        case AMOTION_EVENT_AXIS_RTRIGGER:
            return AMOTION_EVENT_AXIS_GAS;
    }
    return -1;
}

void JoystickInputMapper::dump(std::string& dump) {
    dump += INDENT2 "Joystick Input Mapper:\n";

    dump += INDENT3 "Axes:\n";
    size_t numAxes = mAxes.size();
    for (size_t i = 0; i < numAxes; i++) {
        const Axis& axis = mAxes.valueAt(i);
        const char* label = getAxisLabel(axis.axisInfo.axis);
        if (label) {
            dump += StringPrintf(INDENT4 "%s", label);
        } else {
            dump += StringPrintf(INDENT4 "%d", axis.axisInfo.axis);
        }
        if (axis.axisInfo.mode == AxisInfo::MODE_SPLIT) {
            label = getAxisLabel(axis.axisInfo.highAxis);
            if (label) {
                dump += StringPrintf(" / %s (split at %d)", label, axis.axisInfo.splitValue);
            } else {
                dump += StringPrintf(" / %d (split at %d)", axis.axisInfo.highAxis,
                                     axis.axisInfo.splitValue);
            }
        } else if (axis.axisInfo.mode == AxisInfo::MODE_INVERT) {
            dump += " (invert)";
        }

        dump += StringPrintf(": min=%0.5f, max=%0.5f, flat=%0.5f, fuzz=%0.5f, resolution=%0.5f\n",
                             axis.min, axis.max, axis.flat, axis.fuzz, axis.resolution);
        dump += StringPrintf(INDENT4 "  scale=%0.5f, offset=%0.5f, "
                                     "highScale=%0.5f, highOffset=%0.5f\n",
                             axis.scale, axis.offset, axis.highScale, axis.highOffset);
        dump += StringPrintf(INDENT4 "  rawAxis=%d, rawMin=%d, rawMax=%d, "
                                     "rawFlat=%d, rawFuzz=%d, rawResolution=%d\n",
                             mAxes.keyAt(i), axis.rawAxisInfo.minValue, axis.rawAxisInfo.maxValue,
                             axis.rawAxisInfo.flat, axis.rawAxisInfo.fuzz,
                             axis.rawAxisInfo.resolution);
    }
}

void JoystickInputMapper::configure(nsecs_t when, const InputReaderConfiguration* config,
                                    uint32_t changes) {
    InputMapper::configure(when, config, changes);

    if (!changes) { // first time only
        // Collect all axes.
        for (int32_t abs = 0; abs <= ABS_MAX; abs++) {
            if (!(getAbsAxisUsage(abs, getDeviceContext().getDeviceClasses()) &
                  INPUT_DEVICE_CLASS_JOYSTICK)) {
                continue; // axis must be claimed by a different device
            }

            RawAbsoluteAxisInfo rawAxisInfo;
            getAbsoluteAxisInfo(abs, &rawAxisInfo);
            if (rawAxisInfo.valid) {
                // Map axis.
                AxisInfo axisInfo;
                bool explicitlyMapped = !getDeviceContext().mapAxis(abs, &axisInfo);
                if (!explicitlyMapped) {
                    // Axis is not explicitly mapped, will choose a generic axis later.
                    axisInfo.mode = AxisInfo::MODE_NORMAL;
                    axisInfo.axis = -1;
                }

                // Apply flat override.
                int32_t rawFlat =
                        axisInfo.flatOverride < 0 ? rawAxisInfo.flat : axisInfo.flatOverride;

                // Calculate scaling factors and limits.
                Axis axis;
                if (axisInfo.mode == AxisInfo::MODE_SPLIT) {
                    float scale = 1.0f / (axisInfo.splitValue - rawAxisInfo.minValue);
                    float highScale = 1.0f / (rawAxisInfo.maxValue - axisInfo.splitValue);
                    axis.initialize(rawAxisInfo, axisInfo, explicitlyMapped, scale, 0.0f, highScale,
                                    0.0f, 0.0f, 1.0f, rawFlat * scale, rawAxisInfo.fuzz * scale,
                                    rawAxisInfo.resolution * scale);
                } else if (isCenteredAxis(axisInfo.axis)) {
                    float scale = 2.0f / (rawAxisInfo.maxValue - rawAxisInfo.minValue);
                    float offset = avg(rawAxisInfo.minValue, rawAxisInfo.maxValue) * -scale;
                    axis.initialize(rawAxisInfo, axisInfo, explicitlyMapped, scale, offset, scale,
                                    offset, -1.0f, 1.0f, rawFlat * scale, rawAxisInfo.fuzz * scale,
                                    rawAxisInfo.resolution * scale);
                } else {
                    float scale = 1.0f / (rawAxisInfo.maxValue - rawAxisInfo.minValue);
                    axis.initialize(rawAxisInfo, axisInfo, explicitlyMapped, scale, 0.0f, scale,
                                    0.0f, 0.0f, 1.0f, rawFlat * scale, rawAxisInfo.fuzz * scale,
                                    rawAxisInfo.resolution * scale);
                }

                // To eliminate noise while the joystick is at rest, filter out small variations
                // in axis values up front.
                axis.filter = axis.fuzz ? axis.fuzz : axis.flat * 0.25f;

                mAxes.add(abs, axis);
            }
        }

        // If there are too many axes, start dropping them.
        // Prefer to keep explicitly mapped axes.
        if (mAxes.size() > PointerCoords::MAX_AXES) {
            ALOGI("Joystick '%s' has %zu axes but the framework only supports a maximum of %d.",
                  getDeviceName().c_str(), mAxes.size(), PointerCoords::MAX_AXES);
            pruneAxes(true);
            pruneAxes(false);
        }

        // Assign generic axis ids to remaining axes.
        int32_t nextGenericAxisId = AMOTION_EVENT_AXIS_GENERIC_1;
        size_t numAxes = mAxes.size();
        for (size_t i = 0; i < numAxes; i++) {
            Axis& axis = mAxes.editValueAt(i);
            if (axis.axisInfo.axis < 0) {
                while (nextGenericAxisId <= AMOTION_EVENT_AXIS_GENERIC_16 &&
                       haveAxis(nextGenericAxisId)) {
                    nextGenericAxisId += 1;
                }

                if (nextGenericAxisId <= AMOTION_EVENT_AXIS_GENERIC_16) {
                    axis.axisInfo.axis = nextGenericAxisId;
                    nextGenericAxisId += 1;
                } else {
                    ALOGI("Ignoring joystick '%s' axis %d because all of the generic axis ids "
                          "have already been assigned to other axes.",
                          getDeviceName().c_str(), mAxes.keyAt(i));
                    mAxes.removeItemsAt(i--);
                    numAxes -= 1;
                }
            }
        }
    }
}

bool JoystickInputMapper::haveAxis(int32_t axisId) {
    size_t numAxes = mAxes.size();
    for (size_t i = 0; i < numAxes; i++) {
        const Axis& axis = mAxes.valueAt(i);
        if (axis.axisInfo.axis == axisId ||
            (axis.axisInfo.mode == AxisInfo::MODE_SPLIT && axis.axisInfo.highAxis == axisId)) {
            return true;
        }
    }
    return false;
}

void JoystickInputMapper::pruneAxes(bool ignoreExplicitlyMappedAxes) {
    size_t i = mAxes.size();
    while (mAxes.size() > PointerCoords::MAX_AXES && i-- > 0) {
        if (ignoreExplicitlyMappedAxes && mAxes.valueAt(i).explicitlyMapped) {
            continue;
        }
        ALOGI("Discarding joystick '%s' axis %d because there are too many axes.",
              getDeviceName().c_str(), mAxes.keyAt(i));
        mAxes.removeItemsAt(i);
    }
}

bool JoystickInputMapper::isCenteredAxis(int32_t axis) {
    switch (axis) {
        case AMOTION_EVENT_AXIS_X:
        case AMOTION_EVENT_AXIS_Y:
        case AMOTION_EVENT_AXIS_Z:
        case AMOTION_EVENT_AXIS_RX:
        case AMOTION_EVENT_AXIS_RY:
        case AMOTION_EVENT_AXIS_RZ:
        case AMOTION_EVENT_AXIS_HAT_X:
        case AMOTION_EVENT_AXIS_HAT_Y:
        case AMOTION_EVENT_AXIS_ORIENTATION:
        case AMOTION_EVENT_AXIS_RUDDER:
        case AMOTION_EVENT_AXIS_WHEEL:
            return true;
        default:
            return false;
    }
}

void JoystickInputMapper::reset(nsecs_t when) {
    // Recenter all axes.
    size_t numAxes = mAxes.size();
    for (size_t i = 0; i < numAxes; i++) {
        Axis& axis = mAxes.editValueAt(i);
        axis.resetValue();
    }

    InputMapper::reset(when);
}

void JoystickInputMapper::process(const RawEvent* rawEvent) {
    switch (rawEvent->type) {
        case EV_ABS: {
            ssize_t index = mAxes.indexOfKey(rawEvent->code);
            if (index >= 0) {
                Axis& axis = mAxes.editValueAt(index);
                float newValue, highNewValue;
                switch (axis.axisInfo.mode) {
                    case AxisInfo::MODE_INVERT:
                        newValue = (axis.rawAxisInfo.maxValue - rawEvent->value) * axis.scale +
                                axis.offset;
                        highNewValue = 0.0f;
                        break;
                    case AxisInfo::MODE_SPLIT:
                        if (rawEvent->value < axis.axisInfo.splitValue) {
                            newValue = (axis.axisInfo.splitValue - rawEvent->value) * axis.scale +
                                    axis.offset;
                            highNewValue = 0.0f;
                        } else if (rawEvent->value > axis.axisInfo.splitValue) {
                            newValue = 0.0f;
                            highNewValue =
                                    (rawEvent->value - axis.axisInfo.splitValue) * axis.highScale +
                                    axis.highOffset;
                        } else {
                            newValue = 0.0f;
                            highNewValue = 0.0f;
                        }
                        break;
                    default:
                        newValue = rawEvent->value * axis.scale + axis.offset;
                        highNewValue = 0.0f;
                        break;
                }
                axis.newValue = newValue;
                axis.highNewValue = highNewValue;
            }
            break;
        }

        case EV_SYN:
            switch (rawEvent->code) {
                case SYN_REPORT:
                    sync(rawEvent->when, false /*force*/);
                    break;
            }
            break;
    }
}

void JoystickInputMapper::sync(nsecs_t when, bool force) {
    if (!filterAxes(force)) {
        return;
    }

    int32_t metaState = getContext()->getGlobalMetaState();
    int32_t buttonState = 0;

    PointerProperties pointerProperties;
    pointerProperties.clear();
    pointerProperties.id = 0;
    pointerProperties.toolType = AMOTION_EVENT_TOOL_TYPE_UNKNOWN;

    PointerCoords pointerCoords;
    pointerCoords.clear();

    size_t numAxes = mAxes.size();
    for (size_t i = 0; i < numAxes; i++) {
        const Axis& axis = mAxes.valueAt(i);
        setPointerCoordsAxisValue(&pointerCoords, axis.axisInfo.axis, axis.currentValue);
        if (axis.axisInfo.mode == AxisInfo::MODE_SPLIT) {
            setPointerCoordsAxisValue(&pointerCoords, axis.axisInfo.highAxis,
                                      axis.highCurrentValue);
        }
    }

    // Moving a joystick axis should not wake the device because joysticks can
    // be fairly noisy even when not in use.  On the other hand, pushing a gamepad
    // button will likely wake the device.
    // TODO: Use the input device configuration to control this behavior more finely.
    uint32_t policyFlags = 0;

    NotifyMotionArgs args(getContext()->getNextId(), when, getDeviceId(), AINPUT_SOURCE_JOYSTICK,
                          ADISPLAY_ID_NONE, policyFlags, AMOTION_EVENT_ACTION_MOVE, 0, 0, metaState,
                          buttonState, MotionClassification::NONE, AMOTION_EVENT_EDGE_FLAG_NONE, 1,
                          &pointerProperties, &pointerCoords, 0, 0,
                          AMOTION_EVENT_INVALID_CURSOR_POSITION,
                          AMOTION_EVENT_INVALID_CURSOR_POSITION, 0, /* videoFrames */ {});
    getListener()->notifyMotion(&args);
}

void JoystickInputMapper::setPointerCoordsAxisValue(PointerCoords* pointerCoords, int32_t axis,
                                                    float value) {
    pointerCoords->setAxisValue(axis, value);
    /* In order to ease the transition for developers from using the old axes
     * to the newer, more semantically correct axes, we'll continue to produce
     * values for the old axes as mirrors of the value of their corresponding
     * new axes. */
    int32_t compatAxis = getCompatAxis(axis);
    if (compatAxis >= 0) {
        pointerCoords->setAxisValue(compatAxis, value);
    }
}

bool JoystickInputMapper::filterAxes(bool force) {
    bool atLeastOneSignificantChange = force;
    size_t numAxes = mAxes.size();
    for (size_t i = 0; i < numAxes; i++) {
        Axis& axis = mAxes.editValueAt(i);
        if (force ||
            hasValueChangedSignificantly(axis.filter, axis.newValue, axis.currentValue, axis.min,
                                         axis.max)) {
            axis.currentValue = axis.newValue;
            atLeastOneSignificantChange = true;
        }
        if (axis.axisInfo.mode == AxisInfo::MODE_SPLIT) {
            if (force ||
                hasValueChangedSignificantly(axis.filter, axis.highNewValue, axis.highCurrentValue,
                                             axis.min, axis.max)) {
                axis.highCurrentValue = axis.highNewValue;
                atLeastOneSignificantChange = true;
            }
        }
    }
    return atLeastOneSignificantChange;
}

bool JoystickInputMapper::hasValueChangedSignificantly(float filter, float newValue,
                                                       float currentValue, float min, float max) {
    if (newValue != currentValue) {
        // Filter out small changes in value unless the value is converging on the axis
        // bounds or center point.  This is intended to reduce the amount of information
        // sent to applications by particularly noisy joysticks (such as PS3).
        if (fabs(newValue - currentValue) > filter ||
            hasMovedNearerToValueWithinFilteredRange(filter, newValue, currentValue, min) ||
            hasMovedNearerToValueWithinFilteredRange(filter, newValue, currentValue, max) ||
            hasMovedNearerToValueWithinFilteredRange(filter, newValue, currentValue, 0)) {
            return true;
        }
    }
    return false;
}

bool JoystickInputMapper::hasMovedNearerToValueWithinFilteredRange(float filter, float newValue,
                                                                   float currentValue,
                                                                   float thresholdValue) {
    float newDistance = fabs(newValue - thresholdValue);
    if (newDistance < filter) {
        float oldDistance = fabs(currentValue - thresholdValue);
        if (newDistance < oldDistance) {
            return true;
        }
    }
    return false;
}

} // namespace android
