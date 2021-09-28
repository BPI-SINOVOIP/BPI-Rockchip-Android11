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

package com.android.documentsui;

import static com.android.documentsui.base.SharedMinimal.DEBUG;
import static com.android.documentsui.base.SharedMinimal.TAG;

import android.database.ContentObserver;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

/**
 * A custom {@link ContentObserver} which constructed by a {@link ContentLock}
 * and a {@link Runnable} callback. It will callback when it's onChange and ContentLock is unlock.
 */
public final class LockingContentObserver extends ContentObserver {
    private final ContentLock mLock;
    private final Runnable mContentChangedCallback;

    public LockingContentObserver(ContentLock lock, Runnable contentChangedCallback) {
        super(new Handler(Looper.getMainLooper()));
        mLock = lock;
        mContentChangedCallback = contentChangedCallback;
    }

    @Override
    public boolean deliverSelfNotifications() {
        return true;
    }

    @Override
    public void onChange(boolean selfChange) {
        if (DEBUG) {
            Log.d(TAG, "Content updated.");
        }
        mLock.runWhenUnlocked(mContentChangedCallback);
    }
}
