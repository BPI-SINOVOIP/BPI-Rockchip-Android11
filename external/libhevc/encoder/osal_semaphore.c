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
/*  File Name         : osal_semaphore.c                                     */
/*                                                                           */
/*  Description       : This file contains all the necessary function        */
/*                      definitions required to operate on semaphore         */
/*                                                                           */
/*  List of Functions : osal_sem_create                                      */
/*                      osal_sem_destroy                                     */
/*                      osal_sem_wait                                        */
/*                      osal_sem_wait_timed                                  */
/*                      osal_sem_post                                        */
/*                      osal_sem_count                                       */
/*                      query_semaphore                                      */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         07 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>

#include <semaphore.h>
#include <errno.h>

/* User include files */
#include "cast_types.h"
#include "osal.h"
#include "osal_handle.h"
#include "osal_semaphore.h"

/*****************************************************************************/
/* Static Function Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_sem_create                                          */
/*                                                                           */
/*  Description   : This function creates the semaphore and returns the      */
/*                  handle to the user.                                      */
/*                                                                           */
/*  Inputs        : Memory manager hamdle                                    */
/*                  Attributes to sempahore handle                           */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Allocates memory for handle and creates the semaphore    */
/*                  with specified initialized value by calling OS specific  */
/*                  API's.                                                   */
/*                                                                           */
/*  Outputs       : Semaphore handle                                         */
/*                                                                           */
/*  Returns       : On SUCCESS - Semaphore handle                            */
/*                  On FAILURE - NULL                                        */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         07 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

void *osal_sem_create(IN void *osal_handle, IN osal_sem_attr_t *attr)
{
    osal_t *handle = (osal_t *)osal_handle;
    void *mmr_handle = 0;

    if(0 == handle || 0 == handle->alloc || 0 == handle->free)
        return 0;

    /* Initialize MMR handle */
    mmr_handle = handle->mmr_handle;

    if(0 == attr)
        return 0;

    /* Currenlty naming semaphores is not supported */
    {
        /* Allocate memory for the sempahore handle */
        sem_handle_t *sem_handle = handle->alloc(mmr_handle, sizeof(sem_handle_t));

        if(0 == sem_handle)
            return 0;

        /* Initialize Semaphore handle parameters */
        sem_handle->mmr_handle = mmr_handle;
        sem_handle->hdl = handle;

        /* Create a sempahore */
        if(-1 == sem_init(
                     &(sem_handle->sem_handle), /* Semaphore handle     */
                     0, /* Shared only between threads */
                     attr->value)) /* Initialize value.           */
        {
            handle->free(sem_handle->mmr_handle, sem_handle);
            return 0;
        }

        return sem_handle;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_sem_destroy                                         */
/*                                                                           */
/*  Description   : This function closes the opened semaphore                */
/*                                                                           */
/*  Inputs        : Initialized Semaphore handle.                            */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls OS specific API's to close the semaphore.          */
/*                                                                           */
/*  Outputs       : Status of Semaphore close                                */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         07 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_sem_destroy(IN void *sem_handle)
{
    if(0 == sem_handle)
        return OSAL_ERROR;

    {
        sem_handle_t *handle = (sem_handle_t *)sem_handle;

        /* Validate OSAL handle */
        if(0 == handle->hdl || 0 == handle->hdl->free)
            return OSAL_ERROR;

        /* Destroy the semaphore */
        if(0 == sem_destroy(&(handle->sem_handle)))
        {
            handle->hdl->free(handle->mmr_handle, handle);
            return OSAL_SUCCESS;
        }

        return OSAL_ERROR;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_sem_wait                                            */
/*                                                                           */
/*  Description   : This function waits for semaphore to be unlocked and     */
/*                  then locks the semaphore and control returns back.       */
/*                                                                           */
/*  Inputs        : Initialized Semaphore handle                             */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This fucntion calls blocking semaphore lock API's which  */
/*                  block the caller till semaphore is locked by them or a   */
/*                  signal occurs which results in API function failure      */
/*                                                                           */
/*  Outputs       : Status of Semaphore wait                                 */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         07 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_sem_wait(IN void *sem_handle)
{
    if(0 == sem_handle)
        return OSAL_ERROR;

    {
        sem_handle_t *handle = (sem_handle_t *)sem_handle;

        /* Wait on Semaphore object infinitly */
        return sem_wait(&(handle->sem_handle));
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_sem_post                                            */
/*                                                                           */
/*  Description   : This function releases the lock on the semaphore         */
/*                                                                           */
/*  Inputs        : Initialized Semaphore handle                             */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls OS specific API's to release the lock on Semaphore */
/*                                                                           */
/*  Outputs       : Status of semaphore lock release                         */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FAILURE - -1                                          */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         07 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_sem_post(IN void *sem_handle)
{
    if(0 == sem_handle)
        return OSAL_ERROR;

    {
        sem_handle_t *handle = (sem_handle_t *)sem_handle;

        /* Semaphore Post */
        return sem_post(&(handle->sem_handle));
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_sem_count                                           */
/*                                                                           */
/*  Description   : This function returns the count of semaphore             */
/*                                                                           */
/*  Inputs        : Handle to Semaphore                                      */
/*                  Pointer to value holder                                  */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls OS specific API calls to query on semaphore        */
/*                                                                           */
/*  Outputs       : Status of Query                                          */
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

WORD32 osal_sem_count(IN void *sem_handle, OUT WORD32 *count)
{
    if(0 == sem_handle || 0 == count)
        return OSAL_ERROR;

    {
        sem_handle_t *handle = (sem_handle_t *)sem_handle;

        if(-1 == sem_getvalue(&(handle->sem_handle), count))
            return OSAL_ERROR;

        return OSAL_SUCCESS;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : query_semaphore                                          */
/*                                                                           */
/*  Description   : This function calls NtQuerySemaphore() API call of       */
/*                  ntdll.dll                                                */
/*                                                                           */
/*  Inputs        : Handle to Semaphore                                      */
/*                  Pointer to value holder                                  */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This function calls NtQuerySemaphore() API call of       */
/*                  ntdll.dll                                                */
/*                                                                           */
/*  Outputs       : Status of Query                                          */
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
