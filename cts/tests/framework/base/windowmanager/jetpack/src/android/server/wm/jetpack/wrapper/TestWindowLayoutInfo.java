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

import androidx.annotation.Nullable;

import java.util.List;

/**
 * Test interface for {@link androidx.window.extensions.ExtensionWindowLayoutInfo} and
 * {@link androidx.window.sidecar.SidecarWindowLayoutInfo} that serves as an API compatibility
 * wrapper.
 *
 * @see android.server.wm.jetpack.wrapper.extensionwrapperimpl.TestExtensionWindowLayoutInfo
 * @see android.server.wm.jetpack.wrapper.sidecarwrapperimpl.TestSidecarWindowLayoutInfo
 */
public interface TestWindowLayoutInfo {
    /** Gets the list of display features present within the window. */
    @Nullable
    List<TestDisplayFeature> getDisplayFeatures();
}
