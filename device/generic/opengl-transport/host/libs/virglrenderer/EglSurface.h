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

struct ANativeWindow;

struct EglSurface {
    static std::map<uint32_t, EglSurface*> map;
    static uint32_t nextId;

    EglSurface(EGLConfig config_, uint32_t ctx_, uint32_t width_, uint32_t height_)
        : create_ctx(ctx_), config(config_), height(height_), width(width_), id(nextId++) {
        map.emplace(id, this);
    }

    ~EglSurface() {
        map.erase(id);
    }

    EglSurface* bind(uint32_t ctx_, bool read_) {
        for (auto const& it : EglSurface::map) {
            EglSurface* sur = it.second;
            if (sur == this)
                continue;
            if (sur->bound_ctx == ctx_) {
                if (read_ && sur->read)
                    return sur;
                if (!read_ && sur->draw)
                    return sur;
            }
        }
        if (read_) {
            read = true;
        } else {
            draw = true;
        }
        bound_ctx = ctx_;
        return nullptr;
    }

    void unbind(bool read_) {
        if (read || draw) {
            if (read_)
                read = false;
            else
                draw = false;
            if (read || draw)
                return;
            bound_ctx = 0U;
        }
        return;
    }

    bool disposable() {
        return surface == EGL_NO_SURFACE && bound_ctx == 0U;
    }

    EGLSurface surface = EGL_NO_SURFACE;
    ANativeWindow* window = nullptr;
    uint32_t create_ctx;
    EGLConfig config;
    uint32_t height;
    uint32_t width;
    uint32_t id;

  private:
    uint32_t bound_ctx = 0U;
    bool draw = false;
    bool read = false;
};
