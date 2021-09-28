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

#include "poller.h"

#include "log.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include <unordered_map>
#include <vector>

using std::chrono::duration_cast;

static struct timespec* calculateTimeout(Pollable::Timestamp deadline,
                                         struct timespec* ts) {
    Pollable::Timestamp now = Pollable::Clock::now();
    if (deadline < Pollable::Timestamp::max()) {
        if (deadline <= now) {
            LOGE("Poller found past due deadline, setting to zero");
            ts->tv_sec = 0;
            ts->tv_nsec = 0;
            return ts;
        }

        auto timeout = deadline - now;
        // Convert and round down to seconds
        auto seconds = duration_cast<std::chrono::seconds>(timeout);
        // Then subtract the seconds from the timeout and convert the remainder
        auto nanos = duration_cast<std::chrono::nanoseconds>(timeout - seconds);

        ts->tv_sec = seconds.count();
        ts->tv_nsec = nanos.count();

        return ts;
    }
    return nullptr;
}

Poller::Poller() {
}

void Poller::addPollable(Pollable* pollable) {
    mPollables.push_back(pollable);
}

int Poller::run() {
    // Block all signals while we're running. This way we don't have to deal
    // with things like EINTR. We then uses ppoll to set the original mask while
    // polling. This way polling can be interrupted but socket writing, reading
    // and ioctl remain interrupt free. If a signal arrives while we're blocking
    // it it will be placed in the signal queue and handled once ppoll sets the
    // original mask. This way no signals are lost.
    sigset_t blockMask, mask;
    int status = ::sigfillset(&blockMask);
    if (status != 0) {
        LOGE("Unable to fill signal set: %s", strerror(errno));
        return errno;
    }
    status = ::sigprocmask(SIG_SETMASK, &blockMask, &mask);
    if (status != 0) {
        LOGE("Unable to set signal mask: %s", strerror(errno));
        return errno;
    }

    std::vector<struct pollfd> fds;
    std::unordered_map<int, Pollable*> pollables;
    while (true) {
        fds.clear();
        pollables.clear();
        Pollable::Timestamp deadline = Pollable::Timestamp::max();
        for (auto& pollable : mPollables) {
            size_t start = fds.size();
            pollable->getPollData(&fds);
            Pollable::Timestamp pollableDeadline = pollable->getTimeout();
            // Create a map from each fd to the pollable
            for (size_t i = start; i < fds.size(); ++i) {
                pollables[fds[i].fd] = pollable;
            }
            if (pollableDeadline < deadline) {
                deadline = pollableDeadline;
            }
        }

        struct timespec ts = { 0, 0 };
        struct timespec* tsPtr = calculateTimeout(deadline, &ts);
        status = ::ppoll(fds.data(), fds.size(), tsPtr, &mask);
        if (status < 0) {
            if (errno == EINTR) {
                // Interrupted, just keep going
                continue;
            }
            // Actual error, time to quit
            LOGE("Polling failed: %s", strerror(errno));
            return errno;
        } else if (status > 0) {
            // Check for read or close events
            for (const auto& fd : fds) {
                if ((fd.revents & (POLLIN | POLLHUP)) == 0) {
                    // Neither POLLIN nor POLLHUP, not interested
                    continue;
                }
                auto pollable = pollables.find(fd.fd);
                if (pollable == pollables.end()) {
                    // No matching fd, weird and unexpected
                    LOGE("Poller could not find fd matching %d", fd.fd);
                    continue;
                }
                if (fd.revents & POLLIN) {
                    // This pollable has data available for reading
                    int status = 0;
                    if (!pollable->second->onReadAvailable(fd.fd, &status)) {
                        // The onReadAvailable handler signaled an exit
                        return status;
                    }
                }
                if (fd.revents & POLLHUP) {
                    // The fd was closed from the other end
                    int status = 0;
                    if (!pollable->second->onClose(fd.fd, &status)) {
                        // The onClose handler signaled an exit
                        return status;
                    }
                }
            }
        }
        // Check for timeouts
        Pollable::Timestamp now = Pollable::Clock::now();
        for (const auto& pollable : mPollables) {
            if (pollable->getTimeout() <= now) {
                int status = 0;
                if (!pollable->onTimeout(&status)) {
                    // The onTimeout handler signaled an exit
                    return status;
                }
            }
        }
    }

    return 0;
}
