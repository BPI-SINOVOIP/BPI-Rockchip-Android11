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
package com.android.car.rotary;

import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;
import java.util.Stack;

/** Cache of window type and most recently focused node for each window ID. */
class WindowCache {
    /** Window IDs. */
    private final Stack<Integer> mWindowIds;
    /** Window types keyed by window IDs. */
    private final Map<Integer, Integer> mWindowTypes;
    /** Most recent focused nodes keyed by window IDs. */
    private final Map<Integer, AccessibilityNodeInfo> mFocusedNodes;

    @NonNull
    private NodeCopier mNodeCopier = new NodeCopier();

    WindowCache() {
        mWindowIds = new Stack<>();
        mWindowTypes = new HashMap<>();
        mFocusedNodes = new HashMap<>();
    }

    /**
     * Saves the focused node of the given window, removing the existing entry, if any. This method
     * should be called when the focused node changed.
     */
    void saveFocusedNode(int windowId, @NonNull AccessibilityNodeInfo focusedNode) {
        if (mFocusedNodes.containsKey(windowId)) {
            // Call remove(Integer) to remove.
            AccessibilityNodeInfo oldNode = mFocusedNodes.remove(windowId);
            oldNode.recycle();
        }
        mFocusedNodes.put(windowId, copyNode(focusedNode));
    }

    /**
     * Saves the type of the given window, removing the existing entry, if any. This method should
     * be called when a window was just added.
     */
    void saveWindowType(int windowId, int windowType) {
        Integer id = windowId;
        if (mWindowIds.contains(id)) {
            // Call remove(Integer) to remove.
            mWindowIds.remove(id);
        }
        mWindowIds.push(id);

        mWindowTypes.put(windowId, windowType);
    }

    /**
     * Removes an entry if it exists. This method should be called when a window was just removed.
     */
    void remove(int windowId) {
        Integer id = windowId;
        if (mWindowIds.contains(id)) {
            // Call remove(Integer) to remove.
            mWindowIds.remove(id);
            mWindowTypes.remove(id);
            AccessibilityNodeInfo node = mFocusedNodes.remove(id);
            Utils.recycleNode(node);
        }
    }

    /** Gets the window type keyed by {@code windowId}, or null if none. */
    @Nullable
    Integer getWindowType(int windowId) {
        return mWindowTypes.get(windowId);
    }

    /**
     * Returns a copy of the most recently focused node in the most recently added window, or null
     * if none.
     */
    @Nullable
    AccessibilityNodeInfo getMostRecentFocusedNode() {
        if (mWindowIds.isEmpty()) {
            return null;
        }
        Integer recentWindowId = mWindowIds.peek();
        if (recentWindowId == null) {
            return null;
        }
        return copyNode(mFocusedNodes.get(recentWindowId));
    }

    /** Sets a mock NodeCopier instance for testing. */
    @VisibleForTesting
    void setNodeCopier(@NonNull NodeCopier nodeCopier) {
        mNodeCopier = nodeCopier;
    }

    private AccessibilityNodeInfo copyNode(@Nullable AccessibilityNodeInfo node) {
        return mNodeCopier.copy(node);
    }

    void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        writer.println("  windowIds: " + mWindowIds);
        writer.println("  windowTypes:");
        for (Map.Entry<Integer, Integer> entry : mWindowTypes.entrySet()) {
            writer.println("    windowId: " + entry.getKey()
                    + ", type: " + AccessibilityWindowInfo.typeToString(entry.getValue()));
        }
        writer.println("  focusedNodes:");
        for (Map.Entry<Integer, AccessibilityNodeInfo> entry : mFocusedNodes.entrySet()) {
            writer.println("    windowId: " + entry.getKey() + ", node: " + entry.getValue());
        }
    }
}
