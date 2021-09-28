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

#include "../result.h"

// An interface for commands coming through the pipe. Note that each instance
// can handle any number of commands, all it has to do is differentiate based
// on the incoming command string or arguments. It will have to be registered
// multiple times, once for each command though.
class Command {
public:
    virtual ~Command() = default;

    // Work to perform when a command is received. The result will be used to
    // report the result to the user. If the result indicates success the user
    // will see an "OK" response, on failure the error message in the result
    // will be presented to the user. This means that the result error string
    // should be fairly user-friendly.
    virtual Result onCommand(const char* command, const char* args) = 0;

};

