/*
 $License:
    Copyright (C) 2011-2012 InvenSense Corporation, All Rights Reserved.
    See included License.txt for License information.
 $
 */
#include "mltypes.h"

#ifndef INV_HAL_OUTPUTS_H__
#define INV_HAL_OUTPUTS_H__

#ifdef __cplusplus
extern "C" {
#endif

    int inv_get_sensor_type_orientation(float *values, int8_t *accuracy,
                                         inv_time_t * timestamp);
    int inv_get_sensor_type_accelerometer(float *values, int8_t *accuracy,
                                           inv_time_t * timestamp);
    int inv_get_sensor_type_gyroscope(float *values, int8_t *accuracy,
                                       inv_time_t * timestamp);
    int inv_get_sensor_type_gyroscope_raw(float *values, int8_t *accuracy,
                                       inv_time_t * timestamp);
    int inv_get_sensor_type_magnetic_field(float *values, int8_t *accuracy,
                                            inv_time_t * timestamp);
	int inv_get_sensor_type_magnetic_field_raw(float *values, int8_t *accuracy,
                                            inv_time_t * timestamp);
    int inv_get_sensor_type_rotation_vector(float *values, int8_t *accuracy,
            inv_time_t * timestamp);

    int inv_get_sensor_type_linear_acceleration(float *values,
            int8_t *accuracy,
            inv_time_t * timestamp);
    int inv_get_sensor_type_gravity(float *values, int8_t *accuracy,
                                     inv_time_t * timestamp);

    int inv_get_sensor_type_orientation_6_axis(float *values, int8_t *accuracy,
                                         inv_time_t * timestamp);
    int inv_get_sensor_type_orientation_geomagnetic(float *values, int8_t *accuracy,
                                         inv_time_t * timestamp);
    int inv_get_sensor_type_rotation_vector_6_axis(float *values, int8_t *accuracy,
                                         inv_time_t * timestamp);
    int inv_get_sensor_type_geomagnetic_rotation_vector(float *values, int8_t *accuracy,
                                         inv_time_t * timestamp);

    inv_error_t inv_enable_hal_outputs(void);
    inv_error_t inv_disable_hal_outputs(void);
    inv_error_t inv_init_hal_outputs(void);
    inv_error_t inv_start_hal_outputs(void);
    inv_error_t inv_stop_hal_outputs(void);

    // Set data rates for virtual sensors
    void inv_set_linear_acceleration_sample_rate(long sample_rate_us);
    void inv_set_orientation_sample_rate(long sample_rate_us);
    void inv_set_rotation_vector_sample_rate(long sample_rate_us);
    void inv_set_gravity_sample_rate(long sample_rate_us);
    void inv_set_orientation_6_axis_sample_rate(long sample_rate_us);
    void inv_set_orientation_geomagnetic_sample_rate(long sample_rate_us);
    void inv_set_rotation_vector_6_axis_sample_rate(long sample_rate_us);
    void inv_set_geomagnetic_rotation_vector_sample_rate(long sample_rate_us);
    
#ifdef __cplusplus
}
#endif

#endif                          // INV_HAL_OUTPUTS_H__
