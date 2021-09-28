/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>
#include <map>

#include <EGL/egl.h>

struct EglContext {
    enum GLESApi {
        GLESApi_CM = 1,
        GLESApi_2 = 2,
        GLESApi_3_0 = 3,
        GLESApi_3_1 = 4,
    };

    static std::map<uint32_t, EglContext*> map;
    static uint32_t nextId;

    EglContext(EGLContext context_, uint32_t ctx_, GLESApi api_)
        : create_ctx(ctx_), context(context_), api(api_), id(nextId++) {
        map.emplace(id, this);
    }

    ~EglContext() {
        map.erase(id);
    }

    EglContext* bind(uint32_t ctx_) {
        for (auto const& it : EglContext::map) {
            EglContext* ctx = it.second;
            if (ctx == this)
                continue;
            if (ctx->bound_ctx == ctx_)
                return ctx;
        }
        bound_ctx = ctx_;
        return nullptr;
    }

    void unbind() {
        bound_ctx = 0U;
    }

    bool disposable() {
        return context == EGL_NO_CONTEXT && bound_ctx == 0U;
    }

    uint32_t create_ctx;
    EGLContext context;
    enum GLESApi api;
    uint32_t id;

  private:
    uint32_t bound_ctx = 0U;
};
