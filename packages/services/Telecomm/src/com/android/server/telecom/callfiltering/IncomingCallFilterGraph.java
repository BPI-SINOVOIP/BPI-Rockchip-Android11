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

package com.android.server.telecom.callfiltering;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.telecom.Log;
import android.telecom.Logging.Runnable;

import com.android.server.telecom.Call;
import com.android.server.telecom.LoggedHandlerExecutor;
import com.android.server.telecom.LogUtils;
import com.android.server.telecom.TelecomSystem;
import com.android.server.telecom.Timeouts;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;

public class IncomingCallFilterGraph {
    //TODO: Add logging for control flow.
    public static final String TAG = "IncomingCallFilterGraph";
    public static final CallFilteringResult DEFAULT_RESULT =
            new CallFilteringResult.Builder()
                    .setShouldAllowCall(true)
                    .setShouldReject(false)
                    .setShouldAddToCallLog(true)
                    .setShouldShowNotification(true)
                    .build();

    private final CallFilterResultCallback mListener;
    private final Call mCall;
    private final Handler mHandler;
    private final HandlerThread mHandlerThread;
    private final TelecomSystem.SyncRoot mLock;
    private List<CallFilter> mFiltersList;
    private CallFilter mDummyComplete;
    private boolean mFinished;
    private CallFilteringResult mCurrentResult;
    private Context mContext;
    private Timeouts.Adapter mTimeoutsAdapter;

    private class PostFilterTask {
        private final CallFilter mFilter;

        public PostFilterTask(final CallFilter filter) {
            mFilter = filter;
        }

        public CallFilteringResult whenDone(CallFilteringResult result) {
            Log.i(TAG, "Filter %s done, result: %s.", mFilter, result);
            mFilter.result = result;
            for (CallFilter filter : mFilter.getFollowings()) {
                if (filter.decrementAndGetIndegree() == 0) {
                    scheduleFilter(filter);
                }
            }
            if (mFilter.equals(mDummyComplete)) {
                synchronized (mLock) {
                    mFinished = true;
                    mListener.onCallFilteringComplete(mCall, result);
                    Log.addEvent(mCall, LogUtils.Events.FILTERING_COMPLETED, result);
                }
                mHandlerThread.quit();
            }
            return result;
        }
    }

    public IncomingCallFilterGraph(Call call, CallFilterResultCallback listener, Context context,
            Timeouts.Adapter timeoutsAdapter, TelecomSystem.SyncRoot lock) {
        mListener = listener;
        mCall = call;
        mFiltersList = new ArrayList<>();

        mHandlerThread = new HandlerThread(TAG);
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
        mLock = lock;
        mFinished = false;
        mContext = context;
        mTimeoutsAdapter = timeoutsAdapter;
        mCurrentResult = DEFAULT_RESULT;
    }

    public void addFilter(CallFilter filter) {
        mFiltersList.add(filter);
    }

    public void performFiltering() {
        Log.addEvent(mCall, LogUtils.Events.FILTERING_INITIATED);
        CallFilter dummyStart = new CallFilter();
        mDummyComplete = new CallFilter();

        for (CallFilter filter : mFiltersList) {
            addEdge(dummyStart, filter);
        }
        for (CallFilter filter : mFiltersList) {
            addEdge(filter, mDummyComplete);
        }
        addEdge(dummyStart, mDummyComplete);

        scheduleFilter(dummyStart);
        mHandler.postDelayed(new Runnable("ICFG.pF", mLock) {
            @Override
            public void loggedRun() {
                if (!mFinished) {
                    Log.i(this, "Graph timed out when performing filtering.");
                    Log.addEvent(mCall, LogUtils.Events.FILTERING_TIMED_OUT);
                    mListener.onCallFilteringComplete(mCall, mCurrentResult);
                    mFinished = true;
                    mHandlerThread.quit();
                }
                for (CallFilter filter : mFiltersList) {
                    // unbind timed out call screening service
                    if (filter instanceof CallScreeningServiceFilter) {
                        ((CallScreeningServiceFilter) filter).unbindCallScreeningService();
                    }
                }
            }
        }.prepare(), mTimeoutsAdapter.getCallScreeningTimeoutMillis(mContext.getContentResolver()));
    }

    private void scheduleFilter(CallFilter filter) {
        CallFilteringResult result = new CallFilteringResult.Builder()
                .setShouldAllowCall(true)
                .setShouldReject(false)
                .setShouldSilence(false)
                .setShouldAddToCallLog(true)
                .setShouldShowNotification(true)
                .build();
        for (CallFilter dependencyFilter : filter.getDependencies()) {
            result = result.combine(dependencyFilter.getResult());
        }
        mCurrentResult = result;
        final CallFilteringResult input = result;

        CompletableFuture<CallFilteringResult> startFuture =
                CompletableFuture.completedFuture(input);
        PostFilterTask postFilterTask = new PostFilterTask(filter);

        // TODO: improve these filter logging names to be more reflective of the filters that are
        // executing
        startFuture.thenComposeAsync(filter::startFilterLookup,
                new LoggedHandlerExecutor(mHandler, "ICFG.sF", null))
                .thenApplyAsync(postFilterTask::whenDone,
                        new LoggedHandlerExecutor(mHandler, "ICFG.sF", null));
        Log.i(TAG, "Filter %s scheduled.", filter);
    }

    public static void addEdge(CallFilter before, CallFilter after) {
        before.addFollowings(after);
        after.addDependency(before);
    }

    public HandlerThread getHandlerThread() {
        return mHandlerThread;
    }
}
