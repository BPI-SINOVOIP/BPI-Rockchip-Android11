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

import android.graphics.Rect;
import android.server.wm.jetpack.wrapper.TestDisplayFeature;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.window.extensions.ExtensionDisplayFeature;

/** Compatibility wrapper for extension versions v1.0+. */
final class TestExtensionDisplayFeature implements TestDisplayFeature {

    @Nullable
    static TestExtensionDisplayFeature create(
            @Nullable ExtensionDisplayFeature extensionDisplayFeature) {
        return extensionDisplayFeature == null
                ? null
                : new TestExtensionDisplayFeature(extensionDisplayFeature);
    }

    private final ExtensionDisplayFeature mExtensionDisplayFeature;

    private TestExtensionDisplayFeature(@NonNull ExtensionDisplayFeature extensionDisplayFeature) {
        mExtensionDisplayFeature = extensionDisplayFeature;
    }

    @Override
    public Rect getBounds() {
        return mExtensionDisplayFeature.getBounds();
    }

    @Override
    public int getType() {
        return mExtensionDisplayFeature.getType();
    }

    @NonNull
    @Override
    public String toString() {
        return mExtensionDisplayFeature.toString();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof TestExtensionDisplayFeature)) {
            return false;
        }
        final TestExtensionDisplayFeature other = (TestExtensionDisplayFeature) obj;
        return mExtensionDisplayFeature.equals(other.mExtensionDisplayFeature);
    }

    @Override
    public int hashCode() {
        return mExtensionDisplayFeature.hashCode();
    }
}
