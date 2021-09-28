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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import java.util.List;
import java.util.function.Predicate;

/**
 * Utility methods for traversing {@link AccessibilityNodeInfo} trees.
 */
class TreeTraverser {

    @NonNull
    private NodeCopier mNodeCopier = new NodeCopier();

    /**
     * Iterates starting at {@code node} and then progressing through its ancestors, looking for a
     * node that satisfies {@code targetPredicate}. Returns the first such node (or a copy if it's
     * the {@code node} itself), or null if none. The caller is responsible for recycling the
     * result.
     */
    @Nullable
    AccessibilityNodeInfo findNodeOrAncestor(@NonNull AccessibilityNodeInfo node,
            @NonNull Predicate<AccessibilityNodeInfo> targetPredicate) {
        return findNodeOrAncestor(node, /* stopPredicate= */ null, targetPredicate);
    }

    /**
     * Iterates starting at {@code node} and then progressing through its ancestors, looking for a
     * node that satisfies {@code targetPredicate}. Returns the first such node (or a copy if it's
     * the {@code node} itself), or null if no such nodes are encountered before reaching the root
     * or a node which satisfies {@code stopPredicate}. The caller is responsible for recycling the
     * result.
     */
    @VisibleForTesting
    @Nullable
    AccessibilityNodeInfo findNodeOrAncestor(@NonNull AccessibilityNodeInfo node,
            @Nullable Predicate<AccessibilityNodeInfo> stopPredicate,
            @NonNull Predicate<AccessibilityNodeInfo> targetPredicate) {
        AccessibilityNodeInfo currentNode = copyNode(node);
        while (currentNode != null
                && (stopPredicate == null || !stopPredicate.test(currentNode))) {
            if (targetPredicate.test(currentNode)) {
                return currentNode;
            }
            AccessibilityNodeInfo parentNode = currentNode.getParent();
            currentNode.recycle();
            currentNode = parentNode;
        }
        Utils.recycleNode(currentNode);
        return null;
    }

    /**
     * Searches {@code node} and its descendants in depth-first order and returns the first node
     * satisfying {@code targetPredicate}, if any, or returns null if not found. The caller is
     * responsible for recycling the result.
     */
    @Nullable
    AccessibilityNodeInfo depthFirstSearch(@NonNull AccessibilityNodeInfo node,
            @NonNull Predicate<AccessibilityNodeInfo> targetPredicate) {
        return depthFirstSearch(node, /* skipPredicate= */ null, targetPredicate);
    }

    /**
     * Searches {@code node} and its descendants in depth-first order, skipping any node that
     * satisfies {@code skipPredicate} as well as its descendants, and returns the first node
     * satisfying {@code targetPredicate}, if any, or returns null if not found. If
     * {@code skipPredicate} is null, no nodes are skipped. The caller is responsible for recycling
     * the result.
     */
    @Nullable
    @VisibleForTesting
    AccessibilityNodeInfo depthFirstSearch(@NonNull AccessibilityNodeInfo node,
            @Nullable Predicate<AccessibilityNodeInfo> skipPredicate,
            @NonNull Predicate<AccessibilityNodeInfo> targetPredicate) {
        if (skipPredicate != null && skipPredicate.test(node)) {
            return null;
        }
        if (targetPredicate.test(node)) {
            return copyNode(node);
        }
        for (int i = 0; i < node.getChildCount(); i++) {
            AccessibilityNodeInfo child = node.getChild(i);
            if (child == null) {
                continue;
            }
            AccessibilityNodeInfo result = depthFirstSearch(child, skipPredicate, targetPredicate);
            child.recycle();
            if (result != null) {
                return result;
            }
        }
        return null;
    }

    /**
     * Searches {@code node} and its descendants in reverse depth-first order, and returns the first
     * node satisfying {@code targetPredicate}, if any, or returns null if not found. The caller is
     * responsible for recycling the result.
     */
    @VisibleForTesting
    @Nullable
    AccessibilityNodeInfo reverseDepthFirstSearch(@NonNull AccessibilityNodeInfo node,
            @NonNull Predicate<AccessibilityNodeInfo> targetPredicate) {
        for (int i = node.getChildCount() - 1; i >= 0; i--) {
            AccessibilityNodeInfo child = node.getChild(i);
            if (child == null) {
                continue;
            }
            AccessibilityNodeInfo result =
                    reverseDepthFirstSearch(child, targetPredicate);
            child.recycle();
            if (result != null) {
                return result;
            }
        }
        if (targetPredicate.test(node)) {
            return copyNode(node);
        }
        return null;
    }

    /**
     * Iterates through {@code node} and its descendants in depth-first order, adding nodes which
     * satisfy {@code selectPredicate} to {@code selectedNodes}. Descendants of these nodes aren't
     * checked. The caller is responsible for recycling the added nodes.
     */
    @VisibleForTesting
    void depthFirstSelect(@NonNull AccessibilityNodeInfo node,
            @NonNull Predicate<AccessibilityNodeInfo> selectPredicate,
            @NonNull List<AccessibilityNodeInfo> selectedNodes) {
        if (selectPredicate.test(node)) {
            selectedNodes.add(copyNode(node));
            return;
        }
        for (int i = 0; i < node.getChildCount(); i++) {
            AccessibilityNodeInfo child = node.getChild(i);
            if (child == null) {
                continue;
            }
            depthFirstSelect(child, selectPredicate, selectedNodes);
            child.recycle();
        }
    }

    /** Sets a NodeCopier instance for testing. */
    @VisibleForTesting
    void setNodeCopier(@NonNull NodeCopier nodeCopier) {
        mNodeCopier = nodeCopier;
    }

    private AccessibilityNodeInfo copyNode(@Nullable AccessibilityNodeInfo node) {
        return mNodeCopier.copy(node);
    }
}
