/*
 * Copyright (c) 2018, Fuzhou Rockchip Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _RKISP_CONTROL_AIQ_H_
#define _RKISP_CONTROL_AIQ_H_

#ifdef __cplusplus
extern "C" {
#endif


/*
 * set the Brightness.
 * Args:
 *    |cl_ctx|: current CL context
 *    |level|: Brightness level range: [0,255]
 * Returns:
 *    -EINVAL: failed
 *    0      : success
 */
int rkisp_cl_setBrightness(const void* cl_ctx, unsigned int level);
/*
 * get the Brightness.
 * Args:
 *    |cl_ctx|: current CL context
 *    |level|:  Brightness level range: [0,255]
 * Returns:
 *    -EINVAL: failed
 *    0      : success
 */
int rkisp_cl_getBrightness(const void* cl_ctx, unsigned int *level);
/*
 * set the Contrast.
 * Args:
 *    |cl_ctx|: current CL context
 *    |level|: Contrast level range: [0,255]
 * Returns:
 *    -EINVAL: failed
 *    0      : success
 */
int rkisp_cl_setContrast(const void* cl_ctx, unsigned int level);
/*
 * get the Contrast.
 * Args:
 *    |cl_ctx|: current CL context
 *    |level|: Contrast level range: [0,255]
 * Returns:
 *    -EINVAL: failed
 *    0      : success
 */
int rkisp_cl_getContrast(const void* cl_ctx, unsigned int *level);
/*
 * set the Saturation.
 * Args:
 *    |cl_ctx|: current CL context
 *    |level|: Saturation level range: [0,255]
 * Returns:
 *    -EINVAL: failed
 *    0      : success
 */
int rkisp_cl_setSaturation(const void* cl_ctx, unsigned int level);
/*
 * get the Saturation.
 * Args:
 *    |cl_ctx|: current CL context
 *    |level|: Saturation level range: [0,255]
 * Returns:
 *    -EINVAL: failed
 *    0      : success
 */
int rkisp_cl_getSaturation(const void* cl_ctx, unsigned int *level);

/*!
 * \brief set multiple cameras working concurrently
 * Notify this AIQ ctx will run with other sensor's AIQ ctx.

 * \param[in] cc        set cams concurrently used or not
 * \note should be called before rk_aiq_uapi_sysctl_start
 */
void setMulCamConc(const void* cl_ctx, bool cc);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // _RKISP_CONTROL_API_H_
