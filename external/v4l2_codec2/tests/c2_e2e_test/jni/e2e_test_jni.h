// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef C2_E2E_TEST_E2E_TEST_JNI_H_
#define C2_E2E_TEST_E2E_TEST_JNI_H_

#include <android/native_window.h>

namespace android {

// Callback to communicate from the test back to the Activity.
class ConfigureCallback {
public:
    // Provides a reference to the current test's decoder, or clears the reference.
    virtual void OnDecoderReady(void* decoder) = 0;
    // Configures the surface with the size of the current video.
    virtual void OnSizeChanged(int width, int height) = 0;
    virtual ~ConfigureCallback() = default;
};

}  // namespace android

int RunDecoderTests(char** test_args, int test_args_count, ANativeWindow* window,
                    android::ConfigureCallback* cb);

int RunEncoderTests(char** test_args, int test_args_count);

#endif  // C2_E2E_TEST_E2E_TEST_JNI_H_
