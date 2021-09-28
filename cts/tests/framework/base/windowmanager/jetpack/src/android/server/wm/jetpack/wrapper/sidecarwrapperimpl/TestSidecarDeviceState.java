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

import android.server.wm.jetpack.wrapper.TestDeviceState;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.window.sidecar.SidecarDeviceState;

/** Extension interface compatibility wrapper for v0.1 sidecar. */
@SuppressWarnings("deprecation")
public final class TestSidecarDeviceState implements TestDeviceState {

    @Nullable
    static TestSidecarDeviceState create(@Nullable SidecarDeviceState sidecarDeviceState) {
        return sidecarDeviceState == null
                ? null
                : new TestSidecarDeviceState(sidecarDeviceState);
    }

    private final SidecarDeviceState mSidecarDeviceState;

    private TestSidecarDeviceState(@NonNull SidecarDeviceState sidecarDeviceState) {
        mSidecarDeviceState = sidecarDeviceState;
    }

    @Override
    public int getPosture() {
        return mSidecarDeviceState.posture;
    }

    @NonNull
    @Override
    public String toString() {
        return mSidecarDeviceState.toString();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof TestSidecarDeviceState)) {
            return false;
        }
        final TestSidecarDeviceState other = (TestSidecarDeviceState) obj;
        return mSidecarDeviceState.equals(other.mSidecarDeviceState);
    }

    @Override
    public int hashCode() {
        return mSidecarDeviceState.hashCode();
    }
}
