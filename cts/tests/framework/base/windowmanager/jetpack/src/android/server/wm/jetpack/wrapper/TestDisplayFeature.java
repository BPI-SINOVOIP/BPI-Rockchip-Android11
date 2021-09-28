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

import android.graphics.Rect;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Test interface for {@link androidx.window.extensions.ExtensionDisplayFeature} and
 * {@link androidx.window.sidecar.SidecarDisplayFeature} that serves as an API compatibility
 * wrapper.
 *
 * @see android.server.wm.jetpack.wrapper.extensionwrapperimpl.TestExtensionDisplayFeature
 * @see android.server.wm.jetpack.wrapper.sidecarwrapperimpl.TestSidecarDisplayFeature
 */
public interface TestDisplayFeature {

    /**
     * A fold in the flexible screen without a physical gap.
     */
    public static final int TYPE_FOLD = 1;

    /**
     * A physical separation with a hinge that allows two display panels to fold.
     */
    public static final int TYPE_HINGE = 2;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
            TYPE_FOLD,
            TYPE_HINGE,
    })
    @interface Type{}

    /** Gets the bounding rect of the display feature in window coordinate space. */
    Rect getBounds();

    /** Gets the type of the display feature. */
    int getType();
}
