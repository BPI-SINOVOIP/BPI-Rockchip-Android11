/*
 * Copyright 2019, The Android Open Source Project
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

#include "result.h"

#include <string>

class Bridge {
public:
    explicit Bridge(const std::string& bridgeName);
    ~Bridge();

    Result init();

    Result addInterface(const std::string& interfaceName);
    Result removeInterface(const std::string& interfaceName);
private:
    Result createSocket();
    Result createBridge();
    Result doInterfaceOperation(const std::string& interfaceName,
                                unsigned long operation,
                                const char* operationName);

    std::string mBridgeName;
    int mSocketFd = -1;
};
