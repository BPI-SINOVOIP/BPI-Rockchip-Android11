/******************************************************************************
 *
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/

/*!
******************************************************************************
* \file ihevce_profile.c
*
* \brief
*    This file contains Profiling related functions
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
*
* List of Functions
*
*
******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <math.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "ihevce_profile.h"
#include "itt_video_api.h"
#include "ihevce_api.h"
#include <sys/time.h>

/* print attributes */

/* print everything on console */
#define PRINTF(x, y, ...) printf(__VA_ARGS__)

#if PROFILE_ENABLE

/*!
******************************************************************************
* \if Function name : init_profiler \endif
*
* \brief
*    Initialization of profiling context
*
*****************************************************************************
*/
void init_profiler(profile_database_t *ps_profile_data)
{
    memset(ps_profile_data, 0, sizeof(*ps_profile_data));

    return;
}

/*!
******************************************************************************
* \if Function name : profile_sample_time \endif
*
* \brief
*    This function calls the system function gettimeofday() to get the current
*    time
*
*****************************************************************************
*/
ULWORD64 profile_sample_time()
{
    struct timeval s_time;
    ULWORD64 u8_curr_time;

    gettimeofday(&s_time, NULL);
    u8_curr_time = (((ULWORD64)s_time.tv_sec * 1000 * 1000) + (ULWORD64)(s_time.tv_usec));

    return u8_curr_time;
}

/*!
******************************************************************************
* \if Function name : profile_start \endif
*
* \brief
*    This function samples current time
*
*****************************************************************************
*/
void profile_start(profile_database_t *ps_profile_data)
{
    ps_profile_data->u8_time_start = profile_sample_time();
    assert(0 == ps_profile_data->u1_sample_taken_flag);
    ps_profile_data->u1_sample_taken_flag = 1;

    return;
}

/*!
******************************************************************************
* \if Function name : profile_sample_time_end \endif
*
* \brief
*    This function is called for getting current time after a process call.
*    It also updates this info in profile database
*
*****************************************************************************
*/
void profile_sample_time_end(profile_database_t *ps_profile_data)
{
    ps_profile_data->u8_time_end = profile_sample_time();
    assert(1 == ps_profile_data->u1_sample_taken_flag);
    ps_profile_data->u1_sample_taken_flag = 0;

    return;
}

/*!
******************************************************************************
* \if Function name : profile_get_time_taken \endif
*
* \brief
*    This function computes the time taken by the process call
*
*****************************************************************************
*/
void profile_get_time_taken(profile_database_t *ps_profile_data)
{
    if(ps_profile_data->u8_time_end < ps_profile_data->u8_time_start)
    {
        /* Timer overflow */
        ps_profile_data->u8_cur_time =
            ((LWORD64)0xFFFFFFFF - ps_profile_data->u8_time_start) + ps_profile_data->u8_time_end;
    }
    else
    {
        ps_profile_data->u8_cur_time =
            ps_profile_data->u8_time_end - ps_profile_data->u8_time_start;
    }
}

/*!
******************************************************************************
* \if Function name : profile_get_average \endif
*
* \brief
*    This function computes the average time taken by the process calls so far
*
*****************************************************************************
*/
void profile_get_average(profile_database_t *ps_profile_data)
{
    ps_profile_data->u8_total_time += ps_profile_data->u8_cur_time;
    ps_profile_data->u4_num_profile_calls++;

    ps_profile_data->u8_avg_time =
        (ps_profile_data->u8_total_time / ps_profile_data->u4_num_profile_calls);

    return;
}

/*!
******************************************************************************
* \if Function name : profile_get_avg_time \endif
*
* \brief
*    This function returns the average time taken by the process calls so far
*
*****************************************************************************
*/
int profile_get_avg_time(profile_database_t *ps_profile_data)
{
    return (UWORD32)(ps_profile_data->u8_avg_time);
}

/*!
******************************************************************************
* \if Function name : profile_get_peak \endif
*
* \brief
*    This function computes the peak time taken by the process calls so far
*
*****************************************************************************
*/
void profile_get_peak(profile_database_t *ps_profile_data)
{
    if(ps_profile_data->u8_cur_time > ps_profile_data->u8_peak_time)
    {
        ps_profile_data->u8_peak_time = ps_profile_data->u8_cur_time;
    }
    return;
}

/*!
******************************************************************************
* \if Function name : profile_get_peak \endif
*
* \brief
*    This function returns the peak time taken by the process calls so far
*
*****************************************************************************
*/
int profile_get_peak_time(profile_database_t *ps_profile_data)
{
    return (UWORD32)(ps_profile_data->u8_peak_time);
}

/*!
******************************************************************************
* \if Function name : profile_end \endif
*
* \brief
*    This function prints the profile data - time taken by the last process
*    call, average time so far and peak time so far
*
*****************************************************************************
*/
void profile_end(profile_database_t *ps_profile_data, char *msg)
{
    printf("**********************************************\n");
    if(msg)
    {
        printf(
            "IHEVC : %s, Avg Process Time: %d micro-seconds\n",
            msg,
            (UWORD32)(ps_profile_data->u8_avg_time));
        printf(
            "IHEVC : %s, Peak Process Time : %d micro-seconds\n",
            msg,
            (UWORD32)(ps_profile_data->u8_peak_time));
    }
    else
    {
        printf(
            "IHEVC : %s, Avg Process Time: %d micro-seconds\n",
            "<unknown>",
            (UWORD32)(ps_profile_data->u8_avg_time));
        printf(
            "IHEVC : %s, Peak Process Time : %d micro-seconds\n",
            "<unknown>",
            (UWORD32)(ps_profile_data->u8_peak_time));
    }
    return;
}

/*!
******************************************************************************
* \if Function name : profile_stop \endif
*
* \brief
*    This function prints the profile time
*
*****************************************************************************
*/
void profile_stop(profile_database_t *ps_profile_data, char *msg)
{
    /* Get current time - This corresponds to time after the process call */
    profile_sample_time_end(ps_profile_data);
    /* Get time taken for the process call */
    profile_get_time_taken(ps_profile_data);
    /* Calculate average time taken so far */
    profile_get_average(ps_profile_data);
    /* Calculate peak time per process call taken so far */
    profile_get_peak(ps_profile_data);

    if(msg)
    {
        printf("%s, fps: :%10.3f", msg, (DOUBLE)(1000000.0 / ps_profile_data->u8_avg_time));
    }

    return;
}

#endif /* #if PROFILE_ENABLE */
