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

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.accessibility.AccessibilityNodeInfo;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class UtilsTest {

    @Test
    public void refreshNode_nodeIsNull_returnsNull() {
        AccessibilityNodeInfo result = Utils.refreshNode(null);

        assertThat(result).isNull();
    }

    @Test
    public void refreshNode_nodeRefreshed_returnsNode() {
        AccessibilityNodeInfo input = mock(AccessibilityNodeInfo.class);
        when(input.refresh()).thenReturn(true);

        AccessibilityNodeInfo result = Utils.refreshNode(input);

        assertThat(result).isNotNull();
    }

    @Test
    public void refreshNode_nodeNotRefreshed_returnsNull() {
        AccessibilityNodeInfo input = mock(AccessibilityNodeInfo.class);
        when(input.refresh()).thenReturn(false);

        AccessibilityNodeInfo result = Utils.refreshNode(input);

        assertThat(result).isNull();
    }

    @Test
    public void refreshNode_nodeNotRefreshed_recycleNode() {
        AccessibilityNodeInfo input = mock(AccessibilityNodeInfo.class);
        when(input.refresh()).thenReturn(false);

        Utils.refreshNode(input);

        verify(input).recycle();
    }
}
