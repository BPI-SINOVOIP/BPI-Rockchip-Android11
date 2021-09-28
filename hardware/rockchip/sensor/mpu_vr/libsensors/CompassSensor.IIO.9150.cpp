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

#define LOG_NDEBUG 0

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>
#include <linux/input.h>
#include <string.h>

#include "CompassSensor.IIO.9150.h"
#include "sensors.h"
#include "MPLSupport.h"
#include "sensor_params.h"
#include "ml_sysfs_helper.h"


#include "akm8975.h"
#define AKM_DEVICE_NAME     "/dev/compass"
// 720 LSG = 1G
#define LSG                         (720.0f)


// conversion of acceleration data to SI units (m/s^2)
#define CONVERT_A                   (GRAVITY_EARTH / LSG)
#define CONVERT_A_X                 (CONVERT_A)
#define CONVERT_A_Y                 (CONVERT_A)
#define CONVERT_A_Z                 (CONVERT_A)

// conversion of magnetic data (for AK8975) to uT units
#define CONVERT_M                   (1.0f*0.06f)
#define CONVERT_M_X                 (CONVERT_M)
#define CONVERT_M_Y                 (CONVERT_M)
#define CONVERT_M_Z                 (CONVERT_M)

// conversion of orientation data to degree units
#define CONVERT_O                   (1.0f/64.0f)
#define CONVERT_O_A                 (CONVERT_O)
#define CONVERT_O_P                 (CONVERT_O)
#define CONVERT_O_R                 (CONVERT_O)

// conversion of gyro data to SI units (radian/sec)
#define RANGE_GYRO                  (2000.0f*(float)M_PI/180.0f)
#define CONVERT_GYRO                ((70.0f / 1000.0f) * ((float)M_PI / 180.0f))
#define CONVERT_GYRO_X              (CONVERT_GYRO)
#define CONVERT_GYRO_Y              (CONVERT_GYRO)
#define CONVERT_GYRO_Z              (CONVERT_GYRO)

#define CONVERT_B                   (1.0f/100.0f)

#define SENSOR_STATE_MASK           (0x7FFF)

#define EVENT_TYPE_ACCEL_X          ABS_X
#define EVENT_TYPE_ACCEL_Y          ABS_Y
#define EVENT_TYPE_ACCEL_Z          ABS_Z
#define EVENT_TYPE_ACCEL_STATUS     ABS_WHEEL

#define EVENT_TYPE_YAW              ABS_RX
#define EVENT_TYPE_PITCH            ABS_RY
#define EVENT_TYPE_ROLL             ABS_RZ
#define EVENT_TYPE_ORIENT_STATUS    ABS_RUDDER

#define EVENT_TYPE_MAGV_X           ABS_HAT0X
#define EVENT_TYPE_MAGV_Y           ABS_HAT0Y
#define EVENT_TYPE_MAGV_Z           ABS_BRAKE
#define EVENT_TYPE_MAGV_STATUS      ABS_HAT1X


#define EVENT_TYPE_TEMPERATURE      ABS_THROTTLE
#define EVENT_TYPE_STEP_COUNT       ABS_GAS
#define EVENT_TYPE_PROXIMITY        ABS_DISTANCE
#define EVENT_TYPE_LIGHT            ABS_MISC

#define EVENT_TYPE_GYRO_X           REL_RX
#define EVENT_TYPE_GYRO_Y           REL_RY
#define EVENT_TYPE_GYRO_Z           REL_RZ

#define EVENT_TYPE_PRESSURE         ABS_PRESSURE



#define COMPASS_MAX_SYSFS_ATTRB sizeof(compassSysFs) / sizeof(char*)
#define COMPASS_NAME "USE_SYSFS"

#if defined COMPASS_YAS53x
#pragma message("HAL:build Invensense compass cal with YAS53x IIO on secondary bus")
#define USE_MPL_COMPASS_HAL (1)
#define COMPASS_NAME        "INV_YAS530"

#elif defined COMPASS_AK8975
#pragma message("HAL:build Invensense compass cal with AK8975 on primary bus")
#define USE_MPL_COMPASS_HAL (1)
#define COMPASS_NAME        "INV_AK8975"

#elif defined INVENSENSE_COMPASS_CAL
#   define COMPASS_NAME                 "USE_SYSFS"
#pragma message("HAL:build Invensense compass cal with compass IIO on secondary bus")
#define USE_MPL_COMPASS_HAL (1)
#else
#pragma message("HAL:build third party compass cal HAL")
#define USE_MPL_COMPASS_HAL (0)
// TODO: change to vendor's name
#define COMPASS_NAME        "AKM8975"

