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

package com.android.tv.twopanelsettings.slices;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.CheckBoxPreference;
import androidx.slice.core.SliceActionImpl;

/**
 * Slice version of CheckboxPreference.
 */
public class SliceCheckboxPreference extends CheckBoxPreference implements HasSliceAction {
    private int mActionId;
    private SliceActionImpl mAction;
    private SliceActionImpl mFollowupSliceAction;

    public SliceCheckboxPreference(Context context, SliceActionImpl action) {
        super(context);
        mAction = action;
        update();
    }

    public SliceCheckboxPreference(Context context, AttributeSet attrs, SliceActionImpl action) {
        super(context, attrs);
        mAction = action;
        update();
    }

    @Override
    public int getActionId() {
        return mActionId;
    }

    @Override
    public void setActionId(int actionId) {
        mActionId = actionId;
    }

    @Override
    public SliceActionImpl getSliceAction() {
        return mAction;
    }

    @Override
    public void setSliceAction(SliceActionImpl sliceAction) {
        mAction = sliceAction;
    }

    @Override
    public SliceActionImpl getFollowupSliceAction() {
        return mFollowupSliceAction;
    }

    @Override
    public void setFollowupSliceAction(SliceActionImpl sliceAction) {
        mFollowupSliceAction = sliceAction;
    }

    private void update() {
        this.setChecked(mAction.isChecked());
    }
}
