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
/*  File Name         : osal.c                                               */
/*                                                                           */
/*  Description       : This file contains all the API's of OSAL             */
/*                      initialization and closure                           */
/*                                                                           */
/*  List of Functions : osal_init                                            */
/*                      osal_register_callbacks                              */
/*                      osal_close                                           */
/*                      osal_get_version                                     */
/*                      osal_print_status_log                                */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         19 04 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>

#include <sys/types.h>
#include <semaphore.h>
#include <pthread.h>

/* User include files */
#include "cast_types.h"
#include "osal.h"
#include "osal_handle.h"

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

#define OSAL_VERSION "OSAL_v13.1"

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_init                                                */
/*                                                                           */
/*  Description   : This function creates and initializes the OSAL instance  */
/*                                                                           */
/*  Inputs        : Memory for OSAL handle                                   */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Initializes OSAL handle parameters to default values.    */
/*                                                                           */
/*  Outputs       : Status of OSAL handle initialization                     */
/*                                                                           */
/*  Returns       : On SUCCESS - OSAL_SUCCESS                                */
/*                  On FAILURE - OSAL_ERROR                                  */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         19 04 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_init(IN void *osal_handle)
{
    osal_t *handle = (osal_t *)osal_handle;

    /* Validate the input */
    if(0 == osal_handle)
        return OSAL_ERROR;

    /* Initialize call back functions */
    handle->alloc = 0;
    handle->free = 0;
    handle->mmr_handle = 0;

    return OSAL_SUCCESS;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_register_callbacks                                  */
/*                                                                           */
/*  Description   : This function registers MMR handle and allocation and    */
/*                  freeing call back functions.                             */
/*                                                                           */
/*  Inputs        : OSAL handle                                              */
/*                  OSAL callback attributes                                 */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This function initializes OSAL call back parameters.     */
/*                                                                           */
/*  Outputs       : Status of OSAL callback registration                     */
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

WORD32 osal_register_callbacks(IN void *osal_handle, IN osal_cb_funcs_t *cb_funcs)
{
    osal_t *handle = (osal_t *)osal_handle;

    /* Validate the input */
    if(0 == handle || 0 == cb_funcs)
        return OSAL_ERROR;

    if(0 == cb_funcs->osal_alloc || 0 == cb_funcs->osal_free)
        return OSAL_ERROR;

    /* Initialize call back parameters */
    handle->mmr_handle = cb_funcs->mmr_handle;
    handle->alloc = cb_funcs->osal_alloc;
    handle->free = cb_funcs->osal_free;

    return OSAL_SUCCESS;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_close                                               */
/*                                                                           */
/*  Description   : This function closes the OSAL instance                   */
/*                                                                           */
/*  Inputs        : OSAL handle                                              */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Frees the memory allocated for the OSAL handle           */
/*                                                                           */
/*  Outputs       : Status of OSAL instance close                            */
/*                                                                           */
/*  Returns       : On SUCCESS - 0                                           */
/*                  On FALIURE - -1                                          */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         19 04 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 osal_close(IN void *osal_handle)
{
    /* Validate input */
    if(0 == osal_handle)
        return OSAL_ERROR;

    return OSAL_SUCCESS;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : osal_get_version                                         */
/*                                                                           */
/*  Description   : This function gets the version of OSAL library.          */
/*                                                                           */
/*  Inputs        : None                                                     */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Returns a NULL terminated string with has the version of */
/*                  library being used.                                      */
/*                                                                           */
/*  Outputs       : Version of OSAL library.                                 */
/*                                                                           */
/*  Returns       : Pointer to a NULL terminated string                      */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         07 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD8 *osal_get_version()
{
    return ((WORD8 *)OSAL_VERSION);
}
