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

import android.content.AsyncQueryHandler;
import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.database.Cursor;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import com.android.car.apps.common.log.L;

/**
 * Asynchronously queries data and observes them. A new query will be triggered automatically if
 * data set have changed.
 */
public class ObservableAsyncQuery {
    private static final String TAG = "CD.ObservableAsyncQuery";

    /**
     * Called when query is finished.
     */
    public interface OnQueryFinishedListener {
        /**
         * Called when the query is finished loading. This callbacks will also be called if data
         * changed.
         *
         * <p>Called on main thread.
         */
        @MainThread
        void onQueryFinished(@Nullable Cursor cursor);
    }

    private Context mContext;
    private AsyncQueryHandler mAsyncQueryHandler;
    private QueryParam.Provider mQueryParamProvider;
    private OnQueryFinishedListener mOnQueryFinishedListener;
    private ContentObserver mContentObserver;
    private ContentResolver mContentResolver;
    private boolean mIsActive = false;
    private int mToken;

    /**
     * @param queryParamProvider Supplies query arguments for the current query.
     * @param listener           Listener which will be called when data is available.
     */
    public ObservableAsyncQuery(
            @NonNull Context context,
            @NonNull QueryParam.Provider queryParamProvider,
            @NonNull OnQueryFinishedListener listener) {
        mContext = context;
        mContentResolver = context.getContentResolver();
        mAsyncQueryHandler = new AsyncQueryHandlerImpl(this, mContentResolver);
        mContentObserver = new ContentObserver(mAsyncQueryHandler) {
            @Override
            public void onChange(boolean selfChange) {
                startQuery();
            }
        };
        mQueryParamProvider = queryParamProvider;
        mOnQueryFinishedListener = listener;
        mToken = 0;
    }

    /**
     * Starts the query and stops any pending query.
     */
    @MainThread
    public void startQuery() {
        L.d(TAG, "startQuery");
        mAsyncQueryHandler.cancelOperation(mToken); // Cancel the query task.
        mContentResolver.unregisterContentObserver(mContentObserver);

        mToken++;
        QueryParam queryParam = mQueryParamProvider.getQueryParam();
        if (queryParam != null && ContextCompat.checkSelfPermission(mContext,
                queryParam.mPermission) == PackageManager.PERMISSION_GRANTED) {
            mAsyncQueryHandler.startQuery(
                    mToken,
                    null,
                    queryParam.mUri,
                    queryParam.mProjection,
                    queryParam.mSelection,
                    queryParam.mSelectionArgs,
                    queryParam.mOrderBy);
            mContentResolver.registerContentObserver(queryParam.mUri, false, mContentObserver);
        } else {
            mOnQueryFinishedListener.onQueryFinished(null);
        }
        mIsActive = true;

    }

    /**
     * Stops any pending query and also stops listening on the data set change.
     */
    @MainThread
    public void stopQuery() {
        L.d(TAG, "stopQuery");
        mIsActive = false;
        mContentResolver.unregisterContentObserver(mContentObserver);
        mAsyncQueryHandler.cancelOperation(mToken); // Cancel the query task.
    }

    private void onQueryComplete(int token, Object cookie, Cursor cursor) {
        if (!mIsActive) {
            return;
        }
        L.d(TAG, "onQueryComplete");
        if (mOnQueryFinishedListener != null) {
            mOnQueryFinishedListener.onQueryFinished(cursor);
        }
    }

    private static class AsyncQueryHandlerImpl extends AsyncQueryHandler {
        private ObservableAsyncQuery mQuery;

        AsyncQueryHandlerImpl(ObservableAsyncQuery query, ContentResolver cr) {
            super(cr);
            mQuery = query;
        }

        @Override
        protected void onQueryComplete(int token, Object cookie, Cursor cursor) {
            if (token == mQuery.mToken) {
                // The query result is the most up to date.
                mQuery.onQueryComplete(token, cookie, cursor);
            } else {
                // Query canceled, close the cursor directly.
                cursor.close();
            }
        }
    }
}
