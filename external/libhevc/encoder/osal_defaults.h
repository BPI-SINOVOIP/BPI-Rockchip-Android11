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

/*****************************************************************************/
/*                                                                           */
/*  File Name         : osal_defaults.h                                      */
/*                                                                           */
/*  Description       : This file contains default values to initialize the  */
/*                      attributes required components created through OSAL  */
/*                                                                           */
/*  List of Functions : None                                                 */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         14 07 2007   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

#ifndef OSAL_DEFAULTS_H
#define OSAL_DEFAULTS_H

/*****************************************************************************/
/* Constants                                                                 */
/*****************************************************************************/

/* Default attributes for a mailbox */
#define OSAL_DEFAULT_MBOX_ATTR                                                                     \
    {                                                                                              \
        0, /* Thread handle */                                                                     \
            0, /* Mbox name     */                                                                 \
            0, /* Mbox length   */                                                                 \
            0 /* Msg size      */                                                                  \
    }

/* Default attributes for a semaphore */
#define OSAL_DEFAULT_SEM_ATTR                                                                      \
    {                                                                                              \
        0 /* Initial value */                                                                      \
    }

/* Default attributes for a thread */
#define OSAL_DEFAULT_THREAD_ATTR                                                                   \
    {                                                                                              \
        0, /* Thread function     */                                                               \
            0, /* Thread parameters   */                                                           \
            0, /* Stack size          */                                                           \
            0, /* Stack start address */                                                           \
            0, /* Thread name         */                                                           \
            1, /* Use OSAL priorities */                                                           \
            OSAL_PRIORITY_DEFAULT, /* Thread priority     */                                       \
            0, /* Exit code           */                                                           \
            OSAL_SCHED_OTHER, /* Scheduling policy   */                                            \
            0, /* Core affinity mask  */                                                           \
            0 /* group num           */                                                            \
    }

/* Default attributes for a socket */
#define OSAL_DEFAULT_SOCKET_ATTR                                                                   \
    {                                                                                              \
        OSAL_UDP /* Protocol */                                                                    \
    }

/* Default attributes for a socket address entry */
#define OSAL_DEFAULT_SOCKADDR                                                                      \
    {                                                                                              \
        0                                                                                          \
    } /* Initialize IP and port to 0 */

/* Default attributes for the select engine */
#define OSAL_DEFAULT_SELECT_ENGINE_ATTR                                                            \
    {                                                                                              \
        1, /* Use OSAL priorities */                                                               \
            OSAL_PRIORITY_DEFAULT, /* Thread priority     */                                       \
            0, /* Thread name         */                                                           \
            5000, /* Timeout for select call*/                                                     \
            10000 /* Poll interavel      */                                                        \
    }

/* Default attributes for an entry in the select engine */
#define OSAL_DEFAULT_SELECT_ENTRY                                                                  \
    {                                                                                              \
        0, /* Socket Handle                   */                                                   \
            OSAL_READ_FD, /* Socket type                     */                                    \
            0, /* Init callback                   */                                               \
            0, /* Init callback parameters        */                                               \
            0, /* Socket activity callback        */                                               \
            0, /* Socket activity callback params */                                               \
            0, /* Terminate-time callback         */                                               \
            0, /* Terminate-time callback params  */                                               \
            0, /* Succesful Exit code             */                                               \
            0 /* ID                              */                                                \
    }

/* Default attributes for FD set */
#define OSAL_DEFAULT_FD_SET                                                                        \
    {                                                                                              \
        0 /* Initializes count to 0 */                                                             \
    }

/* Default attributes for time value structure */
#define OSAL_DEFAULT_TIMEVAL                                                                       \
    {                                                                                              \
        0, /* Seconds      */                                                                      \
            0 /* Microseconds */                                                                   \
    }

/* Default attributes for LINGER socket option structure */
#define OSAL_DEFAULT_SOCKOPT_LINGER                                                                \
    {                                                                                              \
        0, /* On/Off */                                                                            \
            0 /* Linger */                                                                         \
    }

/* Default attributes for Multicast interface IP */
#define OSAL_DEFAULT_IP_MREQ                                                                       \
    {                                                                                              \
        0                                                                                          \
    } /* Initialize all IPs to 0 */

#endif /* OSAL_DEFAULTS_H */
