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

/**
******************************************************************************
* @file ihevce_profile.h
*
* @brief
*  This file contains profiling related definitions
*
* @author
*    Ittiam
******************************************************************************
*/

#ifndef _IHEVCE_PROFILE_H_
#define _IHEVCE_PROFILE_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define PROFILE_ENABLE 0

typedef struct
{
    /* Note that time below will be in units of micro seconds */
    /* Time before process call */
    ULWORD64 u8_time_start;

    /* Time after process call */
    ULWORD64 u8_time_end;

    /* Time taken by the last process call */
    ULWORD64 u8_cur_time;

    /* Sum total of the time taken by process calls so far */
    ULWORD64 u8_total_time;

    /*Avg time taken by a process so far */
    ULWORD64 u8_avg_time;

    /* Peak time taken by a process so far */
    ULWORD64 u8_peak_time;

    /* Number of process calls so far.
     * Required for calc of avg time taken per process call */
    UWORD32 u4_num_profile_calls;

    /* This flag is present to check that every
     * profile_start() will have a corresponding
     * arm_profile_sample_time_end() */
    UWORD8 u1_sample_taken_flag;

} profile_database_t;

typedef struct
{
    WORD32 tv_sec; /* Time in seconds.                            */
    WORD32 tv_usec; /* Time in micro seconds.                      */
} timeval_t;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
void profile_sample_time_start();
void profile_sample_time_end();
void profile_print_stats();
int profile_get_avg_time(profile_database_t *ps_profile_data);
int profile_get_peak_time(profile_database_t *ps_profile_data);
int profile_convert_to_milli_sec(profile_database_t *ps_profile_data);

ULWORD64 profile_sample_time();

/* Should be called after each process call */
void profile_stop(profile_database_t *ps_profile_data, char *msg);

/* Should be called before every process call */
void profile_start(profile_database_t *ps_profile_data);

/* Should be called after codec instance initialization */
void init_profiler(profile_database_t *ps_profile_data);

/* Should be called at the end of processing */
void profile_end(profile_database_t *ps_profile_data, char *msg);

#if PROFILE_ENABLE

#define PROFILE_INIT(x) init_profiler(x)
#define PROFILE_START(x) profile_start(x)
#define PROFILE_STOP(x, y) profile_stop(x, y)
#define PROFILE_END(x, y) profile_end(x, y)

#else /* #if PROFILE_ENABLE */

#define PROFILE_INIT(x)
#define PROFILE_START(x)
#define PROFILE_STOP(x, y)
#define PROFILE_END(x, y)

#endif /* #if PROFILE_ENABLE */

#endif /* _IHEVCE_PROFILE_H_ */
