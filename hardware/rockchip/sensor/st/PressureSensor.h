/*
 * Copyright (C) 2010 Motorola, Inc.
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ANDROID_PRESSURE_SENSOR_H
#define ANDROID_PRESSURE_SENSOR_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>


#include "nusensors.h"
#include "SensorBase.h"
#include "InputEventReader.h"

/*****************************************************************************/

struct input_event;

class PressureSensor : public SensorBase {
    int mEnabled;
    InputEventCircularReader mInputReader;
    sensors_event_t mPendingEvent; 
    bool mHasPendingEvent;

    
    int setInitialState();

public:
            PressureSensor();
    virtual ~PressureSensor();

    virtual int setDelay(int32_t handle, int64_t ns);
    virtual int enable(int32_t handle, int enabled);
    virtual int readEvents(sensors_event_t* data, int count);    
    virtual bool hasPendingEvents() const;
    virtual int isActivated(int handle);
    void processEvent(int code, int value);
};

/*****************************************************************************/

#define PRESSURE_IOCTL_MAGIC 'r'
#define PRESSURE_IOCTL_GET_ENABLED 		_IOR(PRESSURE_IOCTL_MAGIC, 1, int *)
#define PRESSURE_IOCTL_ENABLE 			_IOW(PRESSURE_IOCTL_MAGIC, 2, int *)
#define PRESSURE_IOCTL_DISABLE       		_IOW(PRESSURE_IOCTL_MAGIC, 3, int *)
#define PRESSURE_IOCTL_SET_DELAY       		_IOW(PRESSURE_IOCTL_MAGIC, 4, int *)


#endif  // ANDROID_PRESSURE_SENSOR_H
