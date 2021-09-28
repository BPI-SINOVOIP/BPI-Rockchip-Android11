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
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.util.Log;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.slice.Slice;
import androidx.slice.SliceViewManager;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * In TvSettings, if a preference item with corresponding slice first get focused, and regain focus
 * later, we always receive a cache of the slice data first and then we get the real data from
 * SliceProvider. If the cache and the real data is not consistent, user would clearly see the
 * process of the cache data transforming to the real one. e.g, toggle state switching from false
 * to true.
 *
 * This is because when a Lifecycle starts again, LiveData natively notify us about the change,
 * which will trigger onChanged() in SliceFragment.
 *
 * To avoid this issue, we should ignore the lifecycle event, use SingleLiveEvent so we only get
 * notified when the data is actually changed.
 */
@RequiresApi(19)
public final class PreferenceSliceLiveData {
    private static final String TAG = "SliceLiveData";

    /**
     * Produces a {@link LiveData} that tracks a Slice for a given Uri. To use
     * this method your app must have the permission to the slice Uri.
     */
    static @NonNull SliceLiveDataImpl fromUri(@NonNull Context context, @NonNull Uri uri) {
        return new SliceLiveDataImpl(context.getApplicationContext(), uri);
    }

    /**
     * LiveData for slice used by TvSettings.
     */
    static class SliceLiveDataImpl extends MutableLiveData<Slice> {
        final Intent mIntent;
        final SliceViewManager mSliceViewManager;
        Uri mUri;
        final AtomicBoolean mUpdatePending = new AtomicBoolean(false);
        SliceLiveDataImpl(Context context, Uri uri) {
            super();
            mSliceViewManager = SliceViewManager.getInstance(context);
            mUri = uri;
            mIntent = null;
            // TODO: Check if uri points at a Slice?
        }

        @Override
        protected void onActive() {
            AsyncTask.execute(mUpdateSlice);
            if (mUri != null) {
                mSliceViewManager.registerSliceCallback(mUri, mSliceCallback);
            }
        }

        @Override
        protected void onInactive() {
            if (mUri != null) {
                mSliceViewManager.unregisterSliceCallback(mUri, mSliceCallback);
            }
        }

        @Override
        @MainThread
        public void setValue(Slice slice) {
            mUpdatePending.set(true);
            super.setValue(slice);
        }

        private final Runnable mUpdateSlice = new Runnable() {
            @Override
            public void run() {
                try {
                    Slice s = mUri != null ? mSliceViewManager.bindSlice(mUri)
                            : mSliceViewManager.bindSlice(mIntent);
                    if (mUri == null && s != null) {
                        mUri = s.getUri();
                        mSliceViewManager.registerSliceCallback(mUri, mSliceCallback);
                    }
                    postValue(s);
                } catch (Exception e) {
                    Log.e(TAG, "Error binding slice", e);
                    postValue(null);
                }
            }
        };

        final SliceViewManager.SliceCallback mSliceCallback =
                new SliceViewManager.SliceCallback() {
                    @Override
                    public void onSliceUpdated(@NonNull Slice s) {
                        postValue(s);
                    }
                };
    }

    private PreferenceSliceLiveData() {
    }
}
