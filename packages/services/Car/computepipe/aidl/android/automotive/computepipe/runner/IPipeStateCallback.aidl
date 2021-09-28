/*
 * Copyright (C) 2019 The Android Open Source Project
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
package android.automotive.computepipe.runner;

import android.automotive.computepipe.runner.PipeState;

@VintfStability
interface IPipeStateCallback {
    /**
     * Callback that notifies a client about the state of a pipe
     *
     * Client installs IPipeStateHandler with the runner by invoking
     * setPipeStateNotifier(). The runner invokes the method below to notify the
     * client of any state changes.
     *
     * @param state is the state of the pipe.
     */
    oneway void handleState(in PipeState state);
}
