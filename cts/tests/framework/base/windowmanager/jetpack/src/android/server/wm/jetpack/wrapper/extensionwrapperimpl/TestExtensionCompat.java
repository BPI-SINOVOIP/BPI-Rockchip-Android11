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

package android.server.wm.jetpack.wrapper.extensionwrapperimpl;

import android.os.IBinder;
import android.server.wm.jetpack.wrapper.TestDeviceState;
import android.server.wm.jetpack.wrapper.TestInterfaceCompat;
import android.server.wm.jetpack.wrapper.TestWindowLayoutInfo;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.window.extensions.ExtensionDeviceState;
import androidx.window.extensions.ExtensionInterface;
import androidx.window.extensions.ExtensionWindowLayoutInfo;

/** Compatibility wrapper for extension versions v1.0+. */
public final class TestExtensionCompat implements TestInterfaceCompat {

    @Nullable
    public static TestExtensionCompat create(@Nullable ExtensionInterface extensionInterface) {
        return extensionInterface == null ? null : new TestExtensionCompat(extensionInterface);
    }

    @NonNull
    private final ExtensionInterface mExtensionInterface;

    private TestExtensionCompat(@NonNull ExtensionInterface extensionInterface) {
        mExtensionInterface = extensionInterface;
    }

    @Override
    public void setExtensionCallback(@NonNull TestInterfaceCallback callback) {
        mExtensionInterface.setExtensionCallback(new ExtensionInterface.ExtensionCallback() {
            @Override
            public void onDeviceStateChanged(@NonNull ExtensionDeviceState newDeviceState) {
                callback.onDeviceStateChanged(TestExtensionDeviceState.create(newDeviceState));
            }

            @Override
            public void onWindowLayoutChanged(@NonNull IBinder windowToken,
                    @NonNull ExtensionWindowLayoutInfo newLayout) {
                callback.onWindowLayoutChanged(
                        windowToken,
                        TestExtensionWindowLayoutInfo.create(newLayout));
            }
        });
    }

    @Override
    public TestWindowLayoutInfo getWindowLayoutInfo(@NonNull IBinder windowToken) {
        return TestExtensionWindowLayoutInfo.create(
                mExtensionInterface.getWindowLayoutInfo(windowToken));
    }

    @Override
    public void onWindowLayoutChangeListenerAdded(@NonNull IBinder windowToken) {
        mExtensionInterface.onWindowLayoutChangeListenerAdded(windowToken);
    }

    @Override
    public void onWindowLayoutChangeListenerRemoved(@NonNull IBinder windowToken) {
        mExtensionInterface.onWindowLayoutChangeListenerRemoved(windowToken);
    }

    @Override
    public TestDeviceState getDeviceState() {
        return TestExtensionDeviceState.create(mExtensionInterface.getDeviceState());
    }

    @Override
    public void onDeviceStateListenersChanged(boolean isEmpty) {
        mExtensionInterface.onDeviceStateListenersChanged(isEmpty);
    }
}
