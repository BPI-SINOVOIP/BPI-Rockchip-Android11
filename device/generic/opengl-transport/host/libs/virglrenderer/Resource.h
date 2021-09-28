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

extern "C" {
#include <virglrenderer.h>
}

#include <cstdint>
#include <cstdlib>
#include <map>

#include <GLES/gl.h>

#include <sys/uio.h>

#include "EglImage.h"

struct Context;

struct Resource {
    static std::map<uint32_t, Resource*> map;

    Resource(virgl_renderer_resource_create_args* args_, uint32_t num_iovs_, iovec* iov_)
        : args(*args_), num_iovs(num_iovs_), tex_id(0U), iov(iov_) {
        reallocLinear();
        map.emplace(args.handle, this);
    }

    ~Resource() {
        map.erase(args.handle);
        delete image;
    }

    void reallocLinear() {
        if (linearShadow && num_iovs <= 1)
            free(linear);
        linearShadow = num_iovs > 1 ? true : false;
        if (linearShadow) {
            uint32_t i;
            for (i = 0, linearSize = 0U; i < num_iovs; i++)
                linearSize += iov[i].iov_len;
            linear = realloc(linear, linearSize);
        } else if (num_iovs == 1) {
            linearSize = iov[0].iov_len;
            linear = iov[0].iov_base;
        } else {
            linearSize = 0U;
            linear = nullptr;
        }
    }

    std::map<uint32_t, Context*> context_map;
    virgl_renderer_resource_create_args args;
    EglImage* image = nullptr;
    size_t linearSize = 0U;
    void* linear = nullptr;
    uint32_t num_iovs;
    GLuint tex_id;
    iovec* iov;

  private:
    bool linearShadow = false;
};
