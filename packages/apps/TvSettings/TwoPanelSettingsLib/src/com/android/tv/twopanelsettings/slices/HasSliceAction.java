/*
 * Copyright 2019 The Android Open Source Project
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

import androidx.slice.core.SliceActionImpl;

/**
 * Indicates the preference has the slice action to be triggered.
 */
public interface HasSliceAction {
    /** Return the Action ID to be digested for logging. */
    int getActionId();
    /** Set the Action ID to be digested for logging. */
    void setActionId(int actionId);
    SliceActionImpl getSliceAction();
    void setSliceAction(SliceActionImpl sliceAction);
    SliceActionImpl getFollowupSliceAction();
    void setFollowupSliceAction(SliceActionImpl sliceAction);
}
