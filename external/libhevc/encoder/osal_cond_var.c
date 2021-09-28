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
/*  File Name         : osal_cond_var.c                                      */
/*                                                                           */
/*  Description       : This file contains all the necessary function        */
/*                      definitions required to operate on Conditional       */
/*                      Variable.                                            */
/*                                                                           */
/*  List of Functions : osal_cond_var_create                                 */
/*                      osal_cond_var_destroy                                */
/*                      osal_cond_var_wait                                   */
/*                      osal_cond_var_wait_timed                             */
/*                      osal_cond_var_signal                                 */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         05 09 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/* User include files */
#include "cast_types.h"
#include "osal.h"
#include "osal_handle.h"
#include "osal_mutex.h"
#include "osal_cond_var.h"

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_cond_var_create                                     */
/*                                                                           */
/*  Description   : This function initializes the conditional variable and   */
/*                  returns the handle to it.                                */
/*                                                                           */
/*  Inputs        : OSAL handle                                              */
/*                  Memory manager handle                                    */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls system specific API and returns handle to the      */
/*                  conditional variable.                                    */
/*                                                                           */
/*  Outputs       : Handle to Condtional Variable                            */
/*                                                                           */
/*  Returns       : On SUCCESS - Handle to Conditional Varaible              */
/*                  On FAILURE - NULL                                        */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         05 09 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

void *osal_cond_var_create(IN void *osal_handle)
{
    if(0 == osal_handle)
        return 0;

    {
        osal_t *handle = osal_handle;
        cond_var_handle_t *cond_var_handle = 0;
        void *mmr_handle = 0;

        if(0 == handle || 0 == handle->alloc || 0 == handle->free)
            return 0;

        /* Initialize MMR handle */
        mmr_handle = handle->mmr_handle;

        /* Allocate memory for the Handle */
        cond_var_handle = handle->alloc(mmr_handle, sizeof(cond_var_handle_t));

        /* Error in memory allocation */
        if(0 == cond_var_handle)
            return 0;

        cond_var_handle->mmr_handle = mmr_handle;
        cond_var_handle->hdl = handle;

        /* Create semaphore */
        if(0 != pthread_cond_init(&(cond_var_handle->cond_var), 0))
        {
            handle->free(mmr_handle, cond_var_handle);
            return 0;
        }

        return cond_var_handle;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_cond_var_destroy                                    */
/*                                                                           */
/*  Description   : This function destroys all the OS resources allocated by */
/*                  'osal_cond_var_create' API.                              */
/*                                                                           */
/*  Inputs        : Conditional Variable handle                              */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Validates the input and destroys all the OS allocated    */
/*                  resources.                                               */
/*                                                                           */
/*  Outputs       : Status of closure                                        */
/*                                                                           */
/*  Returns       : On SUCCESS - OSAL_SUCCESS                                */
/*                  On FAILURE - OSAL_ERROR                                  */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         10 05 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_cond_var_destroy(IN void *cond_var_handle)
{
    if(0 == cond_var_handle)
        return OSAL_ERROR;

    {
        cond_var_handle_t *handle = (cond_var_handle_t *)cond_var_handle;
        WORD32 status = 0;

        if(0 == handle->hdl || 0 == handle->hdl->free)
            return OSAL_ERROR;

        /* Destroy the mutex */
        status = pthread_cond_destroy(&(handle->cond_var));

        if(0 != status)
            return OSAL_ERROR;

        /* Free the handle */
        handle->hdl->free(handle->mmr_handle, handle);
        return OSAL_SUCCESS;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_cond_var_wait                                       */
/*                                                                           */
/*  Description   : This function waits infinitely on conditional varaiable. */
/*                                                                           */
/*  Inputs        : Conditional Variable handle                              */
/*                  Mutex handle for lock                                    */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This function waits on Conditional variable signal. Till */
/*                  signal is not, lock on mutex is relinquished.            */
/*                                                                           */
/*  Outputs       : Status of wait on conditional variable                   */
/*                                                                           */
/*  Returns       : On SUCCESS - OSAL_SUCCESS                                */
/*                  On FAILURE - OSAL_ERROR                                  */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         10 05 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_cond_var_wait(IN void *cond_var_handle, IN void *mutex_handle)
{
    if(0 == cond_var_handle || 0 == mutex_handle)
        return OSAL_ERROR;

    {
        mutex_handle_t *mutex = (mutex_handle_t *)mutex_handle;
        cond_var_handle_t *cond_var = (cond_var_handle_t *)cond_var_handle;

        return pthread_cond_wait(&(cond_var->cond_var), &(mutex->mutex_handle));
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_cond_var_signal                                     */
/*                                                                           */
/*  Description   : This function signals on a conditional variable.         */
/*                                                                           */
/*  Inputs        : Conditional Variable handle                              */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Calls the underlaying API to signal on a conditional     */
/*                  variable.                                                */
/*                                                                           */
/*  Outputs       : Status of signalling                                     */
/*                                                                           */
/*  Returns       : On SUCCESS - OSAL_SUCCESS                                */
/*                  On FAILURE - OSAL_ERROR                                  */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         10 05 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_cond_var_signal(IN void *cond_var_handle)
{
    if(0 == cond_var_handle)
        return OSAL_ERROR;

    {
        cond_var_handle_t *cond_var = (cond_var_handle_t *)cond_var_handle;
        return pthread_cond_signal(&(cond_var->cond_var));
    }
}
