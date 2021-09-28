/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "macaddress.h"
#include "pollable.h"
#include "result.h"

#include <string>
#include <unordered_set>

struct Ieee80211Header;
struct pcap;
typedef struct pcap pcap_t;

class WifiForwarder : public Pollable {
public:
    explicit WifiForwarder(const char* monitorInterfaceName);
    ~WifiForwarder();

    Result init();

    // Pollable interface
    void getPollData(std::vector<pollfd>* fds) const override;
    Timestamp getTimeout() const override;
    bool onReadAvailable(int fd, int* status) override;
    bool onClose(int fd, int* status) override;
    bool onTimeout(int* status) override;
private:
    void forwardFromPcap();
    void injectFromPipe();
    void cleanup();

    std::string mInterfaceName;
    Pollable::Timestamp mDeadline;
    std::vector<unsigned char> mMonitorBuffer;
    pcap_t* mMonitorPcap;
    int mPipeFd;
};
