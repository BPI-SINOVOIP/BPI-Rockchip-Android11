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
package android.apppredictionservice.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.prediction.AppPredictionContext;
import android.app.prediction.AppPredictionSessionId;
import android.app.prediction.AppPredictor;
import android.app.prediction.AppTarget;
import android.app.prediction.AppTargetEvent;
import android.app.prediction.AppTargetId;
import android.os.Binder;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/**
 * Reports calls from the CTS prediction service back to the tests.
 */
public class ServiceReporter extends Binder {

    public HashMap<AppPredictionSessionId, AppPredictionContext> mSessions = new HashMap<>();

    public ArrayList<AppTargetEvent> mEvents = new ArrayList<>();
    public String mLocationsShown;
    public ArrayList<AppTargetId> mLocationsShownTargets = new ArrayList<>();
    public int mNumRequestedUpdates = 0;
    public boolean mPredictionUpdatesStarted = false;

    private CountDownLatch mCreateSessionLatch = new CountDownLatch(1);
    private CountDownLatch mEventLatch = new CountDownLatch(1);
    private CountDownLatch mLocationShownLatch = new CountDownLatch(1);
    private CountDownLatch mSortLatch = new CountDownLatch(1);
    private CountDownLatch mStartPredictionUpdatesLatch = new CountDownLatch(1);
    private CountDownLatch mStopPredictionUpdatesLatch = new CountDownLatch(1);
    private CountDownLatch mPredictionUpdateLatch = new CountDownLatch(1);
    private CountDownLatch mDestroyLatch = new CountDownLatch(1);

    private PredictionsProvider mPredictionsProvider;
    private SortedPredictionsProvider mSortedPredictionsProvider;

    void setPredictionsProvider(PredictionsProvider cb) {
        mPredictionsProvider = cb;
    }

    PredictionsProvider getPredictionsProvider() {
        return mPredictionsProvider;
    }

    void setSortedPredictionsProvider(SortedPredictionsProvider cb) {
        mSortedPredictionsProvider = cb;
    }

    SortedPredictionsProvider getSortedPredictionsProvider() {
        return mSortedPredictionsProvider;
    }

    void assertActiveSession(AppPredictionSessionId sessionId) {
        assertTrue(mSessions.containsKey(sessionId));
    }

    AppPredictionContext getPredictionContext(AppPredictionSessionId sessionId) {
        assertTrue(mSessions.containsKey(sessionId));
        return mSessions.get(sessionId);
    }

    void onCreatePredictionSession(AppPredictionContext context,
            AppPredictionSessionId sessionId) {
        assertNotNull(context);
        assertNotNull(sessionId);
        assertFalse(mSessions.containsKey(sessionId));
        mSessions.put(sessionId, context);
        mCreateSessionLatch.countDown();
    }

    boolean awaitOnCreatePredictionSession() {
        try {
            return await(mCreateSessionLatch);
        } finally {
            mCreateSessionLatch = new CountDownLatch(1);
        }
    }

    void onAppTargetEvent(AppPredictionSessionId sessionId, AppTargetEvent event) {
        assertTrue(mSessions.containsKey(sessionId));
        mEvents.add(event);
        mEventLatch.countDown();
    }

    boolean awaitOnAppTargetEvent() {
        try {
            return await(mEventLatch);
        } finally {
            mEventLatch = new CountDownLatch(1);
        }
    }

    void onLocationShown(AppPredictionSessionId sessionId, String launchLocation,
            List<AppTargetId> targetIds) {
        assertTrue(mSessions.containsKey(sessionId));
        mLocationsShown = launchLocation;
        mLocationsShownTargets.addAll(targetIds);
        mLocationShownLatch.countDown();
    }

    boolean awaitOnLocationShown() {
        try {
            return await(mLocationShownLatch);
        } finally {
            mLocationShownLatch = new CountDownLatch(1);
        }
    }

