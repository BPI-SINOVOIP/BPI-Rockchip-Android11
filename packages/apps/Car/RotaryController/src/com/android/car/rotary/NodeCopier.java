/*
 * Copyright 2020 The Android Open Source Project
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

package com.android.car.rotary;

import android.view.accessibility.AccessibilityNodeInfo;

import androidx.annotation.Nullable;

/**
 * A class to copy {@link AccessibilityNodeInfo}.
 * <p>
 * {@link AccessibilityNodeInfo#obtain(AccessibilityNodeInfo)} } doesn't work when passed a mock
 * node. To mock it in unit tests, we create a non-static method {@link
 * #copy(AccessibilityNodeInfo)} to wrap it.
 */
class NodeCopier {

    /** Copies a node. The caller is responsible for recycling result. */
    AccessibilityNodeInfo copy(@Nullable AccessibilityNodeInfo node) {
        return node == null ? null : AccessibilityNodeInfo.obtain(node);
    }
}