#endif

/*****************************************************************************/

CompassSensor::CompassSensor() 
                  : SensorBase(AKM_DEVICE_NAME, "compass"),
                    mEnable(0),
                    mPendingMask(0),
                    compass_fd(-1),
                    mCompassTimestamp(0),
                    mCompassInputReader(32)
{
    VFUNC_LOG;

    if(!strcmp(COMPASS_NAME, "USE_SYSFS"))
    {
        find_name_by_sensor_type("in_magn_x_raw", "iio:device", sensor_name);
//        dev_name = sensor_name;
    }
    LOGI("HAL:Secondary Chip Id: %s", dev_name);

    if(inv_init_sysfs_attributes()) {
        LOGE("Error Instantiating Compass\n");
        return;
    }

    /*if (!strcmp(COMPASS_NAME, "INV_COMPASS")) {
        mI2CBus = COMPASS_BUS_SECONDARY;
    } else {
        mI2CBus = COMPASS_BUS_PRIMARY;
    }*/

    memset(mCachedCompassData, 0, sizeof(mCachedCompassData));

    LOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:cat %s (%lld)", 
            compassSysFs.compass_orient, getTimestamp());
    FILE *fptr;
    fptr = fopen(compassSysFs.compass_orient, "r");
    if (fptr != NULL) {
        int om[9];
        if (fscanf(fptr, "%d,%d,%d,%d,%d,%d,%d,%d,%d", 
               &om[0], &om[1], &om[2], &om[3], &om[4], &om[5],
               &om[6], &om[7], &om[8]) < 0 || fclose(fptr) < 0) {
            LOGE("HAL:Could not read compass mounting matrix");
        } else {

            LOGV_IF(EXTRA_VERBOSE,
                    "HAL:compass mounting matrix: "
                    "%+d %+d %+d %+d %+d %+d %+d %+d %+d",
                    om[0], om[1], om[2], om[3], om[4], om[5], om[6], om[7], om[8]);

            mCompassOrientation[0] = om[0];
            mCompassOrientation[1] = om[1];
            mCompassOrientation[2] = om[2];
            mCompassOrientation[3] = om[3];
            mCompassOrientation[4] = om[4];
            mCompassOrientation[5] = om[5];
            mCompassOrientation[6] = om[6];
            mCompassOrientation[7] = om[7];
            mCompassOrientation[8] = om[8];
        }
    }

    if (!isIntegrated()) {
        enable(ID_M, 0);
    }

	// Open magnetic sensor's device driver.
	open_device();

    int flags;
    struct input_absinfo absinfo;

    memset(mPendingEvents, 0, sizeof(mPendingEvents));
    mPendingEvents[MagneticField].version = sizeof(sensors_event_t);
    mPendingEvents[MagneticField].sensor = ID_M;
    mPendingEvents[MagneticField].type = SENSOR_TYPE_MAGNETIC_FIELD;
    mPendingEvents[MagneticField].magnetic.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[Orientation  ].version = sizeof(sensors_event_t);
    mPendingEvents[Orientation  ].sensor = ID_O;
    mPendingEvents[Orientation  ].type = SENSOR_TYPE_ORIENTATION;
    mPendingEvents[Orientation  ].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    flags = 0;
    ioctl(dev_fd, ECS_IOCTL_APP_SET_MVFLAG, &flags);
    ioctl(dev_fd, ECS_IOCTL_APP_SET_MFLAG, &flags);

    if (!ioctl(dev_fd, ECS_IOCTL_APP_GET_MVFLAG, &flags)) {
        if (flags)  {
            mEnable |= 1<<MagneticField;
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
            mEnable |= 1<<Orientation;
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

    if (!mEnable) {
        close_device();
    }

    ALOGD("CompassSensor: mEnable=%d", mEnable);
}

CompassSensor::~CompassSensor()
{
    VFUNC_LOG;

    free(pathP);
    if( compass_fd > 0)
        close(compass_fd);
}

int CompassSensor::getFd() const
{
    VHANDLER_LOG;
    return data_fd;
}

/**
 *  @brief        This function will enable/disable sensor.
 *  @param[in]    handle
 *                  which sensor to enable/disable.
 *  @param[in]    en
 *                  en=1, enable; 
 *                  en=0, disable
 *  @return       if the operation is successful.
 */
int CompassSensor::enable(int32_t handle, int en) 
{
    VFUNC_LOG;

    int res = 0;

    res = write_sysfs_int(compassSysFs.compass_enable, en);

    if (en) {
        res += write_sysfs_int(compassSysFs.compass_x_fifo_enable, en);
        res += write_sysfs_int(compassSysFs.compass_y_fifo_enable, en);
        res += write_sysfs_int(compassSysFs.compass_z_fifo_enable, en);
    }

    int what = -1;
    switch (handle) {
        case ID_M: what = MagneticField; break;
        case ID_O: what = Orientation;   break;
    }

	ALOGD("Entered : handle = 0x%x, what=0x%x, en = 0x%x.", handle, what, en);
    ALOGD("ID_M=%d  ID_O=%d\n", ID_M, ID_O);

    if (what >= numSensors || what < 0)
        return -EINVAL;

    int newState  = en ? 1 : 0;
    int err = 0;

	ALOGI("newState = 0x%x, what = 0x%x, mEnable = 0x%x.", newState, what, mEnable);
    if ((uint32_t(newState)<<what) != (mEnable & (1<<what))) {
        if (!mEnable) {
            open_device();
        }
        int cmd;
        switch (what) {
            //case Accelerometer: cmd = ECS_IOCTL_APP_SET_AFLAG;  break;
            case MagneticField: cmd = ECS_IOCTL_APP_SET_MVFLAG; break;
            case Orientation:   cmd = ECS_IOCTL_APP_SET_MFLAG;  break;
        }
        short flags = newState;
        err = ioctl(dev_fd, cmd, &flags);
        err = err<0 ? -errno : 0;
        LOGE_IF(err, "ECS_IOCTL_APP_SET_XXX failed (%s)", strerror(-err));
        if (!err) {
            mEnable &= ~(1<<what);
            mEnable |= (uint32_t(flags)<<what);
            update_delay();
        }
		if ( !mEnable ) {
    		close_device();
		}
   	}
	ALOGD("to exit : mEnabled = 0x%x.", mEnable);
	return err;
}

int CompassSensor::update_delay()
{
	ALOGD("Entered update_delay.");
    int result = 0;

    if (mEnable) {
        uint64_t wanted = -1LLU;
        for (int i=0 ; i<numSensors ; i++) {
            if (mEnable & (1<<i)) {
                uint64_t ns = mDelay;
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

int CompassSensor::setDelay(int32_t handle, int64_t ns) 
{
    VFUNC_LOG;
    int tempFd;
    int res;

    LOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %.0f > %s (%lld)", 
            1000000000.f / ns, compassSysFs.compass_rate, getTimestamp());
    mDelay = ns;
    if (ns == 0)
        return -1;
    tempFd = open(compassSysFs.compass_rate, O_RDWR);
    res = write_attribute_sensor(tempFd, 1000000000.f / ns);
    if(res < 0) {
        LOGE("HAL:Compass update delay error");
    }
    return res;
}


/**
    @brief      This function will return the state of the sensor.
    @return     1=enabled; 0=disabled
**/
int CompassSensor::getEnable(int32_t handle)
{
    VFUNC_LOG;
    return mEnable;
}

/* use for Invensense compass calibration */
#define COMPASS_EVENT_DEBUG (1)
void CompassSensor::processCompassEvent(const input_event *event)
{
    VHANDLER_LOG;

    switch (event->code) {
    case EVENT_TYPE_ICOMPASS_X:
        LOGV_IF(COMPASS_EVENT_DEBUG, "EVENT_TYPE_ICOMPASS_X\n");
        mCachedCompassData[0] = event->value;
        break;
    case EVENT_TYPE_ICOMPASS_Y:
        LOGV_IF(COMPASS_EVENT_DEBUG, "EVENT_TYPE_ICOMPASS_Y\n");
        mCachedCompassData[1] = event->value;
        break;
    case EVENT_TYPE_ICOMPASS_Z:
        LOGV_IF(COMPASS_EVENT_DEBUG, "EVENT_TYPE_ICOMPASS_Z\n");
        mCachedCompassData[2] = event->value;
        break;
    }
    
    mCompassTimestamp = 
        (int64_t)event->time.tv_sec * 1000000000L + event->time.tv_usec * 1000L;
}

void CompassSensor::getOrientationMatrix(signed char *orient)
{
    VFUNC_LOG;
    memcpy(orient, mCompassOrientation, sizeof(mCompassOrientation));
}

long CompassSensor::getSensitivity()
{
    VFUNC_LOG;

    long sensitivity;
    LOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:cat %s (%lld)", 
            compassSysFs.compass_scale, getTimestamp());
    inv_read_data(compassSysFs.compass_scale, &sensitivity);
    return sensitivity;
}

/**
    @brief         This function is called by sensors_mpl.cpp
                   to read sensor data from the driver.
    @param[out]    data      sensor data is stored in this variable. Scaled such that
                             1 uT = 2^16
    @para[in]      timestamp data's timestamp
    @return        1, if 1   sample read, 0, if not, negative if error
 */
int CompassSensor::readSample(long *data, int64_t *timestamp)
{
    VHANDLER_LOG;

    int numEventReceived = 0, done = 0;

    mPendingMask = 0;

    ssize_t n = mCompassInputReader.fill(data_fd);
    if (n < 0) {
        LOGE("HAL:no compass events read");
        return n;
    }

    input_event const* event;

    while (done == 0 && mCompassInputReader.readEvent(&event)) {
        int type = event->type;
        ALOGD("type = 0x%x.", type);
#if 1
        if (type == EV_ABS) {           // #define EV_ABS 0x03
            processEvent(event->code, event->value);
            mCompassInputReader.next();
        } else if (type == EV_SYN) {    // #define EV_SYN 0x00
            int64_t time = timevalToNano(event->time);
            for (int j=0 ; mPendingMask && j<numSensors ; j++) {
                ALOGD("mPendingMask = 0x%x, j = %d; (mPendingMask & (1<<j)) = 0x%x", mPendingMask, j, (mPendingMask & (1<<j)) );
                if (mPendingMask & (1<<j)) {
                    mPendingMask &= ~(1<<j);
                    mPendingEvents[j].timestamp = time;
                    ALOGD( "mEnable = 0x%x, j = %d; mEnabled & (1<<j) = 0x%x.", mEnable, j, (mEnable & (1 << j) ) );
                    if (mEnable & (1<<j)) {
                        mCompassTimestamp = time;
                        //*timestamp = mCompassTimestamp;
                        //memcpy(data, mPendingEvents[j].magnetic.v, sizeof(mPendingEvents[j].magnetic.v));
                        done = 1;
                    }
                }
            }
            if (!mPendingMask) {
                mCompassInputReader.next();
            }
        } else {
            LOGE("AkmSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
            mCompassInputReader.next();
        }
#else
        if (type == EV_REL) {
            processCompassEvent(event);
        } else if (type == EV_SYN) {
            *timestamp = mCompassTimestamp;
            memcpy(data, mCachedCompassData, sizeof(mCachedCompassData));
            done = 1;
        } else {
            LOGE("HAL:Compass Sensor: unknown event (type=%d, code=%d)",
                 type, event->code);
        }
        mCompassInputReader.next();
#endif
    }

    return done;
}

/**
 *  @brief  This function will return the current delay for this sensor.
 *  @return delay in nanoseconds. 
 */
int64_t CompassSensor::getDelay(int32_t handle)
{
    VFUNC_LOG;
    return mDelay;
}

void CompassSensor::fillList(struct sensor_t *list)
{
    VFUNC_LOG;

    const char *compass = sensor_name;

    if (compass) {
        if(!strcmp(compass, "INV_COMPASS")) {
            list->maxRange = COMPASS_MPU9150_RANGE;
            list->resolution = COMPASS_MPU9150_RESOLUTION;
            list->power = COMPASS_MPU9150_POWER;
            list->minDelay = COMPASS_MPU9150_MINDELAY;
            return;
        }
        if(!strcmp(compass, "compass")
                || !strcmp(compass, "INV_AK8975")
                || !strncmp(compass, "AK89xx",2)) {
            list->maxRange = COMPASS_AKM8975_RANGE;
            list->resolution = COMPASS_AKM8975_RESOLUTION;
            list->power = COMPASS_AKM8975_POWER;
            list->minDelay = COMPASS_AKM8975_MINDELAY;
            return;
        }
        if(!strcmp(compass, "INV_YAS530")) {
            list->maxRange = COMPASS_YAS53x_RANGE;
            list->resolution = COMPASS_YAS53x_RESOLUTION;
            list->power = COMPASS_YAS53x_POWER;
            list->minDelay = COMPASS_YAS53x_MINDELAY;
            return;
        }
        if(!strcmp(compass, "INV_AMI306")) {
            list->maxRange = COMPASS_AMI306_RANGE;
            list->resolution = COMPASS_AMI306_RESOLUTION;
            list->power = COMPASS_AMI306_POWER;
            list->minDelay = COMPASS_AMI306_MINDELAY;
            return;
        }
    }
    LOGE("HAL:unknown compass id %s -- "
         "params default to ak8975 and might be wrong.",
         compass);
    list->maxRange = COMPASS_AKM8975_RANGE;
    list->resolution = COMPASS_AKM8975_RESOLUTION;
    list->power = COMPASS_AKM8975_POWER;
    list->minDelay = COMPASS_AKM8975_MINDELAY;
}

int CompassSensor::inv_init_sysfs_attributes(void)
{
    VFUNC_LOG;

    unsigned char i = 0;
    char sysfs_path[MAX_SYSFS_NAME_LEN], iio_trigger_path[MAX_SYSFS_NAME_LEN], tbuf[2];
    char *sptr;
    char **dptr;
    int num;

    pathP = (char*)malloc(
                    sizeof(char[COMPASS_MAX_SYSFS_ATTRB][MAX_SYSFS_NAME_LEN]));
    sptr = pathP;
    dptr = (char**)&compassSysFs;
    if (sptr == NULL)
        return -1;

    memset(sysfs_path, 0, sizeof(sysfs_path));
    memset(iio_trigger_path, 0, sizeof(iio_trigger_path));

    do {
        *dptr++ = sptr;
        memset(sptr, 0, sizeof(sptr));
        sptr += sizeof(char[MAX_SYSFS_NAME_LEN]);
    } while (++i < COMPASS_MAX_SYSFS_ATTRB);

    // get proper (in absolute/relative) IIO path & build MPU's sysfs paths
    // inv_get_sysfs_abs_path(sysfs_path);
    inv_get_sysfs_path(sysfs_path);
    inv_get_iio_trigger_path(iio_trigger_path);

    if (strcmp(sysfs_path, "") == 0  || strcmp(iio_trigger_path, "") == 0)
        return 0;

#if defined COMPASS_AK8975
    inv_get_input_number(dev_name, &num);
    tbuf[0] = num + 0x30;
    tbuf[1] = 0;
    sprintf(sysfs_path, "%s%s", "sys/class/input/input", tbuf);
    strcat(sysfs_path, "/ak8975");

    sprintf(compassSysFs.compass_enable, "%s%s", sysfs_path, "/enable");
    sprintf(compassSysFs.compass_rate, "%s%s", sysfs_path, "/rate");
    sprintf(compassSysFs.compass_scale, "%s%s", sysfs_path, "/scale");
    sprintf(compassSysFs.compass_orient, "%s%s", sysfs_path, "/compass_matrix");
#else
    sprintf(compassSysFs.compass_enable, "%s%s", sysfs_path, "/compass_enable");
    sprintf(compassSysFs.compass_x_fifo_enable, "%s%s", sysfs_path, "/scan_elements/in_magn_x_en");
    sprintf(compassSysFs.compass_y_fifo_enable, "%s%s", sysfs_path, "/scan_elements/in_magn_y_en");
    sprintf(compassSysFs.compass_z_fifo_enable, "%s%s", sysfs_path, "/scan_elements/in_magn_z_en");
    sprintf(compassSysFs.compass_rate, "%s%s", sysfs_path, "/sampling_frequency");
    sprintf(compassSysFs.compass_scale, "%s%s", sysfs_path, "/in_magn_scale");
    sprintf(compassSysFs.compass_orient, "%s%s", sysfs_path, "/compass_matrix");
#endif

#if 0
    // test print sysfs paths   
    dptr = (char**)&compassSysFs;
    for (i = 0; i < COMPASS_MAX_SYSFS_ATTRB; i++) {
        LOGE("HAL:sysfs path: %s", *dptr++);
    }
#endif
    return 0;
}

void CompassSensor::processEvent(int code, int value)
{
	ALOGD("%s : code = 0x%x, value = %d.", __func__, code, value);
    switch (code) {
        case EVENT_TYPE_MAGV_X:
            mPendingMask |= 1<<MagneticField;
            mPendingEvents[MagneticField].magnetic.x = value * CONVERT_M_X;
            mCachedCompassData[0] = value;
            break;
        case EVENT_TYPE_MAGV_Y:
            mPendingMask |= 1<<MagneticField;
            mPendingEvents[MagneticField].magnetic.y = value * CONVERT_M_Y;
            mCachedCompassData[1] = value;
            break;
        case EVENT_TYPE_MAGV_Z:
            mPendingMask |= 1<<MagneticField;
            mPendingEvents[MagneticField].magnetic.z = value * CONVERT_M_Z;
            mCachedCompassData[2] = value;
            break;
		case EVENT_TYPE_MAGV_STATUS:
            mPendingMask |= 1<<MagneticField;
            mPendingEvents[MagneticField].magnetic.status = value;
            break;
    }
}

