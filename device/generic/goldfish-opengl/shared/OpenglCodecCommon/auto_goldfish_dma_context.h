// Copyright 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ANDROID_INCLUDE_HARDWARE_AUTO_GOLDFISH_DMA_CONTEXT_H
#define ANDROID_INCLUDE_HARDWARE_AUTO_GOLDFISH_DMA_CONTEXT_H

#include <inttypes.h>
#include "goldfish_dma.h"

// A C++ wrapper for goldfish_dma_context that releases resources in dctor.
class AutoGoldfishDmaContext {
public:
    AutoGoldfishDmaContext();
    explicit AutoGoldfishDmaContext(goldfish_dma_context *ctx);
    ~AutoGoldfishDmaContext();

    const goldfish_dma_context &get() const { return m_ctx; }
    void reset(goldfish_dma_context *ctx);
    goldfish_dma_context release();

private:
    AutoGoldfishDmaContext(const AutoGoldfishDmaContext &rhs);
    AutoGoldfishDmaContext& operator=(const AutoGoldfishDmaContext &rhs);

    goldfish_dma_context m_ctx;
};

#endif  // ANDROID_INCLUDE_HARDWARE_AUTO_GOLDFISH_DMA_CONTEXT_H
