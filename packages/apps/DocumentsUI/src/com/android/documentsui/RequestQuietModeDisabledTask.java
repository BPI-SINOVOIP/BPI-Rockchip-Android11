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

import android.content.Context;
import android.os.AsyncTask;

import com.android.documentsui.base.UserId;

import java.lang.ref.WeakReference;

/**
 * A task to request disabling quiet mode for a given user.
 */
class RequestQuietModeDisabledTask extends AsyncTask<Void, Void, Void> {

    private final WeakReference<Context> mContextWeakReference;
    private final UserId mUserId;

    RequestQuietModeDisabledTask(Context context, UserId userId) {
        mContextWeakReference = new WeakReference<>(checkNotNull(context));
        mUserId = checkNotNull(userId);
    }

    @Override
    protected Void doInBackground(Void... voids) {
        Context context = mContextWeakReference.get();
        if (context != null) {
            mUserId.requestQuietModeDisabled(context);
        }
        return null;
    }
}
