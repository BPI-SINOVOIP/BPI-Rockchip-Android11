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
/*  File Name         : osal_thread.c                                        */
/*                                                                           */
/*  Description       : This file contains Thread API's implemented for      */
/*                      different platforms.                                 */
/*                                                                           */
/*  List of Functions : osal_thread_create                                   */
/*                      osal_thread_destroy                                  */
/*                      osal_func                                            */
/*                      osal_set_thread_priority                             */
/*                      osal_set_thread_core_affinity                        */
/*                      osal_thread_sleep                                    */
/*                      osal_thread_yield                                    */
/*                      osal_thread_suspend                                  */
/*                      osal_thread_resume                                   */
/*                      osal_thread_wait                                     */
/*                      osal_get_thread_handle                               */
/*                      osal_get_time                                        */
/*                      osal_get_time_usec                                   */
/*                      osal_get_last_error                                  */
/*                      osal_print_last_error                                */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>

#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <unistd.h>
#include <math.h>
#include <sched.h> /*for CPU_SET, etc.. */
#include <linux/unistd.h>
#include <sys/syscall.h>

/* User include files */
#include "cast_types.h"
#include "osal.h"
#include "osal_handle.h"
#include "osal_thread.h"
#include "osal_errno.h"

/*****************************************************************************/
/* Static Function Declarations                                              */
/*****************************************************************************/

static void osal_func(void *param);

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_thread_create                                       */
/*                                                                           */
/*  Description   : This function create a new thread.                       */
/*                                                                           */
/*  Inputs        : OSAL handle                                              */
/*                  Memory Manager Handle                                    */
/*                  Thread creation attributes                               */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This function calls OS specific thread create API's and  */
/*                  creates a new thread with specified attributes.          */
/*                                                                           */
/*  Outputs       : Status of thread creation                                */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : Only supports creating threads with default attributes   */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

