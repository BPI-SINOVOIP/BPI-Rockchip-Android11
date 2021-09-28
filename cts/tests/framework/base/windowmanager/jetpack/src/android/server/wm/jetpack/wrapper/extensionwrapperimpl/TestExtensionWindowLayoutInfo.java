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


import android.server.wm.jetpack.wrapper.TestDisplayFeature;
import android.server.wm.jetpack.wrapper.TestWindowLayoutInfo;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.window.extensions.ExtensionDisplayFeature;
import androidx.window.extensions.ExtensionWindowLayoutInfo;

import java.util.List;
import java.util.stream.Collectors;

/** Compatibility wrapper for extension versions v1.0+. */
final class TestExtensionWindowLayoutInfo implements TestWindowLayoutInfo {

    @Nullable
    static TestExtensionWindowLayoutInfo create(
            @Nullable ExtensionWindowLayoutInfo extensionWindowLayoutInfo) {
        return extensionWindowLayoutInfo == null
                ? null
                : new TestExtensionWindowLayoutInfo(extensionWindowLayoutInfo);
    }

    private final ExtensionWindowLayoutInfo mExtensionWindowLayoutInfo;

    private TestExtensionWindowLayoutInfo(ExtensionWindowLayoutInfo extensionWindowLayoutInfo) {
        mExtensionWindowLayoutInfo = extensionWindowLayoutInfo;
    }

    @Nullable
    @Override
    public List<TestDisplayFeature> getDisplayFeatures() {
        List<ExtensionDisplayFeature> displayFeatures =
                mExtensionWindowLayoutInfo.getDisplayFeatures();
        return displayFeatures == null
                ? null
                : mExtensionWindowLayoutInfo.getDisplayFeatures()
                        .stream()
                        .map(TestExtensionDisplayFeature::create)
                        .collect(Collectors.toList());
    }

    @NonNull
    @Override
    public String toString() {
        return mExtensionWindowLayoutInfo.toString();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof TestExtensionWindowLayoutInfo)) {
            return false;
        }
        final TestExtensionWindowLayoutInfo other = (TestExtensionWindowLayoutInfo) obj;
        return mExtensionWindowLayoutInfo.equals(other.mExtensionWindowLayoutInfo);
    }

    @Override
    public int hashCode() {
        return mExtensionWindowLayoutInfo.hashCode();
    }
}
