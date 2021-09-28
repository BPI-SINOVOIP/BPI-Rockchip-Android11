/*
 $License:
  Copyright (C) 2011-2012 InvenSense Corporation, All Rights Reserved.
  See included License.txt for License information.
 $
 */

#ifndef MLDMP_MAGDISTURB_H__
#define MLDMP_MAGDISTURB_H__

#include "mltypes.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define WIN_8

int inv_check_magnetic_disturbance(unsigned long delta_time, const long *quat,
                                   const long *compass, const long *gravity);

void inv_track_dip_angle(int mode, float currdip);

inv_error_t inv_enable_magnetic_disturbance(void);
inv_error_t inv_disable_magnetic_disturbance(void);
int inv_get_magnetic_disturbance_state();
inv_error_t inv_set_magnetic_disturbance(int time_ms);
inv_error_t inv_disable_dip_tracking(void);
inv_error_t inv_enable_dip_tracking(void);
inv_error_t inv_init_magnetic_disturbance(void);

/* verbose logging */
void inv_enable_magnetic_disturbance_logging(void);
void inv_disable_magnetic_disturbance_logging(void);

float Mag3ofNormalizedLong(const long *x);

    int inv_mag_disturb_get_detect_status(void);
    void inv_mag_disturb_set_detect_status(int status);

    int inv_mag_disturb_get_drop_heading_accuracy_status(void);
    void inv_mag_disturb_set_drop_heading_accuracy_status(int status);
    int inv_mag_disturb_get_detect_weak_status(void);
    void inv_mag_disturb_set_detect_weak_status(int status);

    float inv_mag_disturb_get_vector_radius(void);
    void inv_mag_disturb_set_vector_radius(float radius);

    /************************/
    /* external API         */
    /************************/
    float inv_mag_disturb_get_magnitude_threshold(void);
    void inv_mag_disturb_set_magnitude_threshold(float radius);

    float inv_mag_disturb_get_time_threshold_detect(void);
    void inv_mag_disturb_set_time_threshold_detect(float time_seconds);


#ifdef __cplusplus
}
#endif

#endif // MLDMP_MAGDISTURB_H__
