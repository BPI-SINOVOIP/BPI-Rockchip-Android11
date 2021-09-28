/*
 * Copyright (C) 2017 Intel Corporation.
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#include <linux/videodev2.h>
#include "NodeTypes.h"

namespace android {
namespace camera2 {

int getDefaultMemoryType(NodeTypes node)
{
    /*
     * According to V4L2 framework, the video device that exports dmabuf
     * must use V4L2_MEMORY_MMAP. V4L2_MEMORY_DMABUF is used for video
     * devices that import dmabuf.
     * ISYS_NODE_RAW works as a dmabuf exporter.
     * IMGU_NODE_INPUT imports the dmabuf fd exported from ISYS_NODE_RAW.
     * IMGU_NODE_PARAM and IMGU_NODE_STAT nodes map and use the pointers
     * to the buffers allocated from kernel.
     * IMGU_NODE_VF/PV_PREVIEW, IMGU_NODE_VIDEO and IMGU_NODE_STILL nodes import
     * dmabuf fd from stream buffer if using internal buffers is not necessary.
     */
    switch (node) {
    case ISYS_NODE_RAW:
    case IMGU_NODE_PARAM:
    case IMGU_NODE_STAT:
        return V4L2_MEMORY_MMAP;
    case IMGU_NODE_INPUT:
    case IMGU_NODE_OUTPUT:
    case IMGU_NODE_VF_PREVIEW:
    case IMGU_NODE_PV_PREVIEW:
    case IMGU_NODE_STILL:
    case IMGU_NODE_VIDEO:
        return V4L2_MEMORY_DMABUF;
    default:
        return V4L2_MEMORY_USERPTR;
    }
}

}
}
