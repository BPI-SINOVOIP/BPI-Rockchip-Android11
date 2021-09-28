/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.tv.common.flags.impl;

import com.android.tv.common.flags.UiFlags;

/** Default Flags for TV app UI */
public class DefaultUiFlags implements UiFlags {

    @Override
    public boolean compiled() {
        return true;
    }

    @Override
    public boolean enableCriticRatings() {
        return false;
    }

    @Override
    public long maxHistoryDays() {
        return 0;
    }

    @Override
    public String moreChannelsUrl() {
        return "";
    }

    @Override
    public boolean unhideLauncher() {
        return false;
    }
}
