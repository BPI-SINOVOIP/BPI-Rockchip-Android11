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

import android.server.wm.jetpack.wrapper.TestDeviceState;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.window.extensions.ExtensionDeviceState;

import com.google.common.base.Preconditions;

/** Compatibility wrapper for extension versions v1.0+. */
final class TestExtensionDeviceState implements TestDeviceState {

    @Nullable
    static TestExtensionDeviceState create(@Nullable ExtensionDeviceState extensionDeviceState) {
        return extensionDeviceState == null
                ? null
                : new TestExtensionDeviceState(extensionDeviceState);
    }

    private final ExtensionDeviceState mExtensionDeviceState;

    private TestExtensionDeviceState(@NonNull ExtensionDeviceState extensionDeviceState) {
        mExtensionDeviceState = Preconditions.checkNotNull(extensionDeviceState);
    }

    @Override
    public int getPosture() {
        return mExtensionDeviceState.getPosture();
    }

    @NonNull
    @Override
    public String toString() {
        return mExtensionDeviceState.toString();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof TestExtensionDeviceState)) {
            return false;
        }
        final TestExtensionDeviceState other = (TestExtensionDeviceState) obj;
        return mExtensionDeviceState.equals(other.mExtensionDeviceState);
    }

    @Override
    public int hashCode() {
        return mExtensionDeviceState.hashCode();
    }
}
