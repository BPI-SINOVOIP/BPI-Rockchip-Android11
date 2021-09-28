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

package android.server.wm.jetpack.wrapper.sidecarwrapperimpl;

import android.os.IBinder;
import android.server.wm.jetpack.wrapper.TestDeviceState;
import android.server.wm.jetpack.wrapper.TestInterfaceCompat;
import android.server.wm.jetpack.wrapper.TestWindowLayoutInfo;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.window.sidecar.SidecarDeviceState;
import androidx.window.sidecar.SidecarInterface;
import androidx.window.sidecar.SidecarWindowLayoutInfo;

/** Extension interface compatibility wrapper for v0.1 sidecar. */
@SuppressWarnings("deprecation")
public final class TestSidecarCompat implements TestInterfaceCompat {

    @Nullable
    public static TestSidecarCompat create(@Nullable SidecarInterface sidecarInterface) {
        return sidecarInterface == null ? null : new TestSidecarCompat(sidecarInterface);
    }

    @NonNull
    private final SidecarInterface mSidecarInterface;

    private TestSidecarCompat(@NonNull SidecarInterface sidecarInterface) {
        mSidecarInterface = sidecarInterface;
    }

    @Override
    public void setExtensionCallback(@NonNull TestInterfaceCallback callback) {
        mSidecarInterface.setSidecarCallback(new SidecarInterface.SidecarCallback() {
            @Override
            public void onDeviceStateChanged(@NonNull SidecarDeviceState newDeviceState) {
                callback.onDeviceStateChanged(TestSidecarDeviceState.create(newDeviceState));
            }

            @Override
            public void onWindowLayoutChanged(@NonNull IBinder windowToken,
                    @NonNull SidecarWindowLayoutInfo newLayout) {
                callback.onWindowLayoutChanged(
                        windowToken,
                        TestSidecarWindowLayoutInfo.create(newLayout));
            }
        });
    }

    @Override
    public TestWindowLayoutInfo getWindowLayoutInfo(@NonNull IBinder windowToken) {
        return TestSidecarWindowLayoutInfo.create(
                mSidecarInterface.getWindowLayoutInfo(windowToken));
    }

    @Override
    public void onWindowLayoutChangeListenerAdded(@NonNull IBinder windowToken) {
        mSidecarInterface.onWindowLayoutChangeListenerAdded(windowToken);
    }

    @Override
    public void onWindowLayoutChangeListenerRemoved(@NonNull IBinder windowToken) {
        mSidecarInterface.onWindowLayoutChangeListenerRemoved(windowToken);
    }

    @Override
    public TestDeviceState getDeviceState() {
        return TestSidecarDeviceState.create(mSidecarInterface.getDeviceState());
    }

    @Override
    public void onDeviceStateListenersChanged(boolean isEmpty) {
        mSidecarInterface.onDeviceStateListenersChanged(isEmpty);
    }
}
