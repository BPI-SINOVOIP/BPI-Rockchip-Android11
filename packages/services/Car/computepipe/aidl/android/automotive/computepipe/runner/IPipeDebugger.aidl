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

import android.automotive.computepipe.runner.PipeProfilingType;
import android.automotive.computepipe.runner.ProfilingData;

/**
 * interface to debug and profile a graph
 */
@VintfStability
interface IPipeDebugger {
    /**
     * Set the debug options for a pipe. The profiling options can be an
     * externsion of the options mentioned here.
     * https://mediapipe.readthedocs.io/en/latest/measure_performance.html
     * This should be done prior to the applyPipeConfigs() call.
     *
     * @param type: The type of profiling a client wants to enable
     */
    void setPipeProfileOptions(in PipeProfilingType type);

    /**
     * Start the profiling for the mediapipe graph.
     * This should be issued after the pipe has transitioned to the PipeState::RUNNING
     *
     * @param out if starting profiling was successful it returns binder::Status::OK
     */
    void startPipeProfiling();

    /**
     * Stop the profiling for the mediapipe graph.
     * This can be done at any point of after a pipe is in RUNNING state.
     *
     * @param out if stoping profiling was successful, it returns binder::Status::OK
     */
    void stopPipeProfiling();

    /**
     * Retrieve the profiling information
     * This can be done after a stopPipeProfiling() call or if a PipeState::DONE
     * notification has been received.
     *
     * This is a polling api, If the pipe crashes, any calls to this api will fail.
     * It blocks until profiling information is available.
     * It returns the profiling data associated with the profiling options
     * chosen by setPipe*().
     * The profiling data is not retained after a call to resetPipeConfigs().
     */
    ProfilingData getPipeProfilingInfo();

    /**
     * Clear up all client specific resources.
     *
     * This clears out any gathered profiling data.
     * This also resets the profiling configuration chosen by the client.
     * After this method is invoked, client will be responsible for
     * reconfiguring the profiling steps.
     *
     * @param out OK if release was configured successfully.
     */
    void releaseDebugger();
}
