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

import android.os.IBinder;
import android.server.wm.jetpack.wrapper.extensionwrapperimpl.TestExtensionCompat;
import android.server.wm.jetpack.wrapper.sidecarwrapperimpl.TestSidecarCompat;

import androidx.annotation.NonNull;

/**
 * Test interface for {@link androidx.window.extensions.ExtensionInterface} and
 * {@link androidx.window.sidecar.SidecarInterface} that serves as an API compatibility wrapper.
 *
 * @see TestExtensionCompat
 * @see TestSidecarCompat
 */
public interface TestInterfaceCompat {

    /**
     * Registers the support library as the callback for the extension. This interface will be used
     * to report all extension changes to the support library.
     */
    void setExtensionCallback(@NonNull TestInterfaceCallback callback);

    /**
     * Gets current information about the display features present within the application window.
     */
    TestWindowLayoutInfo getWindowLayoutInfo(@NonNull IBinder windowToken);

    /**
     * Notifies extension that a listener for display feature layout changes was registered for the
     * given window token.
     */
    void onWindowLayoutChangeListenerAdded(@NonNull IBinder windowToken);

    /**
     * Notifies extension that a listener for display feature layout changes was removed for the
     * given window token.
     */
    void onWindowLayoutChangeListenerRemoved(@NonNull IBinder windowToken);

    /**
     * Gets current device state.
     * @see #onDeviceStateListenersChanged(boolean)
     */
    TestDeviceState getDeviceState();

    /**
     * Notifies the extension that a device state change listener was updated.
     * @param isEmpty flag indicating if the list of device state change listeners is empty.
     */
    void onDeviceStateListenersChanged(boolean isEmpty);

    /**
     * Callback that will be registered with the WindowManager library, and that the extension
     * should use to report all state changes.
     */
    interface TestInterfaceCallback {
        /**
         * Called by extension when the device state changes.
         */
        void onDeviceStateChanged(@NonNull TestDeviceState newDeviceState);

        /**
         * Called by extension when the feature layout inside the window changes.
         */
        void onWindowLayoutChanged(@NonNull IBinder windowToken,
                @NonNull TestWindowLayoutInfo newLayout);
    }
}