void *osal_thread_create(IN void *osal_handle, IN osal_thread_attr_t *attr)
{
    osal_t *handle = (osal_t *)osal_handle;
    WORD32 priority = 0;
    void *mmr_handle = 0;

    /* If Handle or attributes are not valid, return ERRORED. */
    if(0 == attr)
        return 0;

    if(0 == handle || 0 == handle->alloc || 0 == handle->free)
        return 0;

    /* Initialize MMR handle */
    mmr_handle = handle->mmr_handle;

    {
        pthread_attr_t tattr;
        thread_handle_t *hdl = 0;

        attr->sched_policy = OSAL_SCHED_RR;

        /* Allocate memory for thread handle */
        hdl = handle->alloc(mmr_handle, sizeof(thread_handle_t));
        if(0 == hdl)
            return 0;

        /* Initialize thread handle parameters */
        hdl->mmr_handle = mmr_handle;
        hdl->hdl = handle;
        hdl->exit_code = attr->exit_code;
        hdl->priority = priority;
        hdl->thread_func = attr->thread_func;
        hdl->thread_param = attr->thread_param;

        /* initialized with default attributes */
        if(0 != pthread_attr_init(&tattr))
        {
            handle->free(hdl->mmr_handle, hdl);
            return 0;
        }

        /* Create the thread */
        hdl->thread_id = pthread_create(
            &(hdl->thread_handle), /* Thread Handle   */
            &tattr, /* Attributes      */
            (void *(*)(void *))osal_func,
            hdl); /* Parameters      */

        /* In case of error in thread creationn, Free the handle memory and  */
        /* return error.                                                     */
        if(0 != hdl->thread_id)
        {
            handle->free(hdl->mmr_handle, hdl);
            return 0;
        }

        pthread_attr_destroy(&tattr);

        return hdl;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_thread_destroy                                      */
/*                                                                           */
/*  Description   : This function calls OS specific API's to close a thread  */
/*                  which is represented by specified handle.                */
/*                                                                           */
/*  Inputs        : Initialized thread handle                                */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Closing other threads is supported only in windows. So,  */
/*                  only windows platform supports this API.                 */
/*                                                                           */
/*  Outputs       : Status of thread close                                   */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_thread_destroy(IN void *thread_handle)
{
    /* If thread handle is not valid, return error */
    if(0 == thread_handle)
        return OSAL_ERROR;

    {
        thread_handle_t *hdl = (thread_handle_t *)thread_handle;

        /* Free memory allocated for Thread handle */
        ((osal_t *)hdl->hdl)->free(hdl->mmr_handle, hdl);

        return OSAL_SUCCESS;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_func                                                */
/*                                                                           */
/*  Description   : This function calls the registered threads calling       */
/*                  function                                                 */
/*                                                                           */
/*  Inputs        : Thread Handle                                            */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls each registered thread function                    */
/*                                                                           */
/*  Outputs       : None                                                     */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         10 05 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

void osal_func(IN void *param)
{
    thread_handle_t *hdl = (thread_handle_t *)param;

    while(1)
    {
        /* Untill thread returns exit code, invoke the thread function */
        if(hdl->exit_code == hdl->thread_func(hdl->thread_param))
            break;
    }

    /* On Linux platforms call pthread_exit() to release all the resources   */
    /* allocated.                                                            */
    pthread_exit(NULL);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_thread_sleep                                        */
/*                                                                           */
/*  Description   : This function calls OS specific API and makes thread     */
/*                  sleep for specified number of milli seconds.             */
/*                                                                           */
/*  Inputs        : Initialized thread handle                                */
/*                  Time to sleep in millisceonds                            */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls API to sleep for specified number of milli seconds */
/*                                                                           */
/*  Outputs       : Status of sleep                                          */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_thread_sleep(IN UWORD32 milli_seconds)
{
    {
        struct timespec timer;

        /* Convert time in milliseconds into seconds and nano seconds */
        timer.tv_sec = milli_seconds / 1000;
        milli_seconds -= (timer.tv_sec * 1000);
        timer.tv_nsec = milli_seconds * MEGA_CONST;

        /* Using Monotonic clock to sleep, also flag is set to 0 for relative */
        /* time to current clock time                                         */
        if(0 == clock_nanosleep(CLOCK_MONOTONIC, 0, &timer, NULL))
        {
            return OSAL_SUCCESS;
        }

        return OSAL_ERROR;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_thread_yield                                        */
/*                                                                           */
/*  Description   : This function causes the yield its execution.            */
/*                                                                           */
/*  Inputs        : Thread Handle                                            */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls OS specific yield calls.                           */
/*                                                                           */
/*  Outputs       : Status of Thread Yield                                   */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : Yield in WIN32 (whihc is a 16 - bit API) is still present*/
/*                  only to maintian backward compatibility. Can get         */
/*                  deprecated in future.                                    */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_thread_yield()
{
    if(0 == sched_yield())
        return OSAL_SUCCESS;

    return OSAL_ERROR;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_thread_suspend                                      */
/*                                                                           */
/*  Description   : This function causes the suspension its execution.       */
/*                                                                           */
/*  Inputs        : Thread Handle                                            */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls OS specific suspend calls.                         */
/*                                                                           */
/*  Outputs       : Status of Thread Suspend                                 */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : API not supported in Redhat Linux. Refer Redhat          */
/*                  documentation in:                                        */
/*                  http://www.redhat.com/docs/wp/solaris_port/c1347.html    */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         30 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_thread_suspend(IN void *thread_handle)
{
    /* If thread handle is not valid, return error */
    if(0 == thread_handle)
        return OSAL_ERROR;

    {
        /* Thread suspend are not supported in Redhat Linux. Refer link      */
        /* http://www.redhat.com/docs/wp/solaris_port/c1347.html             */

        return OSAL_NOT_SUPPORTED;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_thread_resume                                       */
/*                                                                           */
/*  Description   : This function causes the resumption its execution.       */
/*                                                                           */
/*  Inputs        : Thread Handle                                            */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls OS specific resume calls.                          */
/*                                                                           */
/*  Outputs       : Status of Thread Suspend                                 */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : API not supported in Redhat Linux. Refer Redhat          */
/*                  documentation in:                                        */
/*                  http://www.redhat.com/docs/wp/solaris_port/c1347.html    */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         30 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_thread_resume(IN void *thread_handle)
{
    /* If thread handle is not valid, return error */
    if(0 == thread_handle)
        return OSAL_ERROR;

    {
        /* Thread suspend are not supported in Redhat Linux. Refer link      */
        /* http://www.redhat.com/docs/wp/solaris_port/c1347.html             */

        return OSAL_NOT_SUPPORTED;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_thread_wait                                         */
/*                                                                           */
/*  Description   : This function causes the wait untill called thread       */
/*                  finishes execution                                       */
/*                                                                           */
/*  Inputs        : Thread Handle                                            */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls OS specific wait call for wait on another thread   */
/*                                                                           */
/*  Outputs       : Status of Thread wait                                    */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         30 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_thread_wait(IN void *thread_handle)
{
    if(0 == thread_handle)
        return OSAL_ERROR;

    {
        WORD32 result = 0;
        void *status = 0;

        thread_handle_t *hdl = (thread_handle_t *)thread_handle;

        /* Join the thread to wait for thread to complete execution */
        result = pthread_join(hdl->thread_handle, (void **)&status);

        return result;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_get_thread_handle                                   */
/*                                                                           */
/*  Description   : This function gets current thread handle. Currently not  */
/*                  supported                                                */
/*                                                                           */
/*  Inputs        : OSAL handle.                                             */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Gets all the thread properities and constructs a new     */
/*                  thread handle .                                          */
/*                                                                           */
/*  Outputs       : Thread handle to current thread.                         */
/*                                                                           */
/*  Returns       : On SUCCESS - Current thread handle                       */
/*                  On FAILURE - NULL                                        */
/*                                                                           */
/*  Issues        : Not supported on Linux and BIOS platforms.               */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         10 05 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

void *osal_get_thread_handle(IN void *osal_handle)
{
    osal_t *handle = (osal_t *)osal_handle;

    if(0 == osal_handle)
        return 0;

    {
        thread_handle_t *hdl = handle->alloc(handle->mmr_handle, sizeof(thread_handle_t));
        WORD32 schedpolicy;
        struct sched_param schedparam;

        if(0 == hdl)
            return 0;

        hdl->mmr_handle = handle->mmr_handle;
        hdl->hdl = handle;
        hdl->exit_code = 0;
        hdl->thread_func = 0;
        hdl->thread_param = 0;
        hdl->thread_handle = pthread_self();
        hdl->thread_id = 0;
        hdl->priority = schedparam.sched_priority;

        /* Get thread priority from scheduling parameters */
        if(0 != pthread_getschedparam(hdl->thread_handle, &schedpolicy, &schedparam))
        {
            return 0;
        }

        return hdl;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_get_time                                            */
/*                                                                           */
/*  Description   : This function returns absolute time in milli seconds     */
/*                                                                           */
/*  Inputs        : None                                                     */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Gets the absolute time by calling OS specific API's.     */
/*                                                                           */
/*  Outputs       : Absolute time in milli seconds.                          */
/*                                                                           */
/*  Returns       : +ve 32 bit value                                         */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

UWORD32 osal_get_time()
{
    {
        struct timespec time_val;
        int cur_time;

        /* Get the Monotonic time */
        clock_gettime(CLOCK_MONOTONIC, &time_val);

        /* Convert time in seconds and micro seconds into milliseconds time */
        cur_time = time_val.tv_sec * 1000 + time_val.tv_nsec / 1000000;
        return cur_time;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_get_time_usec                                       */
/*                                                                           */
/*  Description   : This function returns absolute time in micro seconds     */
/*                                                                           */
/*  Inputs        : None                                                     */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Gets the absolute time by calling OS specific API's.     */
/*                                                                           */
/*  Outputs       : Absolute time in micro seconds.                          */
/*                                                                           */
/*  Returns       : +ve 32 bit value                                         */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 03 2009   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_get_time_usec(UWORD32 *sec, UWORD32 *usec)
{
    if((0 == sec) || (0 == usec))
        return OSAL_ERROR;

    {
        struct timespec time_val;

        /* Get the Monotonic time */
        clock_gettime(CLOCK_MONOTONIC, &time_val);

        /* Convert time in seconds and micro seconds into milliseconds time */
        *sec = time_val.tv_sec;
        *usec = time_val.tv_nsec / 1000;

        return OSAL_SUCCESS;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_get_last_error                                      */
/*                                                                           */
/*  Description   : This function gets the last error code.                  */
/*                                                                           */
/*  Inputs        : None                                                     */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Gets the last occured error code by calling OS specific  */
/*                  API call.                                                */
/*                                                                           */
/*  Outputs       : Error Number                                             */
/*                                                                           */
/*  Returns       : If no error - 0                                          */
/*                  Else        - +ve number                                 */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

UWORD32 osal_get_last_error()
{
    UWORD32 get_linux_error(void);
    return get_linux_error();
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_print_last_error                                    */
/*                                                                           */
/*  Description   : This function prints the last error message.             */
/*                                                                           */
/*  Inputs        : None                                                     */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Gets the last occured error code by calling OS specific  */
/*                  API call. It prints argument string (if not NULL),       */
/*                  followed by ': ' then the error_string and <new_line>.   */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         10 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

void osal_print_last_error(IN const STRWORD8 *string)
{
    perror(string);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_get_current_tid                                     */
/*                                                                           */
/*  Description   : Gets the tid of the thread in whose context this call    */
/*                  was made                                                 */
/*                                                                           */
/*  Inputs        : None                                                     */
/*  Globals       : None                                                     */
/*  Processing    : None                                                     */
/*  Outputs       : None                                                     */
/*  Returns       : Thread ID, as a WORD32                                   */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         07 05 2015   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_get_current_tid(void)
{
    return syscall(__NR_gettid);
}
