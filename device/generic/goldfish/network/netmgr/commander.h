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

#include "pollable.h"
#include "result.h"

#include <unordered_map>
#include <vector>

class Command;

class Commander : public Pollable {
public:
    Commander();
    ~Commander();

    Result init();

    void registerCommand(const char* commandStr, Command* command);

    void getPollData(std::vector<pollfd>* fds) const override;
    Timestamp getTimeout() const override;
    bool onReadAvailable(int fd, int* status) override;
    bool onClose(int fd, int* status) override;
    bool onTimeout(int* status) override;
private:
    void openPipe();
    void closePipe();

    int mPipeFd;
    Pollable::Timestamp mDeadline;
    std::vector<char> mReceiveBuffer;
    std::unordered_map<std::string, Command*> mCommands;
};
