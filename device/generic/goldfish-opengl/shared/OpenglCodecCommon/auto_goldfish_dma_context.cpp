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

#include "auto_goldfish_dma_context.h"

namespace {
goldfish_dma_context empty() {
    goldfish_dma_context ctx;

    ctx.mapped_addr = 0;
    ctx.size = 0;
    ctx.fd = -1;

    return ctx;
}

void destroy(goldfish_dma_context *ctx) {
    if (ctx->mapped_addr) {
        goldfish_dma_unmap(ctx);
    }
    if (ctx->fd > 0) {
        goldfish_dma_free(ctx);
    }
}
}  // namespace

AutoGoldfishDmaContext::AutoGoldfishDmaContext() : m_ctx(empty()) {}

AutoGoldfishDmaContext::AutoGoldfishDmaContext(goldfish_dma_context *ctx)
    : m_ctx(*ctx) {
    *ctx = empty();
}

AutoGoldfishDmaContext::~AutoGoldfishDmaContext() {
    destroy(&m_ctx);
}

void AutoGoldfishDmaContext::reset(goldfish_dma_context *ctx) {
    destroy(&m_ctx);
    if (ctx) {
        m_ctx = *ctx;
        *ctx = empty();
    } else {
        m_ctx = empty();
    }
}

goldfish_dma_context AutoGoldfishDmaContext::release() {
    goldfish_dma_context copy = m_ctx;
    m_ctx = empty();
    return copy;
}

