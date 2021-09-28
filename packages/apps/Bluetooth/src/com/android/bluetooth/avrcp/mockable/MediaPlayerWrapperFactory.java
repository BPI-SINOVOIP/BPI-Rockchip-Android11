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

package com.android.bluetooth.avrcp;

import android.os.Looper;

import com.android.internal.annotations.VisibleForTesting;

/**
 * Provide a method to inject custom MediaControllerWrapper objects for testing. By using the
 * factory methods instead of calling the wrap method of MediaControllerWrapper directly, we can
 * inject a custom MediaControllerWrapper that can be used with JUnit and Mockito to set
 * expectations and validate behaviour in tests.
 */
public final class MediaPlayerWrapperFactory {
    private static MediaPlayerWrapper sInjectedWrapper;

    static MediaPlayerWrapper wrap(MediaController controller, Looper looper) {
        if (sInjectedWrapper != null) return sInjectedWrapper;
        if (controller == null || looper == null) {
            return null;
        }

        MediaPlayerWrapper newWrapper;
        if (controller.getPackageName().equals("com.google.android.music")) {
            newWrapper = new GPMWrapper(controller, looper);
        } else {
            newWrapper = new MediaPlayerWrapper(controller, looper);
        }
        return newWrapper;
    }

    @VisibleForTesting
    static void inject(MediaPlayerWrapper wrapper) {
        sInjectedWrapper = wrapper;
    }
}

