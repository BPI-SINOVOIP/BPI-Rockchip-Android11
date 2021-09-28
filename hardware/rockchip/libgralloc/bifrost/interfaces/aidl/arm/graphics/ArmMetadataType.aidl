/*
 * Copyright (C) 2020 Arm Limited.
 * SPDX-License-Identifier: Apache-2.0
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

package arm.graphics;

/**
 * Used by IAllocator/IMapper (gralloc) to describe Arm private metadata types.
 *
 * This is an enum that defines Arm's types of gralloc 4 buffer metadata.
 */
@VintfStability
@Backing(type="long")
enum ArmMetadataType {
    INVALID = 0,

    /**
     * Gives a list of FDs as int64_ts one for each plane of the buffer. These
     * then correspond to the planeLayouts returned by
     * android.hardware.graphics.common.StandardMetadataType::PLANE_LAYOUTS
     */
    PLANE_FDS = 1,
}
