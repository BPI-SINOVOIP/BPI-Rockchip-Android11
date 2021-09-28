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

package com.android.documentsui;

import static com.google.common.truth.Truth.assertThat;

import androidx.recyclerview.selection.SelectionTracker;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;

@SmallTest
public class ProfileTabsControllerTest {

    private SelectionTracker<String> mSelectionMgr = SelectionHelpers.createTestInstance();

    private FakeProfileTabs mFakeProfileTabs = new FakeProfileTabs();
    private ProfileTabsController mController = new ProfileTabsController(mSelectionMgr,
            enabled -> mFakeProfileTabs.enabled = enabled);

    @Before
    public void setUp() throws Exception {
        mSelectionMgr.addObserver(mController);
    }

    @Test
    public void testNoSelection_enable() {
        mController.onSelectionRestored();
        assertThat(mFakeProfileTabs.isEnabled()).isTrue();
    }

    @Test
    public void testClearSelection_enable() {
        mSelectionMgr.select("foo");
        mSelectionMgr.clearSelection();
        assertThat(mFakeProfileTabs.isEnabled()).isTrue();
    }

    @Test
    public void testDeselectAll_enable() {
        mSelectionMgr.select("foo");
        mSelectionMgr.select("bar");
        mSelectionMgr.deselect("foo");
        mSelectionMgr.deselect("bar");
        assertThat(mFakeProfileTabs.isEnabled()).isTrue();
    }

    @Test
    public void testSelection_disable() {
        mSelectionMgr.select("foo");
        assertThat(mFakeProfileTabs.isEnabled()).isFalse();
    }

    @Test
    public void testMultipleSelection_disable() {
        mSelectionMgr.select("foo");
        mSelectionMgr.select("bar");
        mSelectionMgr.select("apple");
        assertThat(mFakeProfileTabs.isEnabled()).isFalse();
    }

    @Test
    public void testDeselectSome_Disable() {
        mSelectionMgr.select("foo");
        mSelectionMgr.select("bar");
        mSelectionMgr.deselect("bar");
        assertThat(mFakeProfileTabs.isEnabled()).isFalse();
    }

    private static class FakeProfileTabs {
        Boolean enabled = null;

        boolean isEnabled() {
            if (enabled == null) {
                throw new IllegalArgumentException("Not initialized");
            }
            return enabled;
        }
    }
}
