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

#ifndef _UI_INPUTREADER_KEYMOUSE_INPUT_MAPPER_H
#define _UI_INPUTREADER_KEYMOUSE_INPUT_MAPPER_H

#include "InputMapper.h"
#include "CursorButtonAccumulator.h"

namespace android {

class KeyMouseInputMapper : public InputMapper {

public:
    KeyMouseInputMapper(InputDeviceContext& deviceContext);
    virtual ~KeyMouseInputMapper();

    virtual uint32_t getSources();
    virtual void populateDeviceInfo(InputDeviceInfo* deviceInfo);
    virtual void dump(std::string& dump);
    virtual void configure(nsecs_t when, const InputReaderConfiguration* config, uint32_t changes);
    virtual void reset(nsecs_t when);
    virtual void process(const RawEvent* rawEvent);

    virtual int32_t getScanCodeState(uint32_t sourceMask, int32_t scanCode);

    virtual void fadePointer();


private:
    CursorButtonAccumulator mCursorButtonAccumulator;
    sp<PointerControllerInterface> mPointerController;

    int32_t mSource;
    float mdeltax, mdeltay;
    int32_t mButtonState;
    nsecs_t mDownTime;
    float mXPrecision;
    float mYPrecision;

    void sync(nsecs_t when);
};
} // namespace android

#endif // _UI_INPUTREADER_KEYMOUSE_INPUT_MAPPER_H
