#ifndef ANDROID_GRALLOC2OMX_PRIV_H
#define ANDROID_GRALLOC2OMX_PRIV_H
/*
 *
 * Copyright 2015 Rockchip Electronics Co. LTD
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

/*
 * @file        gralloc_pirv_omx.h
 * @brief
 * @author      csy(csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2015.1.16 : Create
 */

#include <stdlib.h>
#include <cutils/log.h>

typedef struct gralloc_private_handle_t {
    int format;
    int share_fd;
    int type;
    int stride;
    int size;
}gralloc_private_handle_t;

#ifdef __cplusplus
extern "C" {
#endif
int32_t Rockchip_get_gralloc_private(uint32_t *handle,gralloc_private_handle_t *private_hnd);

#ifdef __cplusplus
}
#endif


#endif /* ANDROID_GRALLOC2OMX_PRIV_H */


