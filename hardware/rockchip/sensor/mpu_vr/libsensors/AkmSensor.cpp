/*
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>

#include "akm8975.h"

#include <cutils/log.h>

#include "AkmSensor.h"

#define D     LOGD
#define I     LOGI
/*****************************************************************************/

AkmSensor::AkmSensor()
: SensorBase(AKM_DEVICE_NAME, "compass"),
      mEnabled(0),
      mPendingMask(0),
      mInputReader(32)
{
    VFUNC_LOG;
    memset(mPendingEvents, 0, sizeof(mPendingEvents));

    mPendingEvents[MagneticField].version = sizeof(sensors_event_t);
    mPendingEvents[MagneticField].sensor = ID_M;
    mPendingEvents[MagneticField].type = SENSOR_TYPE_MAGNETIC_FIELD;
    mPendingEvents[MagneticField].magnetic.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[Orientation  ].version = sizeof(sensors_event_t);
    mPendingEvents[Orientation  ].sensor = ID_O;
    mPendingEvents[Orientation  ].type = SENSOR_TYPE_ORIENTATION;
    mPendingEvents[Orientation  ].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    for (int i=0 ; i<numSensors ; i++)
        mDelays[i] = 200000000; // 200 ms by default

    // read the actual value of all sensors if they're enabled already
    struct input_absinfo absinfo;
    short flags = 0;

    open_device();

    // mEnabled |= 1 << Accelerometer;
#if 0
    if (!ioctl(dev_fd, ECS_IOCTL_APP_GET_AFLAG, &flags)) {
        if (flags)  {
            mEnabled |= 1<<Accelerometer;
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ACCEL_X), &absinfo)) {
                mPendingEvents[Accelerometer].acceleration.x = absinfo.value * CONVERT_A_X;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ACCEL_Y), &absinfo)) {
                mPendingEvents[Accelerometer].acceleration.y = absinfo.value * CONVERT_A_Y;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ACCEL_Z), &absinfo)) {
                mPendingEvents[Accelerometer].acceleration.z = absinfo.value * CONVERT_A_Z;
            }
        }
    }
    if (!ioctl(dev_fd, ECS_IOCTL_APP_GET_MVFLAG, &flags)) {
        if (flags)  {
            mEnabled |= 1<<MagneticField;
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_MAGV_X), &absinfo)) {
                mPendingEvents[MagneticField].magnetic.x = absinfo.value * CONVERT_M_X;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_MAGV_Y), &absinfo)) {
                mPendingEvents[MagneticField].magnetic.y = absinfo.value * CONVERT_M_Y;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_MAGV_Z), &absinfo)) {
                mPendingEvents[MagneticField].magnetic.z = absinfo.value * CONVERT_M_Z;
            }
        }
    }
    if (!ioctl(dev_fd, ECS_IOCTL_APP_GET_MFLAG, &flags)) {
        if (flags)  {
            mEnabled |= 1<<Orientation;
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_YAW), &absinfo)) {
                mPendingEvents[Orientation].orientation.azimuth = absinfo.value;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_PITCH), &absinfo)) {
                mPendingEvents[Orientation].orientation.pitch = absinfo.value;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ROLL), &absinfo)) {
                mPendingEvents[Orientation].orientation.roll = -absinfo.value;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ORIENT_STATUS), &absinfo)) {
                mPendingEvents[Orientation].orientation.status = uint8_t(absinfo.value & SENSOR_STATE_MASK);
            }
        }
    }

    // disable temperature sensor, since it is not reported
    flags = 0;
    ioctl(dev_fd, ECS_IOCTL_APP_SET_TFLAG, &flags);
#endif

    if (!mEnabled) {
        close_device();
    }
}

AkmSensor::~AkmSensor() {
    VFUNC_LOG;
}

int AkmSensor::setEnable(int32_t handle, int en)
{
    VFUNC_LOG;
	//D("Entered : handle = 0x%x, en = 0x%x.", handle, en);
    int what = -1;
    switch (handle) {
        case ID_M: what = MagneticField; break;
        case ID_O: what = Orientation;   break;
    }

    if (uint32_t(what) >= numSensors)
        return -EINVAL;

    int newState  = en ? 1 : 0;
    int err = 0;

	I("newState = 0x%x, what = 0x%x, mEnabled = 0x%x.", newState, what, mEnabled);
    if ((uint32_t(newState)<<what) != (mEnabled & (1<<what))) {
        if (!mEnabled) {
            open_device();
        }
        int cmd;
        switch (what) {
            case MagneticField: cmd = ECS_IOCTL_APP_SET_MVFLAG; break;
            case Orientation:   cmd = ECS_IOCTL_APP_SET_MFLAG;  break;
        }
        short flags = newState;
        err = ioctl(dev_fd, cmd, &flags);
        err = err<0 ? -errno : 0;
        LOGE_IF(err, "ECS_IOCTL_APP_SET_XXX failed (%s)", strerror(-err));
        if (!err) {
            mEnabled &= ~(1<<what);
            mEnabled |= (uint32_t(flags)<<what);
            update_delay();
        }

		/*
        if (!mEnabled) {
            close_device();
        }
		*/
    }
EXIT:
	if ( !mEnabled ) {
    	close_device();
	}
    //D("to exit : mEnabled = 0x%x.", mEnabled);
    return err;
}

