/*
 * Copyright (C) 2019 Rockchip Electronics Co. LTD
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
 * This class defines the interfaces for external use
 *
 ******************************************              **********************************
 *                 System                 *              *           Vendor               *
 *                                        *              *                                *
 *                                        *              *                                *
 * rockit                                 *              *                                *
 *   |                                    *              *                                *
 *   ----->RockitHwClientInterface        *              *         RockitHwService        *
 *                   |                    *     HIDL     *                |               *
 *                   ----->RockitHwClient------------------------  RockitHwManager        *
 *                                        *              *                |               *
 *                                        *              *      --------------------      *
 *                                        *              *      |    |    |    |   |      *
 *                                        *              *      MPI MPP VPUAPI RGA others *
 *                                        *              *                                *
 *        MediaPlayer Process             *              *                                *
 *                                        *              *                                *
 ******************************************              **********************************
 *
 */
#ifndef ANDROID_ROCKIT_HW_CLIENT_INTERFACE_H
#define ANDROID_ROCKIT_HW_CLIENT_INTERFACE_H

#include "RockitHwClient.h"

namespace android {

class RockitHwClientInterface {
public:
    RockitHwClientInterface();
    virtual ~RockitHwClientInterface();
    virtual int init(int type,void* param);
    virtual int commitBuffer(RTHWBuffer& buffer);
    virtual int giveBackBuffer(RTHWBuffer& buffer);
    virtual int process(RockitHWBufferList& list);
    virtual int enqueue(RTHWBuffer& data);
    virtual int dequeue(RTHWBuffer* buffer);
    virtual int control(uint32_t cmd, void* param);
    virtual int query(uint32_t cmd, void* param);
    virtual int flush();
    virtual int reset();

protected:
    RockitHwClient* mClient;
};

}  //  ANDROID_ROCKIT_HW_CLIENT_INTERFACE_H

#endif