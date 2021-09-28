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

// Fork and run the provided program with arguments and wait until the program
// exits. The list of arguments in |argv| has to be terminated by a NULL
// pointer (e.g. { "ls", "-l", "/", nullptr } to run 'ls -l /'
// Returns true if the process exits normally with a return code of 0. Returns
// false if the process is terminated by a signal or if it exits with a return
// code that is not 0.
bool forkAndExec(const char* argv[]);