int AkmSensor::getEnable(int32_t handle)
{
    VFUNC_LOG;
    return !!(mEnabled&handle);
}

int AkmSensor::setDelay(int32_t handle, int64_t ns)
{
    VFUNC_LOG;
	//D("Entered : handle = 0x%x, ns = %lld.", handle, ns);
#ifdef ECS_IOCTL_APP_SET_DELAY
    int what = -1;
    switch (handle) {
        case ID_M: what = MagneticField; break;
        case ID_O: what = Orientation;   break;
    }

    if (uint32_t(what) >= numSensors)
        return -EINVAL;

    if (ns < 0)
        return -EINVAL;

    mDelays[what] = ns;
    return update_delay();
#else
    return -1;
#endif
}

int64_t AkmSensor::getDelay(int32_t handle)
{
    VFUNC_LOG;
    int what = -1;
    switch (handle) {
        case ID_M: what = MagneticField; break;
        case ID_O: what = Orientation;   break;
    }

    if (uint32_t(what) >= numSensors)
        return -EINVAL;

    return mDelays[what];
}

int AkmSensor::update_delay()
{
    VFUNC_LOG;

    int result = 0;

    if (mEnabled) {
        uint64_t wanted = -1LLU;
        for (int i=0 ; i<numSensors ; i++) {
            if (mEnabled & (1<<i)) {
                uint64_t ns = mDelays[i];
                wanted = wanted < ns ? wanted : ns;
            }
        }
        short delay = int64_t(wanted) / 1000000;
        if (ioctl(dev_fd, ECS_IOCTL_APP_SET_DELAY, &delay)) {
            return -errno;
        }
    }
    return result;
}

int AkmSensor::readEvents(sensors_event_t* data, int count)
{
    VFUNC_LOG;

    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;       /* 已经接收的 event 的数量, 待返回. */
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
//        D("count = 0x%x, type = 0x%x.", count, type);
        if (type == EV_ABS) {           // #define EV_ABS 0x03
            processEvent(event->code, event->value);
            mInputReader.next();
        } else if (type == EV_SYN) {    // #define EV_SYN 0x00
            int64_t time = getTimestamp(); //timevalToNano(event->time);
            for (int j=0 ; count && mPendingMask && j<numSensors ; j++) {
//                D("mPendingMask = 0x%x, j = %d; (mPendingMask & (1<<j)) = 0x%x", mPendingMask, j, (mPendingMask & (1<<j)) );
                if (mPendingMask & (1<<j)) {
                    mPendingMask &= ~(1<<j);
                    mPendingEvents[j].timestamp = time;
//                    D( "mEnabled = 0x%x, j = %d; mEnabled & (1<<j) = 0x%x.", mEnabled, j, (mEnabled & (1 << j) ) );
                    if (mEnabled & (1<<j)) {
                        *data++ = mPendingEvents[j];
                        count--;
                        numEventReceived++;
                    }
                }
            }
            if (!mPendingMask) {
                mInputReader.next();
            }
        } else {
            LOGE("AkmSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
            mInputReader.next();
        }
    }

    return numEventReceived;
}

void AkmSensor::processEvent(int code, int value)
{
    VFUNC_LOG;
//	D("Entered : code = 0x%x, value = 0x%x.", code, value);

    switch (code) {
        case ABS_HAT0X:
            mPendingMask |= 1<<MagneticField;
            mPendingEvents[MagneticField].magnetic.x = value;// * CONVERT_M_X; 0
            break;
        case ABS_HAT0Y:
            mPendingMask |= 1<<MagneticField;
            mPendingEvents[MagneticField].magnetic.y = value;// * CONVERT_M_Y; 0
            break;
        case ABS_BRAKE:
            mPendingMask |= 1<<MagneticField;
            mPendingEvents[MagneticField].magnetic.z = value;// * CONVERT_M_Z; 0
            break;
    }
}

int AkmSensor::readSample(long *data, int64_t *timestamp)
{
    VFUNC_LOG;
    int n=0;
    sensors_event_t sensor_data;

    n = readEvents(&sensor_data, 1);

    if (n>0) {
        data[0] = sensor_data.magnetic.x;
        data[1] = sensor_data.magnetic.y;
        data[2] = sensor_data.magnetic.z;
        *timestamp = sensor_data.timestamp;

//        D("AKM DATA: %d %d %d %lld\n", data[0], data[1], data[2], *timestamp);
    }

    return n;
}

int AkmSensor::readRawSample(float *data, int64_t *timestamp)
{
    VFUNC_LOG;

    data[0] = mPendingEvents[0].magnetic.x;
    data[1] = mPendingEvents[0].magnetic.y;
    data[2] = mPendingEvents[0].magnetic.z;
    *timestamp = mPendingEvents[0].timestamp;

    return 1;
}

