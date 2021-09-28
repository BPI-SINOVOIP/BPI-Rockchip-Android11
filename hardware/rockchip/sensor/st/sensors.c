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

#include <hardware/sensors.h>

#include "nusensors.h"

/*****************************************************************************/

/*
 * The SENSORS Module
 */

/*
 * the AK8973 has a 8-bit ADC but the firmware seems to average 16 samples,
 * or at least makes its calibration on 12-bits values. This increases the
 * resolution by 4 bits.
 */

static const struct sensor_t sSensorList[] = {
        #ifdef GRAVITY_SENSOR_SUPPORT
        { .name       = "Accelerometer sensor",
          .vendor     = "The Android Open Source Project",
          .version    = 1,
          .handle     = SENSORS_HANDLE_BASE+ID_A,
          .type       = SENSOR_TYPE_ACCELEROMETER,
          .maxRange   = 4.0f*9.80f,
          .resolution = (4.0f*9.80f)/4096.0f,
          .power      = 0.2f,
          .minDelay   = 7000,
          .fifoReservedEventCount = 0,
          .fifoMaxEventCount = 0,
          .stringType = SENSOR_STRING_TYPE_ACCELEROMETER,
          .requiredPermission = 0,
          .maxDelay = 200000,
          .flags = SENSOR_FLAG_CONTINUOUS_MODE,
          .reserved   = {}
        },
        #endif
        #ifdef COMPASS_SENSOR_SUPPORT
        { .name       = "Compass Magnetic field sensor",
          .vendor     = "The Android Open Source Project",
          .version    = 1,
          .handle     = SENSORS_HANDLE_BASE+ID_M,
          .type       = SENSOR_TYPE_MAGNETIC_FIELD,
          .maxRange   = 2000.0f,
          .resolution = 1.0f/16.0f,
          .power      = 6.8f,
          .minDelay   = 7000,
          .fifoReservedEventCount = 0,
          .fifoMaxEventCount = 0,
          .stringType = SENSOR_STRING_TYPE_MAGNETIC_FIELD,
          .requiredPermission = 0,
          .maxDelay = 200000,
          .flags = SENSOR_FLAG_CONTINUOUS_MODE,
          .reserved   = {}
        },
        #endif
        #ifdef GYROSCOPE_SENSOR_SUPPORT
        { .name       = "Gyroscope sensor",
          .vendor     = "The Android Open Source Project",
          .version    = 1,
          .handle     = SENSORS_HANDLE_BASE+ID_GY,
          .type       = SENSOR_TYPE_GYROSCOPE,
          .maxRange   = RANGE_GYRO,
          .resolution = CONVERT_GYRO,
          .power      = 6.1f,
          .minDelay   = 7000,
          .fifoReservedEventCount = 0,
          .fifoMaxEventCount = 0,
          .stringType = SENSOR_STRING_TYPE_GYROSCOPE,
          .requiredPermission = 0,
          .maxDelay = 200000,
          .flags = SENSOR_FLAG_CONTINUOUS_MODE,
          .reserved   = {}
        },
        #endif
        #ifdef PROXIMITY_SENSOR_SUPPORT
        { .name       = "Proximity sensor",
          .vendor     = "The Android Open Source Project",
          .version    = 1,
          .handle     = SENSORS_HANDLE_BASE+ID_P,
          .type       = SENSOR_TYPE_PROXIMITY,
          .maxRange   = PROXIMITY_THRESHOLD_CM,
          .resolution = PROXIMITY_THRESHOLD_CM,
          .power      = 0.5f,
          .minDelay   = 7000,
          .fifoReservedEventCount = 0,
          .fifoMaxEventCount = 0,
          .stringType = SENSOR_STRING_TYPE_PROXIMITY,
          .requiredPermission = 0,
          .maxDelay = 200000,
          .flags = SENSOR_FLAG_ON_CHANGE_MODE | SENSOR_FLAG_WAKE_UP,
          .reserved   = {}
        },
        #endif
        #ifdef LIGHT_SENSOR_SUPPORT
        { .name       = "Light sensor",
          .vendor     = "The Android Open Source Project",
          .version    = 1,
          .handle     = SENSORS_HANDLE_BASE+ID_L,
          .type       = SENSOR_TYPE_LIGHT,
          .maxRange   = 10240.0f,
          .resolution = 1.0f,
          .power      = 0.5f,
          .minDelay   = 7000,
          .fifoReservedEventCount = 0,
          .fifoMaxEventCount = 0,
          .stringType = SENSOR_STRING_TYPE_LIGHT,
          .requiredPermission = 0,
          .maxDelay = 200000,
          .flags = SENSOR_FLAG_ON_CHANGE_MODE,
          .reserved   = {}
        },
        #endif
        #ifdef PRESSURE_SENSOR_SUPPORT
        { .name       = "Pressure sensor",
          .vendor     = "The Android Open Source Project",
          .version    = 1,
          .handle     = SENSORS_HANDLE_BASE+ID_PR,
          .type       = SENSOR_TYPE_PRESSURE,
          .maxRange   = 110000.0f,
          .resolution = 1.0f,
          .power      = 1.0f,
          .minDelay   = 7000,
          .fifoReservedEventCount = 0,
          .fifoMaxEventCount = 0,
          .stringType = SENSOR_STRING_TYPE_PRESSURE,
          .requiredPermission = 0,
          .maxDelay = 200000,
          .flags = SENSOR_FLAG_CONTINUOUS_MODE,
          .reserved   = {}
        },
        #endif
        #ifdef TEMPERATURE_SENSOR_SUPPORT
        { .name       = "Temperature sensor",
          .vendor     = "The Android Open Source Project",
          .version    = 1,
          .handle     = SENSORS_HANDLE_BASE+ID_TMP,
          .type       = SENSOR_TYPE_AMBIENT_TEMPERATURE,
          .maxRange   = 110000.0f,
          .resolution = 1.0f,
          .power      = 1.0f,
          .minDelay   = 7000,
          .fifoReservedEventCount = 0,
          .fifoMaxEventCount = 0,
          .stringType = SENSOR_STRING_TYPE_TEMPERATURE,
          .requiredPermission = 0,
          .maxDelay = 200000,
          .flags = SENSOR_FLAG_ON_CHANGE_MODE,
          .reserved   = {}
        },
        #endif
};

static int open_sensors(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static int sensors__get_sensors_list(struct sensors_module_t* module,
        struct sensor_t const** list)
{
    *list = sSensorList;
    return ARRAY_SIZE(sSensorList);
}

static struct hw_module_methods_t sensors_module_methods = {
    .open = open_sensors
};

 struct sensors_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = SENSORS_HARDWARE_MODULE_ID,
        .name = "Rockchip Sensors Module",
        .author = "The RKdroid Project",
        .methods = &sensors_module_methods,
    },
    .get_sensors_list = sensors__get_sensors_list
};

/*****************************************************************************/

static int open_sensors(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    return init_nusensors(module, device);
}
