/*
 * Copyright (C) 2021 The Android Open Source Project
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

import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_FORWARD;
import static android.view.accessibility.AccessibilityNodeInfo.FOCUS_INPUT;

import static com.android.car.rotary.Utils.FOCUS_AREA_CLASS_NAME;
import static com.android.car.rotary.Utils.FOCUS_PARKING_VIEW_CLASS_NAME;
import static com.android.car.rotary.Utils.GENERIC_FOCUS_PARKING_VIEW_CLASS_NAME;
import static com.android.car.ui.utils.RotaryConstants.FOCUS_AREA_BOTTOM_BOUND_OFFSET;
import static com.android.car.ui.utils.RotaryConstants.FOCUS_AREA_LEFT_BOUND_OFFSET;
import static com.android.car.ui.utils.RotaryConstants.FOCUS_AREA_RIGHT_BOUND_OFFSET;
import static com.android.car.ui.utils.RotaryConstants.FOCUS_AREA_TOP_BOUND_OFFSET;
import static com.android.car.ui.utils.RotaryConstants.ROTARY_CONTAINER;
import static com.android.car.ui.utils.RotaryConstants.ROTARY_VERTICALLY_SCROLLABLE;

import static com.google.common.truth.Truth.assertThat;

import android.graphics.Rect;
import android.os.Bundle;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;

@RunWith(AndroidJUnit4.class)
public class NodeBuilderTest {
    private static final String PACKAGE_NAME = "package_name";
    private static final String CLASS_NAME = "class_name";
    private static final String CONTENT_DESCRIPTION = "content_description";
    private static final String STATE_DESCRIPTION = "state_description";
    private NodeBuilder mNodeBuilder;

    @Before
    public void setUp() {
        mNodeBuilder = new NodeBuilder(new ArrayList<>());
    }

    @Test
    public void testBuildDefaultNode() {
        AccessibilityNodeInfo node = mNodeBuilder.build();
        assertThat(node.isFocusable()).isTrue();
        assertThat(node.isFocused()).isFalse();
        assertThat(node.isVisibleToUser()).isTrue();
        assertThat(node.refresh()).isTrue();
        assertThat(node.isEnabled()).isTrue();
        assertThat(node.isScrollable()).isFalse();
        Rect boundsInParent = new Rect();
        node.getBoundsInParent(boundsInParent);
        assertThat(boundsInParent.isEmpty()).isFalse();
        Rect boundsInScreen = new Rect();
        node.getBoundsInScreen(boundsInScreen);
        assertThat(boundsInScreen).isEqualTo(NodeBuilder.DEFAULT_BOUNDS);
        assertThat(node.getBoundsInScreen()).isEqualTo(NodeBuilder.DEFAULT_BOUNDS);
    }

    @Test
    public void testSetFocusable() {
        AccessibilityNodeInfo node = mNodeBuilder.setFocusable(false).build();
        assertThat(node.isFocusable()).isFalse();
    }

    @Test
    public void testSetFocused() {
        AccessibilityNodeInfo node = mNodeBuilder.setFocused(true).build();
        assertThat(node.isFocused()).isTrue();
    }

    @Test
    public void testSetVisibleToUser() {
        AccessibilityNodeInfo node = mNodeBuilder.setVisibleToUser(false).build();
        assertThat(node.isVisibleToUser()).isFalse();
    }

    @Test
    public void testSetInViewTree() {
        AccessibilityNodeInfo node = mNodeBuilder.setInViewTree(false).build();
        assertThat(node.refresh()).isFalse();
    }

    @Test
    public void testSetScrollable() {
        AccessibilityNodeInfo node = mNodeBuilder.setScrollable(true).build();
        assertThat(node.isScrollable()).isTrue();
    }

    @Test
    public void testSetEnabled() {
        AccessibilityNodeInfo node = mNodeBuilder.setEnabled(false).build();
        assertThat(node.isEnabled()).isFalse();
    }

    @Test
    public void testSetWindow() {
        AccessibilityWindowInfo window = new WindowBuilder().build();
        AccessibilityNodeInfo node = mNodeBuilder.setWindow(window).build();
        assertThat(node.getWindow()).isEqualTo(window);
    }

    @Test
    public void testSetBoundsInParent() {
        Rect setBounds = new Rect(100, 200, 300, 400);
        AccessibilityNodeInfo node = mNodeBuilder.setBoundsInParent(setBounds).build();
        Rect retrievedBounds = new Rect();
        node.getBoundsInParent(retrievedBounds);
        assertThat(retrievedBounds).isEqualTo(setBounds);
    }

    @Test
    public void testSetBoundsInScreen() {
        Rect setBounds = new Rect(100, 200, 300, 400);
        AccessibilityNodeInfo node = mNodeBuilder.setBoundsInScreen(setBounds).build();
        Rect retrievedBounds = new Rect();
        node.getBoundsInScreen(retrievedBounds);
        assertThat(retrievedBounds).isEqualTo(setBounds);
    }

    @Test
    public void testSetPackageName() {
        AccessibilityNodeInfo node = mNodeBuilder.setPackageName(PACKAGE_NAME).build();
        assertThat(node.getPackageName().toString()).isEqualTo(PACKAGE_NAME);
    }

    @Test
    public void testSetClassName() {
        AccessibilityNodeInfo node = mNodeBuilder.setClassName(CLASS_NAME).build();
        assertThat(node.getClassName().toString()).isEqualTo(CLASS_NAME);
    }

    @Test
    public void testSetContentDescription() {
        AccessibilityNodeInfo node =
                mNodeBuilder.setContentDescription(CONTENT_DESCRIPTION).build();
        assertThat(node.getContentDescription().toString()).isEqualTo(CONTENT_DESCRIPTION);
    }

    @Test
    public void testSetStateDescription() {
        AccessibilityNodeInfo node =
                mNodeBuilder.setStateDescription(STATE_DESCRIPTION).build();
        assertThat(node.getStateDescription().toString()).isEqualTo(STATE_DESCRIPTION);
    }

    @Test
    public void testSetParent() {
        AccessibilityNodeInfo parent = mNodeBuilder.build();
        AccessibilityNodeInfo child1 = mNodeBuilder.setParent(parent).build();
        AccessibilityNodeInfo child2 = mNodeBuilder.setParent(parent).build();
        assertThat(child1.getParent()).isEqualTo(parent);
        assertThat(parent.getChildCount()).isEqualTo(2);
        assertThat(parent.getChild(0)).isEqualTo(child1);
        assertThat(parent.getChild(1)).isEqualTo(child2);
        assertThat(parent.getChild(2)).isNull();
    }

    @Test
    public void testFindInputFocus_succeeded() {
        AccessibilityNodeInfo root = mNodeBuilder.build();
        AccessibilityNodeInfo parent1 = mNodeBuilder.setParent(root).build();
        AccessibilityNodeInfo parent2 = mNodeBuilder.setParent(root).build();
        AccessibilityNodeInfo child1 = mNodeBuilder.setParent(parent1).build();
        AccessibilityNodeInfo child2 = mNodeBuilder.setParent(parent1).build();
        AccessibilityNodeInfo child3 = mNodeBuilder.setParent(parent2).setFocused(true).build();
        AccessibilityNodeInfo child4 = mNodeBuilder.setParent(parent2).build();

        assertThat(root.findFocus(FOCUS_INPUT)).isEqualTo(child3);
    }

    @Test
    public void testFindInputFocus_failed() {
        AccessibilityNodeInfo parent = mNodeBuilder.build();
        AccessibilityNodeInfo child = mNodeBuilder.setParent(parent).build();
        assertThat(parent.findFocus(FOCUS_INPUT)).isNull();
    }

    @Test
    public void testSetActions() {
        AccessibilityNodeInfo node = mNodeBuilder.setActions(ACTION_SCROLL_FORWARD).build();
        assertThat(node.getActionList()).containsExactly(ACTION_SCROLL_FORWARD);
    }

    @Test
    public void testSetFocusArea() {
        AccessibilityNodeInfo node = mNodeBuilder.setFocusArea().build();
        assertThat(node.getClassName().toString()).isEqualTo(FOCUS_AREA_CLASS_NAME);
        assertThat(node.isFocusable()).isFalse();
    }

    @Test
    public void testSetFocusAreaBoundsOffset() {
        int left = 10;
        int top = 20;
        int right = 30;
        int bottom = 40;
        AccessibilityNodeInfo node = mNodeBuilder
                .setFocusAreaBoundsOffset(left, top, right, bottom)
                .build();
        Bundle extras = node.getExtras();
        assertThat(extras.getInt(FOCUS_AREA_LEFT_BOUND_OFFSET)).isEqualTo(left);
        assertThat(extras.getInt(FOCUS_AREA_TOP_BOUND_OFFSET)).isEqualTo(top);
        assertThat(extras.getInt(FOCUS_AREA_RIGHT_BOUND_OFFSET)).isEqualTo(right);
        assertThat(extras.getInt(FOCUS_AREA_BOTTOM_BOUND_OFFSET)).isEqualTo(bottom);
    }

    @Test
    public void testSetFpv() {
        AccessibilityNodeInfo node = mNodeBuilder.setFpv().build();
        assertThat(node.getClassName().toString()).isEqualTo(FOCUS_PARKING_VIEW_CLASS_NAME);
    }

    @Test
    public void testSetGenericFpv() {
        AccessibilityNodeInfo node = mNodeBuilder.setGenericFpv().build();
        assertThat(node.getClassName().toString()).isEqualTo(GENERIC_FOCUS_PARKING_VIEW_CLASS_NAME);
    }

    @Test
    public void testSetScrollableContainer() {
        AccessibilityNodeInfo node = mNodeBuilder.setScrollableContainer().build();
        assertThat(node.getContentDescription().toString()).isEqualTo(ROTARY_VERTICALLY_SCROLLABLE);
    }

    @Test
    public void testSetRotaryContainer() {
        AccessibilityNodeInfo node = mNodeBuilder.setRotaryContainer().build();
        assertThat(node.getContentDescription().toString()).isEqualTo(ROTARY_CONTAINER);
    }
}
