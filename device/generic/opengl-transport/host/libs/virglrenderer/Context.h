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

#undef NDEBUG

#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "GLESv1.h"
#include "GLESv3.h"
#include "RenderControl.h"
#include "Resource.h"

struct EglContext;
struct Context;

typedef void (*PFNSUBMITCMD)(Context*, char*, size_t, int);

struct Context {
    static std::map<uint32_t, Context*> map;

    Context(uint32_t handle_, const char* name_, uint32_t nlen_, PFNSUBMITCMD pfnProcessCmd_,
            EGLDisplay dpy_)
        : render_control(this, dpy_), worker(), name(std::string(name_, nlen_)), handle(handle_),
          pfnProcessCmd(pfnProcessCmd_) {
        map.emplace(handle, this);
        reset();
    }

    ~Context() {
        {
            std::lock_guard<std::mutex> lk(m);
            killWorker = true;
        }
        cv.notify_one();
        if (worker.joinable())
            worker.join();
        map.erase(handle);
    }

    Context* bind(EglContext* ctx_) {
        for (auto const& it : Context::map) {
            Context* ctx = it.second;
            if (ctx == this)
                continue;
            if (ctx->ctx == ctx_)
                return ctx;
        }
        ctx = ctx_;
        return nullptr;
    }

    void unbind() {
        ctx = nullptr;
    }

    void setPidTid(int pid_, int tid_) {
        if (pid != pid_ && tid != tid_) {
            assert(!worker.joinable() && "Changing pid/tid is not allowed");
            worker = std::thread(&Context::worker_func, this);
        }
        pid = pid_;
        tid = tid_;
    }

    void submitCommand(void* buf, size_t bufSize) {
        char* cmdBufCopy = new char[bufSize];
        memcpy(cmdBufCopy, buf, bufSize);
        {
            std::lock_guard<std::mutex> lk(m);
            cmdBufSize = bufSize;
            cmdBuf = cmdBufCopy;
        }
        cv.notify_one();
    }

    void setFence(int fence_) {
        {
            std::lock_guard<std::mutex> lk(m);
            fence = fence_;
            if (!worker.joinable())
                processCmd();
        }
        cv.notify_one();
    }

    std::map<uint32_t, Resource*> resource_map;
    ChecksumCalculator checksum_calc;
    RenderControl render_control;
    Resource* cmd_resp = nullptr;
    EglContext* ctx = nullptr;
    std::thread worker;
    std::string name;
    uint32_t handle;
    GLESv1 gles1;
    GLESv3 gles3;
    int pid = 0;
    int tid = 0;

  private:
    std::condition_variable cv;
    PFNSUBMITCMD pfnProcessCmd;
    bool killWorker = false;
    size_t cmdBufSize;
    char* cmdBuf;
    std::mutex m;
    int fence;

    void reset() {
        cmdBuf = nullptr;
        cmdBufSize = 0U;
        fence = 0;
    }

    void worker_func() {
        while (!killWorker) {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [this] { return killWorker || (cmdBuf && fence); });
            if (!killWorker)
                processCmd();
            lk.unlock();
        }
    }

    void processCmd() {
        pfnProcessCmd(this, cmdBuf, cmdBufSize, fence);
        delete cmdBuf;
        reset();
    }
};
