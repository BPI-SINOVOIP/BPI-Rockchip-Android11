/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.ide.common.resources.deprecated;

/**
 * @deprecated This class is part of an obsolete resource repository system that is no longer used
 *     in production code. The class is preserved temporarily for LayoutLib tests.
 */
@Deprecated
public class ScanningContext {
    private boolean mNeedsFullAapt;

    /**
     * Marks that a full aapt compilation of the resources is necessary because it has
     * detected a change that cannot be incrementally handled.
     */
    protected void requestFullAapt() {
        mNeedsFullAapt = true;
    }

    /**
     * Returns whether this repository has been marked as "dirty"; if one or
     * more of the constituent files have declared that the resource item names
     * that they provide have changed.
     *
     * @return true if a full aapt compilation is required
     */
    public boolean needsFullAapt() {
        return mNeedsFullAapt;
    }
}
