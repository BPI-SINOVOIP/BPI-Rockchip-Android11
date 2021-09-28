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

package com.android.car.telephony.common;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;
import androidx.lifecycle.LiveData;

import com.android.car.apps.common.log.L;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;

/**
 * Asynchronously queries a {@link ContentResolver} for a given query and observes the loaded data
 * for changes, reloading if necessary.
 *
 * @param <T> The type of data held by this instance
 */
public abstract class AsyncQueryLiveData<T> extends LiveData<T> {

    private static final String TAG = "CD.AsyncQueryLiveData";
    private final ExecutorService mExecutorService;

    private final ObservableAsyncQuery mObservableAsyncQuery;
    private CursorRunnable mCurrentCursorRunnable;
    private Future<?> mCurrentRunnableFuture;

    public AsyncQueryLiveData(Context context, QueryParam.Provider provider) {
        this(context, provider, WorkerExecutor.getInstance().getSingleThreadExecutor());
    }

    public AsyncQueryLiveData(Context context, QueryParam.Provider provider,
            ExecutorService executorService) {
        mObservableAsyncQuery = new ObservableAsyncQuery(context, provider, this::onCursorLoaded);
        mExecutorService = executorService;
    }

    @Override
    protected void onActive() {
        super.onActive();
        mObservableAsyncQuery.startQuery();
    }

    @Override
    protected void onInactive() {
        super.onInactive();
        if (mCurrentCursorRunnable != null) {
            mCurrentCursorRunnable.closeCursorIfNecessary();
            mCurrentRunnableFuture.cancel(false);
        }
        mObservableAsyncQuery.stopQuery();
    }

    /**
     * Override this function to convert the loaded data. This function is called on non-UI thread.
     */
    @WorkerThread
    protected abstract T convertToEntity(@NonNull Cursor cursor);

    private void onCursorLoaded(Cursor cursor) {
        L.d(TAG, "onCursorLoaded: " + this);
        if (mCurrentCursorRunnable != null) {
            mCurrentCursorRunnable.closeCursorIfNecessary();
            mCurrentRunnableFuture.cancel(false);
        }
        mCurrentCursorRunnable = new CursorRunnable(cursor);
        mCurrentRunnableFuture = mExecutorService.submit(mCurrentCursorRunnable);
    }

    private class CursorRunnable implements Runnable {
        private final Cursor mCursor;
        private boolean mIsActive;

        private CursorRunnable(@Nullable Cursor cursor) {
            mCursor = cursor;
            mIsActive = true;
        }

        @Override
        public void run() {
            // Bypass the workload to convert to entity and UI change triggered by post value if
            // cursor is not current.
            if (mIsActive) {
                T entity = mCursor == null ? null : convertToEntity(mCursor);
                if (mIsActive) {
                    postValue(entity);
                }
            }
            closeCursorIfNecessary();
        }

        public synchronized void closeCursorIfNecessary() {
            if (!mIsActive && mCursor != null) {
                mCursor.close();
            }
            mIsActive = false;
        }
    }
}
