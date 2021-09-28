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

#include "RTGraphicWindowApi.h"

#define AVCOL_TRC_SMPTEST2084   16
#define AVCOL_TRC_ARIB_STD_B67  18
#define AVCOL_TRC_BT2020_10     14

namespace android
{

GraphicWindowApi::GraphicWindowApi() {

}

GraphicWindowApi::~GraphicWindowApi() {

}

void GraphicWindowApi::SetSwapInterval(ANativeWindow* window, int interval) {
    window->setSwapInterval(window, interval);
}

void GraphicWindowApi::SetColorSpace(ANativeWindow* window, int trc) {
    if (AVCOL_TRC_SMPTEST2084 == trc) {
        native_window_set_buffers_data_space(window,(android_dataspace_t)(HAL_DATASPACE_TRANSFER_ST2084 | HAL_DATASPACE_STANDARD_BT2020 | HAL_DATASPACE_RANGE_LIMITED));
    } else if (AVCOL_TRC_ARIB_STD_B67 == trc) {
        native_window_set_buffers_data_space(window, (android_dataspace_t)(HAL_DATASPACE_TRANSFER_HLG | HAL_DATASPACE_STANDARD_BT2020 | HAL_DATASPACE_RANGE_LIMITED));
    } else if (AVCOL_TRC_BT2020_10 == trc) {
        native_window_set_buffers_data_space(window, (android_dataspace_t)(HAL_DATASPACE_STANDARD_BT2020 | HAL_DATASPACE_RANGE_LIMITED));
    }
}


//SurfaceComposerClient&SurfaceControl
void GraphicWindowApi::OpenSurfaceTransaction() {
}

void GraphicWindowApi::SetSurfacePosition(const sp<SurfaceControl>& sc, Transaction *t, float x, float y) {
    t->setPosition(sc, x, y);
}

void GraphicWindowApi::SetSurfaceSize(const sp<SurfaceControl>& sc, Transaction *t, uint32_t w, uint32_t h) {
    t->setSize(sc, w, h);
}

void GraphicWindowApi::SetSurfaceLayer(const sp<SurfaceControl>& sc, Transaction *t, int32_t z) {
    t->setLayer(sc, z);
}

void GraphicWindowApi::SetSurfaceLayerStack(const sp<SurfaceControl>& sc, Transaction *t, uint32_t layerStack) {
    t->setLayerStack(sc, layerStack);
}

void GraphicWindowApi::ShowSurface(const sp<SurfaceControl>& sc, Transaction *t) {
    t->show(sc);
}

void GraphicWindowApi::HideSurface(const sp<SurfaceControl>& sc, Transaction *t) {
    t->hide(sc);
}

void GraphicWindowApi::CloseSurfaceTransaction(Transaction *t) {
    t->apply();
}

}

