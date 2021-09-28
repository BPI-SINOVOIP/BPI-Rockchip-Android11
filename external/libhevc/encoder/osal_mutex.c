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
/*  File Name         : osal_mutex.c                                         */
/*                                                                           */
/*  Description       : This file contains all the necessary function        */
/*                      definitions required to operate on mutex             */
/*                                                                           */
/*  List of Functions : osal_get_mutex_handle_size                           */
/*                      osal_mutex_create                                    */
/*                      osal_mutex_destroy                                   */
/*                      osal_mutex_lock                                      */
/*                      osal_mutex_lock_timed                                */
/*                      osal_mutex_unlock                                    */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         20 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>

#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>

/* User include files */
#include "cast_types.h"
#include "osal.h"
#include "osal_handle.h"
#include "osal_mutex.h"

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_mutex_create                                        */
/*                                                                           */
/*  Description   : This function creates the mutex and returns the handle   */
/*                  to the user.                                             */
/*                                                                           */
/*  Inputs        : OSAL handle                                              */
/*                  Pointer to Memory manager handle                         */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Allocates memory for Mutex handle and calls OS specific  */
/*                  mutex create API call.                                   */
/*                                                                           */
/*  Outputs       : Mutex handle                                             */
/*                                                                           */
/*  Returns       : On SUCCESS - Mutex handle                                */
/*                  On FAILURE - NULL                                        */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         20 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

void *osal_mutex_create(IN void *osal_handle)
{
    void *mmr_handle = 0;

    /* Currenlty naming semaphores is not supported */
    {
        osal_t *handle = osal_handle;
        mutex_handle_t *mutex_handle = 0;

        if(0 == handle || 0 == handle->alloc || 0 == handle->free)
            return 0;

        /* Initialize MMR handle */
        mmr_handle = handle->mmr_handle;

        /* Allocate memory for the Handle */
        mutex_handle = handle->alloc(mmr_handle, sizeof(mutex_handle_t));

        /* Error in memory allocation */
        if(0 == mutex_handle)
            return 0;

        mutex_handle->mmr_handle = mmr_handle;
        mutex_handle->hdl = handle;

        /* Create semaphore */
        if(0 != pthread_mutex_init(&(mutex_handle->mutex_handle), NULL))
        {
            handle->free(mmr_handle, mutex_handle);
            return 0;
        }

        return mutex_handle;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_mutex_destroy                                       */
/*                                                                           */
/*  Description   : This function destroys the mutex.                        */
/*                                                                           */
/*  Inputs        : Mutex Handle                                             */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This function destroys the mutex refernced by the handle */
/*                  and frees the memory held by the handle.                 */
/*                                                                           */
/*  Outputs       : Status of mutex destroy                                  */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_mutex_destroy(IN void *mutex_handle)
{
    if(0 == mutex_handle)
        return OSAL_ERROR;

    {
        mutex_handle_t *handle = (mutex_handle_t *)mutex_handle;
        WORD32 status = 0;

        if(0 == handle->hdl || 0 == handle->hdl->free)
            return OSAL_ERROR;

        /* Destroy the mutex */
        status = pthread_mutex_destroy(&(handle->mutex_handle));

        if(0 != status)
            return OSAL_ERROR;

        /* Free the handle */
        handle->hdl->free(handle->mmr_handle, handle);
        return OSAL_SUCCESS;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_mutex_lock                                          */
/*                                                                           */
/*  Description   : This function locks the mutex.                           */
/*                                                                           */
/*  Inputs        : Mutex handle                                             */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls OS specific mutex lock API.                        */
/*                                                                           */
/*  Outputs       : Status of mutex lock                                     */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_mutex_lock(IN void *mutex_handle)
{
    if(0 == mutex_handle)
        return OSAL_ERROR;

    {
        mutex_handle_t *handle = (mutex_handle_t *)mutex_handle;

        /* Wait on mutex lock */
        return pthread_mutex_lock(&(handle->mutex_handle));
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_mutex_unlock                                        */
/*                                                                           */
/*  Description   : This function unlocks the mutex                          */
/*                                                                           */
/*  Inputs        : Mutex handle                                             */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls OS specific unlock mutex API.                      */
/*                                                                           */
/*  Outputs       : Status of mutex unlock                                   */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_mutex_unlock(IN void *mutex_handle)
{
    if(0 == mutex_handle)
        return OSAL_ERROR;

    {
        mutex_handle_t *handle = (mutex_handle_t *)mutex_handle;

        /* Release the lock */
        if(0 == pthread_mutex_unlock(&(handle->mutex_handle)))
            return OSAL_SUCCESS;

        return OSAL_ERROR;
    }
}
