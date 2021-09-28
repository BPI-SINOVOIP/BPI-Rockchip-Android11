/*
 * Copyright (C) 2016-2017 Intel Corporation
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
#ifndef _CAMERA3_HAL_ISYS_TYPES_H_
#define _CAMERA3_HAL_ISYS_TYPES_H_

NAMESPACE_DECLARATION {
/**
 * This enumeration lists the V4L2 nodes exposed by the InputSystem
 */
enum IsysNodeTypes {
    ISYS_NODE_NULL =                0,
    ISYS_NODE_CSI_RX0 =             1 << 0,
    ISYS_NODE_CSI_RX1 =             1 << 1,
    ISYS_NODE_CSI_RX2 =             1 << 2,
    ISYS_NODE_CSI_RX3 =             1 << 3,
    ISYS_NODE_CSI_RX4 =             1 << 4,
    ISYS_NODE_CSI_RX5 =             1 << 5, // All CSI RX nodes output MIPI_COMRESSED in case of RAW sensors (ie. packed RAW including MIPI headers)
    ISYS_NODE_CSI_BE_RAW =          1 << 6, // Used with RAW sensors, outputs vectorized RAW Bayer without MIPI headers
    ISYS_NODE_CSI_BE_SOC =          1 << 7, // Support RAW Bayer and YUV output, it's unvectorized and unpacked, don't support cropping
    ISYS_NODE_ISA_CFG =             1 << 8,
    ISYS_NODE_ISA_OUT =             1 << 9, // Vectorized RAW Bayer with ISA processing
    ISYS_NODE_ISA_OUT_DOWNSCALED =  1 << 10,// Downscaled and vectorized RAW Bayer or vectorized YUV
    ISYS_NODE_ISA_3A =              1 << 11,
    ISYS_NODE_METADATA =            1 << 12,
    ISYS_NODE_TPG_OUT =             1 << 13
};
} NAMESPACE_DECLARATION_END

#endif  //  _CAMERA3_HAL_ISYS_TYPES_H_
