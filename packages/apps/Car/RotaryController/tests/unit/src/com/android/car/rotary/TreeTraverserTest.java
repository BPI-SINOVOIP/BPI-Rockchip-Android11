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

import static com.google.common.truth.Truth.assertThat;

import android.app.UiAutomation;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class TreeTraverserTest {

    private static UiAutomation sUiAutomoation;

    @Rule
    public ActivityTestRule<TreeTraverserTestActivity> mActivityRule =
            new ActivityTestRule<>(TreeTraverserTestActivity.class);

    private TreeTraverser mTreeTraverser;

    private AccessibilityNodeInfo mNode0;
    private AccessibilityNodeInfo mNode1;
    private AccessibilityNodeInfo mNode2;
    private AccessibilityNodeInfo mNode3;
    private AccessibilityNodeInfo mNode4;
    private AccessibilityNodeInfo mNode5;
    private AccessibilityNodeInfo mNode6;

    @BeforeClass
    public static void oneTimeSetup() {
        sUiAutomoation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
    }

    @Before
    public void setUp() {
        mTreeTraverser = new TreeTraverser();

        AccessibilityNodeInfo root = sUiAutomoation.getRootInActiveWindow();

        mNode0 = root.findAccessibilityNodeInfosByViewId(
                "com.android.car.rotary.tests.unit:id/node0").get(0);
        mNode1 = root.findAccessibilityNodeInfosByViewId(
                "com.android.car.rotary.tests.unit:id/node1").get(0);
        mNode2 = root.findAccessibilityNodeInfosByViewId(
                "com.android.car.rotary.tests.unit:id/node2").get(0);
        mNode3 = root.findAccessibilityNodeInfosByViewId(
                "com.android.car.rotary.tests.unit:id/node3").get(0);
        mNode4 = root.findAccessibilityNodeInfosByViewId(
                "com.android.car.rotary.tests.unit:id/node4").get(0);
        mNode5 = root.findAccessibilityNodeInfosByViewId(
                "com.android.car.rotary.tests.unit:id/node5").get(0);
        mNode6 = root.findAccessibilityNodeInfosByViewId(
                "com.android.car.rotary.tests.unit:id/node6").get(0);

        root.recycle();
    }

    @After
    public void tearDown() {
        Utils.recycleNodes(mNode0, mNode1, mNode2, mNode3, mNode4, mNode5, mNode6);
    }

    /**
     * Tests
     * {@link TreeTraverser#findNodeOrAncestor} in the following node tree:
     * <pre>
     *                   node0
     *                  /     \
     *                /         \
     *           node1           node4
     *           /   \           /   \
     *         /       \       /       \
     *      node2    node3   node5    node6
     * </pre>
     */
    @Test
    public void testFindNodeOrAncestor() {
        // Should check the node itself.
        AccessibilityNodeInfo result = mTreeTraverser.findNodeOrAncestor(mNode0,
                /* stopPredicate= */ null, /* targetPredicate= */ node -> node.equals(mNode0));
        assertThat(result).isEqualTo(mNode0);
        Utils.recycleNode(result);

        // Parent.
        result = mTreeTraverser.findNodeOrAncestor(mNode1, /* stopPredicate= */ null,
                /* targetPredicate= */ node -> node.equals(mNode0));
        assertThat(result).isEqualTo(mNode0);
        Utils.recycleNode(result);

        // Grandparent.
        result = mTreeTraverser.findNodeOrAncestor(mNode2, /* stopPredicate= */ null,
                /* targetPredicate= */ node -> node.equals(mNode0));
        assertThat(result).isEqualTo(mNode0);
        Utils.recycleNode(result);

        // No ancestor found.
        result = mTreeTraverser.findNodeOrAncestor(mNode2, /* stopPredicate= */ null,
                /* targetPredicate= */ node -> node.equals(mNode6));
        assertThat(result).isNull();
        Utils.recycleNode(result);

        // Stop before target.
        result = mTreeTraverser.findNodeOrAncestor(mNode2, /* stopPredicate= */
                node -> node.equals(mNode1),
                /* targetPredicate= */ node -> node.equals(mNode0));
        assertThat(result).isNull();
        Utils.recycleNode(result);

        // Stop at target.
        result = mTreeTraverser.findNodeOrAncestor(mNode2, /* stopPredicate= */
                node -> node.equals(mNode0),
                /* targetPredicate= */ node -> node.equals(mNode0));
        assertThat(result).isNull();
        Utils.recycleNode(result);
    }

    /**
     * Tests {@link TreeTraverser#depthFirstSearch} in the following node tree:
     * <pre>
     *                   node0
     *                  /     \
     *                /         \
     *           node1           node4
     *           /   \           /   \
     *         /       \       /       \
     *      node2    node3   node5    node6
     * </pre>
     */
    @Test
    public void testDepthFirstSearch() {
        // Iterate in depth-first order, finding nothing.
        List<AccessibilityNodeInfo> targetPredicateCalledWithNodes = new ArrayList<>();
        AccessibilityNodeInfo result = mTreeTraverser.depthFirstSearch(
                mNode0,
                /* skipPredicate= */ null,
                node -> {
                    targetPredicateCalledWithNodes.add(new AccessibilityNodeInfo(node));
                    return false;
                });
        assertThat(result).isNull();
        assertThat(targetPredicateCalledWithNodes).containsExactly(mNode0, mNode1, mNode2,
                mNode3, mNode4, mNode5, mNode6);
        Utils.recycleNode(result);

        // Find root.
        result = mTreeTraverser.depthFirstSearch(mNode0, /* skipPredicate= */ null,
                /* targetPredicate= */ node -> node.equals(mNode0));
        assertThat(result).isEqualTo(mNode0);
        Utils.recycleNode(result);

        // Find child.
        result = mTreeTraverser.depthFirstSearch(mNode0, /* skipPredicate= */ null,
                /* targetPredicate= */ node -> node.equals(mNode4));
        assertThat(result).isEqualTo(mNode4);
        Utils.recycleNode(result);

        // Find grandchild.
        result = mTreeTraverser.depthFirstSearch(mNode0, /* skipPredicate= */ null,
                /* targetPredicate= */ node -> node.equals(mNode6));
        assertThat(result).isEqualTo(mNode6);
        Utils.recycleNode(result);

        // Iterate in depth-first order, skipping a subtree containing the target
        List<AccessibilityNodeInfo> skipPredicateCalledWithNodes = new ArrayList<>();
        targetPredicateCalledWithNodes.clear();
        result = mTreeTraverser.depthFirstSearch(mNode0,
                node -> {
                    skipPredicateCalledWithNodes.add(new AccessibilityNodeInfo(node));
                    return node.equals(mNode1);
                },
                node -> {
                    targetPredicateCalledWithNodes.add(new AccessibilityNodeInfo(node));
                    return node.equals(mNode2);
                });
        assertThat(result).isNull();
        assertThat(skipPredicateCalledWithNodes).containsExactly(mNode0, mNode1, mNode4, mNode5,
                mNode6);
        assertThat(targetPredicateCalledWithNodes).containsExactly(mNode0, mNode4, mNode5, mNode6);
        Utils.recycleNode(result);

        // Skip subtree whose root is the target.
        result = mTreeTraverser.depthFirstSearch(mNode0,
                /* skipPredicate= */ node -> node.equals(mNode1),
                /* skipPredicate= */ node -> node.equals(mNode1));
        assertThat(result).isNull();
        Utils.recycleNode(result);
    }

    /**
     * Tests {@link TreeTraverser#reverseDepthFirstSearch} in the following node tree:
     * <pre>
     *                   node0
     *                  /     \
     *                /         \
     *           node1           node4
     *           /   \           /   \
     *         /       \       /       \
     *      node2    node3   node5    node6
     * </pre>
     */
    @Test
    public void testReverseDepthFirstSearch() {
        // Iterate in reverse depth-first order, finding nothing.
        List<AccessibilityNodeInfo> predicateCalledWithNodes = new ArrayList<>();
        AccessibilityNodeInfo result = mTreeTraverser.reverseDepthFirstSearch(
                mNode0,
                node -> {
                    predicateCalledWithNodes.add(new AccessibilityNodeInfo(node));
                    return false;
                });
        assertThat(result).isNull();
        assertThat(predicateCalledWithNodes).containsExactly(
                mNode6, mNode5, mNode4, mNode3, mNode2, mNode1, mNode0);
        Utils.recycleNode(result);

        // Find root.
        result = mTreeTraverser.reverseDepthFirstSearch(mNode0, node -> node.equals(mNode0));
        assertThat(result).isEqualTo(mNode0);
        Utils.recycleNode(result);

        // Find child.
        result = mTreeTraverser.reverseDepthFirstSearch(mNode0, node -> node.equals(mNode1));
        assertThat(result).isEqualTo(mNode1);
        Utils.recycleNode(result);

        // Find grandchild.
        result = mTreeTraverser.reverseDepthFirstSearch(mNode0, node -> node.equals(mNode2));
        assertThat(result).isEqualTo(mNode2);
        Utils.recycleNode(result);
    }

    /**
     * Tests {@link TreeTraverser#depthFirstSelect} in the following node tree:
     * <pre>
     *                   node0
     *                  /     \
     *                /         \
     *           node1           node4
     *           /   \           /   \
     *         /       \       /       \
     *      node2    node3   node5    node6
     * </pre>
     */
    @Test
    public void testDepthFirstSelect() {
        // Iterate in depth-first order, selecting no nodes.
        List<AccessibilityNodeInfo> predicateCalledWithNodes = new ArrayList<>();
        List<AccessibilityNodeInfo> selectedNodes = new ArrayList<>();
        mTreeTraverser.depthFirstSelect(mNode0, node -> {
            predicateCalledWithNodes.add(new AccessibilityNodeInfo(node));
            return false;
        }, selectedNodes);
        assertThat(predicateCalledWithNodes).containsExactly(
                mNode0, mNode1, mNode2, mNode3, mNode4, mNode5, mNode6);
        assertThat(selectedNodes).isEmpty();
        Utils.recycleNodes(selectedNodes);

        // Find any node. Selects root and skips descendants.
        predicateCalledWithNodes.clear();
        selectedNodes = new ArrayList<>();
        mTreeTraverser.depthFirstSelect(mNode0, node -> {
            predicateCalledWithNodes.add(new AccessibilityNodeInfo(node));
            return true;
        }, selectedNodes);
        assertThat(predicateCalledWithNodes).containsExactly(mNode0);
        assertThat(selectedNodes).containsExactly(mNode0);
        Utils.recycleNodes(selectedNodes);

        // Find children of root node. Skips grandchildren.
        predicateCalledWithNodes.clear();
        selectedNodes = new ArrayList<>();
        mTreeTraverser.depthFirstSelect(mNode0, node -> {
            predicateCalledWithNodes.add(new AccessibilityNodeInfo(node));
            return node.equals(mNode1) || node.equals(mNode4);
        }, selectedNodes);
        assertThat(predicateCalledWithNodes).containsExactly(mNode0, mNode1, mNode4);
        assertThat(selectedNodes).containsExactly(mNode1, mNode4);
        Utils.recycleNodes(selectedNodes);

        // Find grandchildren of root node.
        selectedNodes = new ArrayList<>();
        mTreeTraverser.depthFirstSelect(mNode0,
                node -> node.equals(mNode2) || node.equals(mNode3) || node.equals(mNode5)
                        || node.equals(mNode6),
                selectedNodes);
        assertThat(selectedNodes).containsExactly(mNode2, mNode3, mNode5, mNode6);
        Utils.recycleNodes(selectedNodes);
    }
}
