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

#include "command.h"
#include "result.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Bridge;

class WifiCommand : public Command {
public:
    explicit WifiCommand(Bridge& bridge);
    virtual ~WifiCommand() = default;

    Result onCommand(const char* command, const char* args) override;
private:
    void readConfig();
    Result writeConfig();
    Result triggerHostApd();

    Result onAdd(const std::vector<std::string>& args);
    Result onBlock(const std::vector<std::string>& args);
    Result onUnblock(const std::vector<std::string>& args);

    struct AccessPoint {
        AccessPoint() : blocked(false) {}
        std::string ifName;
        std::string ssid;
        std::string password;
        bool blocked;
    };

    Bridge& mBridge;
    std::unordered_map<std::string, AccessPoint> mAccessPoints;
    std::unordered_set<std::string> mUsedInterfaces;
    int mLowestInterfaceNumber;
};

