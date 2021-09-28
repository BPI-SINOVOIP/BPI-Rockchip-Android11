/*
 * Copyright 2018, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "commander.h"

#include "commands/command.h"
#include "log.h"

#include <errno.h>
#include <qemu_pipe_bp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

static const char kQemuPipeName[] = "qemud:network";

// How much space to use for each command received
static const size_t kReceiveSpace = 1024;
// The maximum amount of bytes to keep in the receive buffer for a single
// command before dropping data.
static const size_t kMaxReceiveBufferSize = 65536;

Commander::Commander() : mPipeFd(-1) {
}

Commander::~Commander() {
    closePipe();
}

Result Commander::init() {
    if (mPipeFd != -1) {
        return Result::error("Commander already initialized");
    }

    openPipe();

    return Result::success();
}

void Commander::registerCommand(const char* commandStr, Command* command) {
    mCommands[commandStr] = command;
}

void Commander::getPollData(std::vector<pollfd>* fds) const {
    if (mPipeFd != -1) {
        fds->push_back(pollfd{mPipeFd, POLLIN, 0});
    }
}

Pollable::Timestamp Commander::getTimeout() const {
    return mDeadline;
}

bool Commander::onReadAvailable(int /*fd*/, int* /*status*/) {
    size_t offset = mReceiveBuffer.size();
    mReceiveBuffer.resize(offset + kReceiveSpace);
    if (mReceiveBuffer.size() > kMaxReceiveBufferSize) {
        // We have buffered too much data, this should never happen but as a
        // seurity measure let's just drop everything we have and keep 
        // receiving. Maybe the situation will improve.
        mReceiveBuffer.resize(kReceiveSpace);
        offset = 0;
    }
    while (true) {
        int status = qemu_pipe_read(mPipeFd, &mReceiveBuffer[offset], kReceiveSpace);

        if (status < 0) {
            if (errno == EINTR) {
                // We got an interrupt, try again
                continue;
            }
            LOGE("Commander failed to receive on pipe: %s", strerror(errno));
            // Don't exit the looper because of this, keep trying
            return true;
        }
        size_t length = static_cast<size_t>(status);
        mReceiveBuffer.resize(offset + length);

        while (true) {
            auto endline = std::find(mReceiveBuffer.begin(),
                                     mReceiveBuffer.end(),
                                     '\n');
            if (endline == mReceiveBuffer.end()) {
                // No endline in sight, keep waiting and buffering
                return true;
            }

            *endline = '\0';

            char* args = ::strchr(mReceiveBuffer.data(), ' ');
            if (args) {
                *args++ = '\0';
            }
            auto command = mCommands.find(mReceiveBuffer.data());

            if (command != mCommands.end()) {
                command->second->onCommand(mReceiveBuffer.data(), args);
            }
            // Now that we have processed this line let's remove it from the
            // receive buffer
            ++endline;
            if (endline == mReceiveBuffer.end()) {
                mReceiveBuffer.clear();
                // There can be nothing left, just return
                return true;
            } else {
                mReceiveBuffer.erase(mReceiveBuffer.begin(), endline + 1);
                // There may be another line in there so keep looping and look
                // for more
            }
        }
        return true;
    }
}

bool Commander::onClose(int /*fd*/, int* /*status*/) {
    // Pipe was closed from the other end, close it on our side and re-open
    closePipe();
    openPipe();
    return true;
}

bool Commander::onTimeout(int* /*status*/) {
    if (mPipeFd == -1) {
        openPipe();
    }
    return true;
}

void Commander::openPipe() {
    mPipeFd = qemu_pipe_open_ns(NULL, kQemuPipeName, O_RDWR);
    if (mPipeFd == -1) {
        LOGE("Failed to open QEMU pipe '%s': %s",
             kQemuPipeName,
             strerror(errno));
        // Try again in the future
        mDeadline = Pollable::Clock::now() + std::chrono::minutes(1);
    } else {
        mDeadline = Pollable::Timestamp::max();
    }
}

void Commander::closePipe() {
    if (qemu_pipe_valid(mPipeFd)) {
        qemu_pipe_close(mPipeFd);
        mPipeFd = QEMU_PIPE_INVALID_HANDLE;
    }
}