    void onSortAppTargets(AppPredictionSessionId sessionId, List<AppTarget> targets,
            Consumer<List<AppTarget>> callback) {
        assertTrue(mSessions.containsKey(sessionId));
        assertNotNull(targets);
        assertNotNull(callback);
        mSortLatch.countDown();
    }

    boolean awaitOnSortAppTargets() {
        try {
            return await(mSortLatch);
        } finally {
            mSortLatch = new CountDownLatch(1);
        }
    }

    void onStartPredictionUpdates() {
        mPredictionUpdatesStarted = true;
    }

    boolean awaitOnStartPredictionUpdates() {
        try {
            return await(mStartPredictionUpdatesLatch);
        } finally {
            mStartPredictionUpdatesLatch = new CountDownLatch(1);
        }
    }

    void onStopPredictionUpdates() {
        mPredictionUpdatesStarted = false;
    }

    boolean awaitOnStopPredictionUpdates() {
        try {
            return await(mStopPredictionUpdatesLatch);
        } finally {
            mStopPredictionUpdatesLatch = new CountDownLatch(1);
        }
    }

    void onRequestPredictionUpdate(AppPredictionSessionId sessionId) {
        assertTrue(mSessions.containsKey(sessionId));
        mNumRequestedUpdates++;
        mPredictionUpdateLatch.countDown();
    }

    boolean awaitOnRequestPredictionUpdate() {
        try {
            return await(mPredictionUpdateLatch);
        } finally {
            mPredictionUpdateLatch = new CountDownLatch(1);
        }
    }

    void onDestroyPredictionSession(AppPredictionSessionId sessionId) {
        assertTrue(mSessions.containsKey(sessionId));
        mSessions.remove(sessionId);
        mDestroyLatch.countDown();
    }

    boolean awaitOnDestroyPredictionSession() {
        try {
            return await(mDestroyLatch);
        } finally {
            mDestroyLatch = new CountDownLatch(1);
        }
    }

    public class Event {
        final AppTarget target;
        final int launchLocation;
        final int eventType;

        public Event(AppTarget target, int launchLocation, int eventType) {
            this.target = target;
            this.launchLocation = launchLocation;
            this.eventType = eventType;
        }
    }

    private boolean await(CountDownLatch latch) {
        try {
            latch.await(500, TimeUnit.MILLISECONDS);
            return true;
        } catch (InterruptedException e) {
            return false;
        }
    }

    public static class RequestVerifier implements AppPredictor.Callback, PredictionsProvider,
            Consumer<List<AppTarget>> {

        private ServiceReporter mReporter;
        private CountDownLatch mReceivedLatch;
        private List<AppTarget> mTargets;

        public RequestVerifier(ServiceReporter reporter) {
            mReporter = reporter;
            mReceivedLatch = new CountDownLatch(1);
        }

        @Override
        public List<AppTarget> getTargets(AppPredictionSessionId sessionId) {
            return mTargets;
        }

        @Override
        public void onTargetsAvailable(List<AppTarget> targets) {
            if (mTargets != null) {
                // Verify that the targets match
                assertEquals(targets, mTargets);
            } else {
                // For the case where we didn't setup the request, save the targets so we can verify
                // them in awaitTargets()
                mTargets = targets;
            }
            mReceivedLatch.countDown();
        }

        @Override
        public void accept(List<AppTarget> appTargets) {
            onTargetsAvailable(appTargets);
        }

        /**
         * @param requestUpdateCb Callback called when the request is setup
         */
        boolean requestAndWaitForTargets(List<AppTarget> targets, Runnable requestUpdateCb) {
            mTargets = targets;
            mReceivedLatch = new CountDownLatch(1);
            mReporter.setPredictionsProvider(this);
            requestUpdateCb.run();
            try {
                return awaitTargets(targets);
            } finally {
                mReporter.setPredictionsProvider(null);
            }
        }

        boolean awaitTargets(List<AppTarget> targets) {
            try {
                boolean result = mReceivedLatch.await(500, TimeUnit.MILLISECONDS);
                assertEquals(targets, mTargets);
                return result;
            } catch (InterruptedException e) {
                return false;
            }
        }
    }
}
