/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.server.wm.jetpack.wrapper;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Test interface for {@link androidx.window.extensions.ExtensionDeviceState} and
 * {@link androidx.window.sidecar.SidecarDeviceState} that serves as an API compatibility wrapper.
 *
 * @see android.server.wm.jetpack.wrapper.extensionwrapperimpl.TestExtensionDeviceState
 * @see android.server.wm.jetpack.wrapper.sidecarwrapperimpl.TestSidecarDeviceState
 */
public interface TestDeviceState {

    /** Copied from {@link androidx.window.extensions.ExtensionDeviceState}. */
    public static final int POSTURE_UNKNOWN = 0;
    public static final int POSTURE_CLOSED = 1;
    public static final int POSTURE_HALF_OPENED = 2;
    public static final int POSTURE_OPENED = 3;
    public static final int POSTURE_FLIPPED = 4;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
            POSTURE_UNKNOWN,
            POSTURE_CLOSED,
            POSTURE_HALF_OPENED,
            POSTURE_OPENED,
            POSTURE_FLIPPED
    })
    @interface Posture{}

    /** Gets the current posture of the foldable device. */
    @Posture
    int getPosture();
}
