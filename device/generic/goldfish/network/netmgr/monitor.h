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

#include "interface_state.h"
#include "pollable.h"
#include "result.h"

#include <unordered_set>

/** Monitor network interfaces and provide notifications of changes to those
 *  interfaces.
 */
class Monitor : public Pollable {
public:
    using OnInterfaceStateCallback = std::function<void (unsigned int index,
                                                         const char* name,
                                                         InterfaceState state)>;
    Monitor();
    ~Monitor();

    Result init();

    void setOnInterfaceState(OnInterfaceStateCallback callback);

    // Pollable interface
    void getPollData(std::vector<pollfd>* fds) const override;
    Timestamp getTimeout() const override;
    bool onReadAvailable(int fd, int* status) override;
    bool onClose(int fd, int* status) override;
    bool onTimeout(int* status) override;

private:
    Result openSocket();
    Result requestInterfaces();
    void closeSocket();
    void handleNewLink(const struct nlmsghdr* hdr);

    int mSocketFd;
    OnInterfaceStateCallback mOnInterfaceStateCallback;
    std::unordered_set<unsigned int> mUpInterfaces;
};

