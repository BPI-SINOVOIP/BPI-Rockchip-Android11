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

import android.graphics.Rect;
import android.server.wm.jetpack.wrapper.TestDisplayFeature;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.window.sidecar.SidecarDisplayFeature;

/** Extension interface compatibility wrapper for v0.1 sidecar. */
@SuppressWarnings("deprecation")
final class TestSidecarDisplayFeature implements TestDisplayFeature {

    @Nullable
    static TestSidecarDisplayFeature create(@Nullable SidecarDisplayFeature sidecarDisplayFeature) {
        return sidecarDisplayFeature == null
                ? null
                : new TestSidecarDisplayFeature(sidecarDisplayFeature);
    }

    private final SidecarDisplayFeature mSidecarDisplayFeature;

    private TestSidecarDisplayFeature(@NonNull SidecarDisplayFeature sidecarDisplayFeature) {
        mSidecarDisplayFeature = sidecarDisplayFeature;
    }

    @Override
    public Rect getBounds() {
        return mSidecarDisplayFeature.getRect();
    }

    @Override
    public int getType() {
        return mSidecarDisplayFeature.getType();
    }

    @NonNull
    @Override
    public String toString() {
        return mSidecarDisplayFeature.toString();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof TestSidecarDisplayFeature)) {
            return false;
        }
        final TestSidecarDisplayFeature other = (TestSidecarDisplayFeature) obj;
        return mSidecarDisplayFeature.equals(other.mSidecarDisplayFeature);
    }

    @Override
    public int hashCode() {
        return mSidecarDisplayFeature.hashCode();
    }
}
