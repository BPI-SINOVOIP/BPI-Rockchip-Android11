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
package com.android.wallpaper.testing;

import android.content.Intent;
import android.net.Uri;

import com.android.wallpaper.module.ExploreIntentChecker;

/**
 * Test implementation of ExploreIntentChecker.
 */
public class TestExploreIntentChecker implements ExploreIntentChecker {

    private boolean mViewHandlerExists;

    public TestExploreIntentChecker() {
        // True by default.
        mViewHandlerExists = true;
    }

    @Override
    public void fetchValidActionViewIntent(Uri uri, IntentReceiver receiver) {
        Intent intent = mViewHandlerExists ? new Intent(Intent.ACTION_VIEW, uri) : null;
        receiver.onIntentReceived(intent);
    }

    public void setViewHandlerExists(boolean exists) {
        mViewHandlerExists = exists;
    }
}
