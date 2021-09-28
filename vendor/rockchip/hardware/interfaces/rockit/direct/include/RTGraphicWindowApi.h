/*
 * Copyright 2018 Rockchip Electronics Co. LTD
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
 *
 * author: xhr@rock-chips.com
 * date:   2019/12/24
 */

#ifndef ROCKIT_DIRECT_RTGRAPHICWINDOW_H_
#define ROCKIT_DIRECT_RTGRAPHICWINDOW_H_

#include <utils/RefBase.h>
#include <system/window.h>
#include <ui/GraphicBuffer.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/SurfaceControl.h>


namespace android
{
using Transaction = SurfaceComposerClient::Transaction;

class GraphicWindowApi {
public:
    GraphicWindowApi();
    virtual     ~GraphicWindowApi();

    static void                     SetSwapInterval(ANativeWindow* window, int interval);
    static void                     SetColorSpace(ANativeWindow* window, int trc);
    //SurfaceComposerClient&SurfaceControl
    static void                     OpenSurfaceTransaction();
    static void                     SetSurfacePosition(const sp<SurfaceControl>& sc, Transaction *t, float x, float y);
    static void                     SetSurfaceSize(const sp<SurfaceControl>& sc, Transaction *t, uint32_t w, uint32_t h);
    static void                     SetSurfaceLayer(const sp<SurfaceControl>& sc, Transaction *t, int32_t z);
    static void                     SetSurfaceLayerStack(const sp<SurfaceControl>& sc, Transaction *t, uint32_t layerStack);
    static void                     ShowSurface(const sp<SurfaceControl>& sc, Transaction *t);
    static void                     HideSurface(const sp<SurfaceControl>& sc, Transaction *t);
    static void                     CloseSurfaceTransaction(Transaction *t);
};
}

#endif  //  ROCKIT_DIRECT_RTGRAPHICWINDOW_H_

