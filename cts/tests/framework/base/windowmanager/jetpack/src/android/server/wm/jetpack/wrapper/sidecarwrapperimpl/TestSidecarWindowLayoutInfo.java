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


import android.server.wm.jetpack.wrapper.TestDisplayFeature;
import android.server.wm.jetpack.wrapper.TestWindowLayoutInfo;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.window.sidecar.SidecarDisplayFeature;
import androidx.window.sidecar.SidecarWindowLayoutInfo;

import java.util.List;
import java.util.stream.Collectors;

/** Extension interface compatibility wrapper for v0.1 sidecar. */
@SuppressWarnings("deprecation")
final class TestSidecarWindowLayoutInfo implements TestWindowLayoutInfo {

    @Nullable
    static TestSidecarWindowLayoutInfo create(
            @Nullable SidecarWindowLayoutInfo sidecarWindowLayoutInfo) {
        return sidecarWindowLayoutInfo == null
                ? null
                : new TestSidecarWindowLayoutInfo(sidecarWindowLayoutInfo);
    }

    private final SidecarWindowLayoutInfo mSidecarWindowLayoutInfo;

    private TestSidecarWindowLayoutInfo(SidecarWindowLayoutInfo sidecarWindowLayoutInfo) {
        mSidecarWindowLayoutInfo = sidecarWindowLayoutInfo;
    }

    @Nullable
    @Override
    public List<TestDisplayFeature> getDisplayFeatures() {
        List<SidecarDisplayFeature> displayFeatures = mSidecarWindowLayoutInfo.displayFeatures;
        return displayFeatures == null
                ? null
                : displayFeatures
                        .stream()
                        .map(TestSidecarDisplayFeature::create)
                        .collect(Collectors.toList());
    }

    @NonNull
    @Override
    public String toString() {
        return mSidecarWindowLayoutInfo.toString();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof TestSidecarWindowLayoutInfo)) {
            return false;
        }
        final TestSidecarWindowLayoutInfo other = (TestSidecarWindowLayoutInfo) obj;
        return mSidecarWindowLayoutInfo.equals(other.mSidecarWindowLayoutInfo);
    }

    @Override
    public int hashCode() {
        return mSidecarWindowLayoutInfo.hashCode();
    }
}
