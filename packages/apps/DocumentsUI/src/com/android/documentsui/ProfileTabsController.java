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

import static androidx.core.util.Preconditions.checkNotNull;

import androidx.recyclerview.selection.SelectionTracker;
import androidx.recyclerview.selection.SelectionTracker.SelectionObserver;

/**
 * A controller that listens to selection changes and controls profile tabs behavior.
 */
public class ProfileTabsController extends SelectionObserver<String> {

    private static final String TAG = "ProfileTabsController";

    private final SelectionTracker<String> mSelectionMgr;
    private final ProfileTabsAddons mProfileTabsAddons;

    public ProfileTabsController(
            SelectionTracker<String> selectionMgr,
            ProfileTabsAddons profileTabsAddons) {
        mSelectionMgr = checkNotNull(selectionMgr);
        mProfileTabsAddons = checkNotNull(profileTabsAddons);
    }

    @Override
    public void onSelectionChanged() {
        onSelectionUpdated();
    }

    @Override
    public void onSelectionRestored() {
        onSelectionUpdated();
    }

    private void onSelectionUpdated() {
        mProfileTabsAddons.setEnabled(mSelectionMgr.getSelection().isEmpty());
    }
}
