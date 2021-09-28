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
 * \file ihevce_sys_api.c
 *
 * \brief
 *    This file contains wrapper utilities to use hevc encoder library
 *
 * \date
 *    15/04/2014
 *
 * \author
 *    Ittiam
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
#include "ihevc_macros.h"

#include "itt_video_api.h"
#include "ihevce_api.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
 ******************************************************************************
 * \if Function name : ihevce_printf \endif
 *
 * \brief
 *    This function implements printf
 *
 *****************************************************************************
 */
WORD32 ihevce_printf(void *pv_handle, const char *format, ...)
{
    UNUSED(pv_handle);
    UNUSED(format);
    return 0;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_fopen \endif
 *
 * \brief
 *    This function implements fopen
 *
 *****************************************************************************
 */
FILE *ihevce_fopen(void *pv_handle, const char *filename, const char *mode)
{
    UNUSED(pv_handle);
    UNUSED(filename);
    UNUSED(mode);
    return NULL;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_fclose \endif
 *
 * \brief
 *    This function implements fclose
 *
 *****************************************************************************
 */
int ihevce_fclose(void *pv_handle, FILE *file_ptr)
{
    UNUSED(pv_handle);
    UNUSED(file_ptr);
    return -1;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_fflush \endif
 *
 * \brief
 *    This function implements fflush
 *
 *****************************************************************************
 */
int ihevce_fflush(void *pv_handle, FILE *file_ptr)
{
    UNUSED(pv_handle);
    UNUSED(file_ptr);
    return -1;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_fseek \endif
 *
 * \brief
 *    This function implements fseek
 *
 *****************************************************************************
 */
int ihevce_fseek(void *pv_handle, FILE *file_ptr, long offset, int origin)
{
    UNUSED(pv_handle);
    UNUSED(file_ptr);
    UNUSED(offset);
    UNUSED(origin);
    return -1;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_fscanf \endif
 *
 * \brief
 *    This function implements fscanf
 *
 *****************************************************************************
 */
int ihevce_fscanf(
    void *pv_handle, IHEVCE_DATA_TYPE e_data_type, FILE *file_ptr, const char *format, void *pv_dst)
{
    UNUSED(pv_handle);
    UNUSED(e_data_type);
    UNUSED(file_ptr);
    UNUSED(format);
    UNUSED(pv_dst);
    return 0;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_fgets \endif
 *
 * \brief
 *    This function implements fgets
 *
 *****************************************************************************
 */
char *ihevce_fgets(void *pv_handle, char *pi1_str, int i4_size, FILE *pf_stream)
{
    UNUSED(pv_handle);
    UNUSED(pi1_str);
    UNUSED(i4_size);
    UNUSED(pf_stream);
    return NULL;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_fread \endif
 *
 * \brief
 *    This function implements fread
 *
 *****************************************************************************
 */
size_t
    ihevce_fread(void *pv_handle, void *pv_dst, size_t element_size, size_t count, FILE *file_ptr)
{
    UNUSED(pv_handle);
    UNUSED(pv_dst);
    UNUSED(element_size);
    UNUSED(count);
    UNUSED(file_ptr);
    return 0;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_sscanf \endif
 *
 * \brief
 *    This function implements sscanf
 *
 *****************************************************************************
 */
int ihevce_sscanf(void *pv_handle, const char *pv_src, const char *format, int *p_dst_int)
{
    UNUSED(pv_handle);
    UNUSED(pv_src);
    UNUSED(format);
    UNUSED(p_dst_int);
    return 0;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_fprintf \endif
 *
 * \brief
 *    This function implements fprintf
 *
 *****************************************************************************
 */
int ihevce_fprintf(void *pv_handle, FILE *file_ptr, const char *format, ...)
{
    UNUSED(pv_handle);
    UNUSED(file_ptr);
    UNUSED(format);
    return 0;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_fwrite \endif
 *
 * \brief
 *    This function implements fwrite
 *
 *****************************************************************************
 */
size_t ihevce_fwrite(
    void *pv_handle, const void *pv_src, size_t element_size, size_t count, FILE *file_ptr)
{
    UNUSED(pv_handle);
    UNUSED(pv_src);
    UNUSED(element_size);
    UNUSED(count);
    UNUSED(file_ptr);
    return 0;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_sprintf \endif
 *
 * \brief
 *    This function implements sprintf
 *
 *****************************************************************************
 */
int ihevce_sprintf(void *pv_handle, char *dst, const char *format, ...)
{
    UNUSED(pv_handle);
    UNUSED(dst);
    UNUSED(format);
    return 0;
}

/*!
 ******************************************************************************
 * \if Function name : ihevce_init_sys_api \endif
 *
 * \brief
 *    This function initialises sysstem call apis
 *
 * \param[in]
 *   pv_main_ctxt    : This is used only for storing.
 *   ps_sys_api      : This is address to sys_api structure of static_cfg_prms
 *
 * \return
 *    None
 *
 * \author
 *  Ittiam
 *
 *****************************************************************************
 */
void ihevce_init_sys_api(void *pv_cb_handle, ihevce_sys_api_t *ps_sys_api)
{
    ps_sys_api->pv_cb_handle = pv_cb_handle;

    /* Console IO APIs */
    ps_sys_api->ihevce_printf = ihevce_printf;

    ps_sys_api->ihevce_sscanf = ihevce_sscanf;
    ps_sys_api->ihevce_sprintf = ihevce_sprintf;

    /* File IO APIs */
    ps_sys_api->s_file_io_api.ihevce_fopen = ihevce_fopen;
    ps_sys_api->s_file_io_api.ihevce_fclose = ihevce_fclose;
    ps_sys_api->s_file_io_api.ihevce_fflush = ihevce_fflush;
    ps_sys_api->s_file_io_api.ihevce_fseek = ihevce_fseek;

    ps_sys_api->s_file_io_api.ihevce_fscanf = ihevce_fscanf;
    ps_sys_api->s_file_io_api.ihevce_fread = ihevce_fread;

    ps_sys_api->s_file_io_api.ihevce_fprintf = ihevce_fprintf;
    ps_sys_api->s_file_io_api.ihevce_fwrite = ihevce_fwrite;
    ps_sys_api->s_file_io_api.ihevce_fgets = ihevce_fgets;
}
