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

#include "KeyMouseInputMapper.h"
#include "TouchCursorInputMapperCommon.h"
#include <cutils/properties.h>

namespace android {

// --- Static Definitions ---

KeyMouseInputMapper::KeyMouseInputMapper(InputDeviceContext& deviceContext) : InputMapper(deviceContext) {
}

KeyMouseInputMapper::~KeyMouseInputMapper() {
}

uint32_t KeyMouseInputMapper::getSources() {
    return AINPUT_SOURCE_MOUSE;
}

void KeyMouseInputMapper::populateDeviceInfo(InputDeviceInfo* info) {
    InputMapper::populateDeviceInfo(info);
}

void KeyMouseInputMapper::dump(std::string& dump) {

}

void KeyMouseInputMapper::configure(nsecs_t when,
        const InputReaderConfiguration* config, uint32_t changes) {

    InputMapper::configure(when, config, changes);
    mSource=AINPUT_SOURCE_MOUSE;
    mXPrecision = 1.0f;
    mYPrecision = 1.0f;
	mPointerController = getContext()->getPointerController(getDeviceId());
}

void KeyMouseInputMapper::reset(nsecs_t when) {
    mButtonState = 0;
    mDownTime = 0;
    mCursorButtonAccumulator.reset(getDeviceContext());

    InputMapper::reset(when);
}

void KeyMouseInputMapper::process(const RawEvent* rawEvent) {

        mCursorButtonAccumulator.process(rawEvent);

        int mID;
        char mgetDeviceID[PROPERTY_VALUE_MAX] = "";
        property_get("sys.ID.mID", mgetDeviceID,0);
        mID=atoi(mgetDeviceID);
        if (rawEvent->type == EV_KEY && ((rawEvent->code== 28)||(rawEvent->code== 232))) {
                mdeltax = 0;
                mdeltay = 0;
                sync(rawEvent->when);
        }
}

void KeyMouseInputMapper::sync(nsecs_t when) {
    int32_t lastButtonState = mButtonState;
    int32_t currentButtonState = mCursorButtonAccumulator.getButtonState();
    mButtonState = currentButtonState;
    char mKeyLock[PROPERTY_VALUE_MAX] = "";
    memset(mKeyLock,0,5);
    property_get("sys.KeyMouse.mKeyMouseState", mKeyLock, "off");

    bool wasDown = isPointerDown(lastButtonState);
    bool down = isPointerDown(currentButtonState);
    bool downChanged;
    if (!wasDown && down) {
        mDownTime = when;
        downChanged = true;
    } else if (wasDown && !down) {
        downChanged = true;
    } else {
        downChanged = false;
    }
    nsecs_t downTime = mDownTime;
    if(strcmp(mKeyLock, "off") == 0) {
        return;
    }
    bool buttonsChanged = currentButtonState != lastButtonState;
    int32_t buttonsPressed = currentButtonState & ~lastButtonState;
    int32_t buttonsReleased = lastButtonState & ~currentButtonState;

    float deltaX = mdeltax;
    float deltaY = mdeltay;

    // Rotate delta according to orientation if needed.
    if (false) {
        rotateDelta(DISPLAY_ORIENTATION_0, &deltaX, &deltaY);
    }

    // Move the pointer.
    PointerProperties pointerProperties;
    pointerProperties.clear();
    pointerProperties.id = 0;
    pointerProperties.toolType = AMOTION_EVENT_TOOL_TYPE_MOUSE;

    PointerCoords pointerCoords;
    pointerCoords.clear();

    //paint the pointer of mouse here
    uint32_t policyFlags = 0;
    policyFlags |= POLICY_FLAG_WAKE;

    int32_t displayId = ADISPLAY_ID_DEFAULT;
    float xCursorPosition = AMOTION_EVENT_INVALID_CURSOR_POSITION;
    float yCursorPosition = AMOTION_EVENT_INVALID_CURSOR_POSITION;

    if (mPointerController != NULL) {
		mPointerController->setPresentation(PointerControllerInterface::PRESENTATION_POINTER);
        mPointerController->move(deltaX,deltaY);
        mPointerController->unfade(PointerControllerInterface::TRANSITION_IMMEDIATE);

        mPointerController->getPosition(&xCursorPosition, &yCursorPosition);
        pointerCoords.setAxisValue(AMOTION_EVENT_AXIS_X, xCursorPosition);
        pointerCoords.setAxisValue(AMOTION_EVENT_AXIS_Y, yCursorPosition);
        displayId = mPointerController->getDisplayId();//ADISPLAY_ID_DEFAULT;
    }
    // Moving an external trackball or mouse should wake the device.
    // We don't do this for internal cursor devices to prevent them from waking up
    // the device in your pocket.
    // TODO: Use the input device configuration to control this behavior more finely.

    // Synthesize key down from buttons if needed.
    synthesizeButtonKeys(getContext(), AKEY_EVENT_ACTION_DOWN, when, getDeviceId(), mSource,
                  displayId, policyFlags, lastButtonState, currentButtonState);

    // Send motion event.
    if (downChanged || buttonsChanged) {
        int32_t metaState = getContext()->getGlobalMetaState();
        int32_t buttonState = lastButtonState;
        int32_t motionEventAction;
        if (downChanged) {
            motionEventAction = down ? AMOTION_EVENT_ACTION_DOWN : AMOTION_EVENT_ACTION_UP;
        } else if (down || (mSource != AINPUT_SOURCE_MOUSE)) {
            motionEventAction = AMOTION_EVENT_ACTION_MOVE;
        } else {
            motionEventAction = AMOTION_EVENT_ACTION_HOVER_MOVE;
        }
        if (buttonsReleased) {
            BitSet32 released(buttonsReleased);
            while (!released.isEmpty()) {
                int32_t actionButton = BitSet32::valueForBit(released.clearFirstMarkedBit());
                buttonState &= ~actionButton;
                NotifyMotionArgs releaseArgs(getContext()->getNextId(), when, getDeviceId(),
                                             mSource, displayId, policyFlags,
                                             AMOTION_EVENT_ACTION_BUTTON_RELEASE, actionButton, 0,
                                             metaState, buttonState, MotionClassification::NONE,
                                             AMOTION_EVENT_EDGE_FLAG_NONE, 1, &pointerProperties,
                                             &pointerCoords, mXPrecision, mYPrecision,
                                             xCursorPosition, yCursorPosition, downTime,
                                             /* videoFrames */ {});
                getListener()->notifyMotion(&releaseArgs);
            }
        }

        NotifyMotionArgs args(getContext()->getNextId(), when, getDeviceId(), mSource, displayId,
                              policyFlags, motionEventAction, 0, 0, metaState, currentButtonState,
                              MotionClassification::NONE, AMOTION_EVENT_EDGE_FLAG_NONE, 1,
                              &pointerProperties, &pointerCoords, mXPrecision, mYPrecision,
                              xCursorPosition, yCursorPosition, downTime,
                              /* videoFrames */ {});
        getListener()->notifyMotion(&args);

        if (buttonsPressed) {
            BitSet32 pressed(buttonsPressed);
            while (!pressed.isEmpty()) {
                int32_t actionButton = BitSet32::valueForBit(pressed.clearFirstMarkedBit());
                buttonState |= actionButton;
                NotifyMotionArgs pressArgs(getContext()->getNextId(), when, getDeviceId(), mSource,
                                           displayId, policyFlags,
                                           AMOTION_EVENT_ACTION_BUTTON_PRESS, actionButton, 0,
                                           metaState, buttonState, MotionClassification::NONE,
                                           AMOTION_EVENT_EDGE_FLAG_NONE, 1, &pointerProperties,
                                           &pointerCoords, mXPrecision, mYPrecision,
                                           xCursorPosition, yCursorPosition, downTime,
                                           /* videoFrames */ {});
                getListener()->notifyMotion(&pressArgs);
            }
        }

        ALOG_ASSERT(buttonState == currentButtonState);

        // Send hover move after UP to tell the application that the mouse is hovering now.
        if (motionEventAction == AMOTION_EVENT_ACTION_UP
                && (mSource == AINPUT_SOURCE_MOUSE)) {
            NotifyMotionArgs hoverArgs(getContext()->getNextId(), when, getDeviceId(), mSource,
                                       displayId, policyFlags, AMOTION_EVENT_ACTION_HOVER_MOVE, 0,
                                       0, metaState, currentButtonState, MotionClassification::NONE,
                                       AMOTION_EVENT_EDGE_FLAG_NONE, 1, &pointerProperties,
                                       &pointerCoords, mXPrecision, mYPrecision, xCursorPosition,
                                       yCursorPosition, downTime, /* videoFrames */ {});
            getListener()->notifyMotion(&hoverArgs);
        }

    }
    // Synthesize key up from buttons if needed.
    synthesizeButtonKeys(getContext(), AKEY_EVENT_ACTION_UP, when, getDeviceId(), mSource,
            displayId, policyFlags, lastButtonState, currentButtonState);
}

int32_t KeyMouseInputMapper::getScanCodeState(uint32_t sourceMask, int32_t scanCode) {
    if (scanCode >= BTN_MOUSE && scanCode < BTN_JOYSTICK) {
        return getDeviceContext().getScanCodeState(scanCode);
    } else {
        return AKEY_STATE_UNKNOWN;
    }
}

void KeyMouseInputMapper::fadePointer() {

}

} // namespace android
