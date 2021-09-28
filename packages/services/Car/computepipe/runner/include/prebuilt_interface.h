/*
 * Copyright 2019 The Android Open Source Project
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

#ifndef COMPUTEPIPE_RUNNER_INCLUDE_PREBUILT_INTERFACE_H_
#define COMPUTEPIPE_RUNNER_INCLUDE_PREBUILT_INTERFACE_H_

#include <cstddef>
#include <cstdint>

#define COMPUTEPIPE_RUNNER(a) PrebuiltComputepipeRunner_##a

extern "C" {

// Enum value to report the error code for function calls.
enum PrebuiltComputepipeRunner_ErrorCode {
    SUCCESS = 0,
    INTERNAL_ERROR,
    INVALID_ARGUMENT,
    ILLEGAL_STATE,
    NO_MEMORY,
    FATAL_ERROR,
    ERROR_CODE_MAX,
};

enum PrebuiltComputepipeRunner_PixelDataFormat {
    RGB = 0,
    RGBA = 1,
    GRAY = 2,
    PIXEL_DATA_FORMAT_MAX = 3,
};

// Gets the version of the library. The runner should check if the version of
// the prebuilt matches the version of android runner for which it was built
// and fail out if needed.
const unsigned char* COMPUTEPIPE_RUNNER(GetVersion)();

// Gets the error code. This API is necessary because the graph execution is
// asynchronous and even if the function calls to execute the graph succeed, it
// could fail at a later point in the execution of the graph.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(GetErrorCode)();

// Gets the graph error message from the graph. The return value is not the
// graph error code but the error code returned if the call to the function
// fails.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(GetErrorMessage)(
    unsigned char* error_msg_buffer, size_t error_msg_buffer_size, size_t* error_msg_size);

// Gets the supported graph config options. This is ideally generated once and
// cached for subsequent calls.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(GetSupportedGraphConfigs)(
    const void** config, size_t* config_size);

// Sets the graph configuration or updates it if an incomplete config is passed.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(UpdateGraphConfig)(
    const unsigned char* graph_config, size_t graph_config_size);

// Sets the stream contents. This can only be used after the graph has started
// running successfully. The contents of this stream are typically a serialized
// proto and would be deserialized and fed into the graph.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(SetInputStreamData)(
    int stream_index, int64_t timestamp, const unsigned char* stream_data, size_t stream_data_size);

// Sets the pixel data as stream contents. This can be set only after the graph
// has started running successfully. Pixel data should be copied within this
// function as there are no guarantess on the lifetime of the pixel data beyond
// the return of this function call.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(SetInputStreamPixelData)(
    int stream_index, int64_t timestamp, const uint8_t* pixels, int width, int height, int step,
    int format);

// Sets a callback function for when a packet is generated. Note that a c-style
// function needs to be passed as no object context is being passed around here.
// The runner would be responsible for using the buffer provided in the callback
// immediately or copying it as there are no guarantees on its lifetime beyond
// the return of the callback.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(SetOutputStreamCallback)(
    void (*streamCallback)(void* cookie, int stream_index, int64_t timestamp,
                           const unsigned char* data, size_t data_size));

// Sets a callback function for when new pixel data is generated. C-style
// function pointers need to passed as no object context is being passed around.
// The runner would be responsible for immediately copying out the data. The
// prebuilt is expected to pass contiguous data.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(SetOutputPixelStreamCallback)(
    void (*streamCallback)(void* cookie, int stream_index, int64_t timestamp, const uint8_t* pixels,
                           int width, int height, int step, int format));

// Sets a callback function for when the graph terminates.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(SetGraphTerminationCallback)(
    void (*terminationCallback)(void* cookie, const unsigned char* termination_message,
                                size_t termination_message_size));

// Starts the graph execution. Debugging can be enabled which will enable
// profiling. The profiling info can be obtained by calling getDebugInfo once
// the graph execution has stopped.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(StartGraphExecution)(void* cookie,
                                                                            bool debugging_enabled);

// Stops the graph execution.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(StopGraphExecution)(bool flushOutputFrames);

// Resets the graph completely. Should be called only after graph execution has been stopped.
void COMPUTEPIPE_RUNNER(ResetGraph)();

// Get debugging/profiling information. The function outputs the size of
// profiling information string and if the buffer size is larger than or equal
// to the size, then it copies it over to the buffer. Debugging info will be
// empty if the graph is started without debugging support.
PrebuiltComputepipeRunner_ErrorCode COMPUTEPIPE_RUNNER(GetDebugInfo)(unsigned char* debug_info,
                                                                     size_t debug_info_buffer_size,
                                                                     size_t* debug_info_size);
}
#endif  // COMPUTEPIPE_RUNNER_INCLUDE_PREBUILT_INTERFACE_H_
