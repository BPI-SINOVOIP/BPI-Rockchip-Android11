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

#pragma once

#include <chrono>
#include <vector>

#include <poll.h>

/* An interface for pollable classes.
 */
class Pollable {
public:
    using Clock = std::chrono::steady_clock;
    using Timestamp = Clock::time_point;
    virtual ~Pollable() = default;

    /* Get the poll data for the next poll loop. The implementation can place
     * as many fds as needed in |fds|.
     */
    virtual void getPollData(std::vector<pollfd>* fds) const = 0;
    /* Get the timeout for the next poll loop. This should be a timestamp
     * indicating when the timeout should be triggered. Note that this may
     * be called at any time and any number of times for a poll loop so the
     * deadline should not be adjusted in this call, a set deadline should
     * just be returned. Note specifically that if a call to onReadAvailable
     * modifies the deadline the timeout for the previous timestamp might not
     * fire as the poller will check the timestamp AFTER onReadAvailable is
     * called.
     */
    virtual Timestamp getTimeout() const = 0;
    /* Called when there is data available to read on an fd associated with
     * the pollable. |fd| indicates which fd to read from. If the call returns
     * false the poller will exit its poll loop with a return code of |status|.
     */
    virtual bool onReadAvailable(int fd, int* status) = 0;
    /* Called when an fd associated with the pollable is closed. |fd| indicates
     * which fd was closed. If the call returns false the poller will exit its
     * poll loop with a return code of |status|.
     */
    virtual bool onClose(int fd, int* status) = 0;
    /* Called when the timeout returned by getPollData has been reached. If
     * the call returns false the poller will exit its poll loop with a return
     * code of |status|.
     */
    virtual bool onTimeout(Timestamp now, int* status) = 0;
};

