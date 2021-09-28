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

package android.autofillservice.cts.augmented;

import static android.autofillservice.cts.Timeouts.FILL_EVENTS_TIMEOUT;
import static android.autofillservice.cts.augmented.AugmentedHelper.await;
import static android.autofillservice.cts.augmented.AugmentedHelper.getActivityName;
import static android.autofillservice.cts.augmented.AugmentedTimeouts.AUGMENTED_CONNECTION_TIMEOUT;
import static android.autofillservice.cts.augmented.AugmentedTimeouts.AUGMENTED_FILL_TIMEOUT;
import static android.autofillservice.cts.augmented.CannedAugmentedFillResponse.AugmentedResponseType.NULL;
import static android.autofillservice.cts.augmented.CannedAugmentedFillResponse.AugmentedResponseType.TIMEOUT;

import static com.google.common.truth.Truth.assertWithMessage;

import android.autofillservice.cts.Helper;
import android.content.ComponentName;
import android.content.Context;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.service.autofill.FillEventHistory;
import android.service.autofill.FillEventHistory.Event;
import android.service.autofill.augmented.AugmentedAutofillService;
import android.service.autofill.augmented.FillCallback;
import android.service.autofill.augmented.FillController;
import android.service.autofill.augmented.FillRequest;
import android.service.autofill.augmented.FillResponse;
import android.util.ArraySet;
import android.util.Log;
import android.view.autofill.AutofillManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.compatibility.common.util.RetryableException;
import com.android.compatibility.common.util.TestNameUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Implementation of {@link AugmentedAutofillService} used in the tests.
 */
public class CtsAugmentedAutofillService extends AugmentedAutofillService {

    private static final String TAG = CtsAugmentedAutofillService.class.getSimpleName();

    public static final String SERVICE_PACKAGE = Helper.MY_PACKAGE;
    public static final String SERVICE_CLASS = CtsAugmentedAutofillService.class.getSimpleName();

    public static final String SERVICE_NAME = SERVICE_PACKAGE + "/.augmented." + SERVICE_CLASS;

    private static final AugmentedReplier sAugmentedReplier = new AugmentedReplier();

    // We must handle all requests in a separate thread as the service's main thread is the also
    // the UI thread of the test process and we don't want to hose it in case of failures here
    private static final HandlerThread sMyThread = new HandlerThread("MyAugmentedServiceThread");
    private final Handler mHandler;

    private final CountDownLatch mConnectedLatch = new CountDownLatch(1);
    private final CountDownLatch mDisconnectedLatch = new CountDownLatch(1);

    private static ServiceWatcher sServiceWatcher;

    static {
        Log.i(TAG, "Starting thread " + sMyThread);
        sMyThread.start();
    }

    public CtsAugmentedAutofillService() {
        mHandler = Handler.createAsync(sMyThread.getLooper());
    }

    @NonNull
    public static ServiceWatcher setServiceWatcher() {
        if (sServiceWatcher != null) {
            throw new IllegalStateException("There Can Be Only One!");
        }
        sServiceWatcher = new ServiceWatcher();
        return sServiceWatcher;
    }


    public static void resetStaticState() {
        List<Throwable> exceptions = sAugmentedReplier.mExceptions;
        if (exceptions != null) {
            exceptions.clear();
        }
        // TODO(b/123540602): should probably set sInstance to null as well, but first we would need
        // to make sure each test unbinds the service.

        // TODO(b/123540602): each test should use a different service instance, but we need
        // to provide onConnected() / onDisconnected() methods first and then change the infra so
        // we can wait for those

        if (sServiceWatcher != null) {
            Log.wtf(TAG, "resetStaticState(): should not have sServiceWatcher");
            sServiceWatcher = null;
        }
    }

    @Override
    public void onConnected() {
        Log.i(TAG, "onConnected(): sServiceWatcher=" + sServiceWatcher);

        if (sServiceWatcher == null) {
            addException("onConnected() without a watcher");
            return;
        }

        if (sServiceWatcher.mService != null) {
            addException("onConnected(): already created: %s", sServiceWatcher);
            return;
        }

        sServiceWatcher.mService = this;
        sServiceWatcher.mCreated.countDown();

        Log.d(TAG, "Whitelisting " + Helper.MY_PACKAGE + " for augmented autofill");
        final ArraySet<String> packages = new ArraySet<>(1);
        packages.add(Helper.MY_PACKAGE);

        final AutofillManager afm = getApplication().getSystemService(AutofillManager.class);
        if (afm == null) {
            addException("No AutofillManager on application context on onConnected()");
            return;
        }
        afm.setAugmentedAutofillWhitelist(packages, /* activities= */ null);

        if (mConnectedLatch.getCount() == 0) {
            addException("already connected: %s", mConnectedLatch);
        }
        mConnectedLatch.countDown();
    }

    @Override
    public void onDisconnected() {
        Log.i(TAG, "onDisconnected(): sServiceWatcher=" + sServiceWatcher);

        if (mDisconnectedLatch.getCount() == 0) {
            addException("already disconnected: %s", mConnectedLatch);
        }
        mDisconnectedLatch.countDown();

        if (sServiceWatcher == null) {
            addException("onDisconnected() without a watcher");
            return;
        }
        if (sServiceWatcher.mService == null) {
            addException("onDisconnected(): no service on %s", sServiceWatcher);
            return;
        }

        sServiceWatcher.mDestroyed.countDown();
        sServiceWatcher.mService = null;
        sServiceWatcher = null;
    }

    public FillEventHistory getFillEventHistory(int expectedSize) throws Exception {
        return FILL_EVENTS_TIMEOUT.run("getFillEvents(" + expectedSize + ")", () -> {
            final FillEventHistory history = getFillEventHistory();
            if (history == null) {
                return null;
            }
            final List<Event> events = history.getEvents();
            if (events != null) {
                assertWithMessage("Didn't get " + expectedSize + " events yet: " + events).that(
                        events.size()).isEqualTo(expectedSize);
            } else {
                assertWithMessage("Events is null (expecting " + expectedSize + ")").that(
                        expectedSize).isEqualTo(0);
                return null;
            }
            return history;
        });
    }

    /**
     * Waits until the system calls {@link #onConnected()}.
     */
    public void waitUntilConnected() throws InterruptedException {
        await(mConnectedLatch, "not connected");
    }

    /**
     * Waits until the system calls {@link #onDisconnected()}.
     */
    public void waitUntilDisconnected() throws InterruptedException {
        await(mDisconnectedLatch, "not disconnected");
    }

    @Override
    public void onFillRequest(FillRequest request, CancellationSignal cancellationSignal,
            FillController controller, FillCallback callback) {
        Log.i(TAG, "onFillRequest(): " + AugmentedHelper.toString(request));

        final ComponentName component = request.getActivityComponent();

        if (!TestNameUtils.isRunningTest()) {
            Log.e(TAG, "onFillRequest(" + component + ") called after tests finished");
            return;
        }
        mHandler.post(() -> sAugmentedReplier.handleOnFillRequest(getApplicationContext(), request,
                cancellationSignal, controller, callback));
    }

    /**
     * Gets the {@link AugmentedReplier} singleton.
     */
    static AugmentedReplier getAugmentedReplier() {
        return sAugmentedReplier;
    }

    private static void addException(@NonNull String fmt, @Nullable Object...args) {
        final String msg = String.format(fmt, args);
        Log.e(TAG, msg);
        sAugmentedReplier.addException(new IllegalStateException(msg));
    }

    /**
     * POJO representation of the contents of a {@link FillRequest}
     * that can be asserted at the end of a test case.
     */
    public static final class AugmentedFillRequest {
        public final FillRequest request;
        public final CancellationSignal cancellationSignal;
        public final FillController controller;
        public final FillCallback callback;

        private AugmentedFillRequest(FillRequest request, CancellationSignal cancellationSignal,
                FillController controller, FillCallback callback) {
            this.request = request;
            this.cancellationSignal = cancellationSignal;
            this.controller = controller;
            this.callback = callback;
        }

        @Override
        public String toString() {
            return "AugmentedFillRequest[activity=" + getActivityName(request) + ", request="
                    + AugmentedHelper.toString(request) + "]";
        }
    }

    /**
     * Object used to answer a
     * {@link AugmentedAutofillService#onFillRequest(FillRequest, CancellationSignal,
     * FillController, FillCallback)} on behalf of a unit test method.
     */
    public static final class AugmentedReplier {

        private final BlockingQueue<CannedAugmentedFillResponse> mResponses =
                new LinkedBlockingQueue<>();
        private final BlockingQueue<AugmentedFillRequest> mFillRequests =
                new LinkedBlockingQueue<>();

        private List<Throwable> mExceptions;
        private boolean mReportUnhandledFillRequest = true;

        private AugmentedReplier() {
        }

        /**
         * Gets the exceptions thrown asynchronously, if any.
         */
        @Nullable
        public List<Throwable> getExceptions() {
            return mExceptions;
        }

        private void addException(@Nullable Throwable e) {
            if (e == null) return;

            if (mExceptions == null) {
                mExceptions = new ArrayList<>();
            }
            mExceptions.add(e);
        }

        /**
         * Sets the expectation for the next {@code onFillRequest}.
         */
        public AugmentedReplier addResponse(@NonNull CannedAugmentedFillResponse response) {
            if (response == null) {
                throw new IllegalArgumentException("Cannot be null - use NO_RESPONSE instead");
            }
            mResponses.add(response);
            return this;
        }
        /**
         * Gets the next fill request, in the order received.
         */
        public AugmentedFillRequest getNextFillRequest() {
            AugmentedFillRequest request;
            try {
                request = mFillRequests.poll(AUGMENTED_FILL_TIMEOUT.ms(), TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new IllegalStateException("Interrupted", e);
            }
            if (request == null) {
                throw new RetryableException(AUGMENTED_FILL_TIMEOUT, "onFillRequest() not called");
            }
            return request;
        }

        /**
         * Asserts all {@link AugmentedAutofillService#onFillRequest(FillRequest,
         * CancellationSignal, FillController, FillCallback)} received by the service were properly
         * {@link #getNextFillRequest() handled} by the test case.
         */
        public void assertNoUnhandledFillRequests() {
            if (mFillRequests.isEmpty()) return; // Good job, test case!

            if (!mReportUnhandledFillRequest) {
                // Just log, so it's not thrown again on @After if already thrown on main body
                Log.d(TAG, "assertNoUnhandledFillRequests(): already reported, "
                        + "but logging just in case: " + mFillRequests);
                return;
            }

            mReportUnhandledFillRequest = false;
            throw new AssertionError(mFillRequests.size() + " unhandled fill requests: "
                    + mFillRequests);
        }

        /**
         * Gets the current number of unhandled requests.
         */
        public int getNumberUnhandledFillRequests() {
            return mFillRequests.size();
        }

        /**
         * Resets its internal state.
         */
        public void reset() {
            mResponses.clear();
            mFillRequests.clear();
            mExceptions = null;
            mReportUnhandledFillRequest = true;
        }

        private void handleOnFillRequest(@NonNull Context context, @NonNull FillRequest request,
                @NonNull CancellationSignal cancellationSignal, @NonNull FillController controller,
                @NonNull FillCallback callback) {
            final AugmentedFillRequest myRequest = new AugmentedFillRequest(request,
                    cancellationSignal, controller, callback);
            Log.d(TAG, "offering " + myRequest);
            Helper.offer(mFillRequests, myRequest, AUGMENTED_CONNECTION_TIMEOUT.ms());
            try {
                final CannedAugmentedFillResponse response;
                try {
                    response = mResponses.poll(AUGMENTED_CONNECTION_TIMEOUT.ms(),
                            TimeUnit.MILLISECONDS);
                } catch (InterruptedException e) {
                    Log.w(TAG, "Interrupted getting CannedAugmentedFillResponse: " + e);
                    Thread.currentThread().interrupt();
                    addException(e);
                    return;
                }
                if (response == null) {
                    Log.w(TAG, "onFillRequest() for " + getActivityName(request)
                            + " received when no canned response was set.");
                    return;
                }

                // sleep for timeout tests.
                final long delay = response.getDelay();
                if (delay > 0) {
                    SystemClock.sleep(response.getDelay());
                }

                if (response.getResponseType() == NULL) {
                    Log.d(TAG, "onFillRequest(): replying with null");
                    callback.onSuccess(null);
                    return;
                }

                if (response.getResponseType() == TIMEOUT) {
                    Log.d(TAG, "onFillRequest(): not replying at all");
                    return;
                }

                Log.v(TAG, "onFillRequest(): response = " + response);
                final FillResponse fillResponse = response.asFillResponse(context, request,
                        controller);
                Log.v(TAG, "onFillRequest(): fillResponse = " + fillResponse);
                callback.onSuccess(fillResponse);
            } catch (Throwable t) {
                addException(t);
            }
        }
    }

    public static final class ServiceWatcher {

        private final CountDownLatch mCreated = new CountDownLatch(1);
        private final CountDownLatch mDestroyed = new CountDownLatch(1);

        private CtsAugmentedAutofillService mService;

        @NonNull
        public CtsAugmentedAutofillService waitOnConnected() throws InterruptedException {
            await(mCreated, "not created");

            if (mService == null) {
                throw new IllegalStateException("not created");
            }

            return mService;
        }

        public void waitOnDisconnected() throws InterruptedException {
            await(mDestroyed, "not destroyed");
        }

        @Override
        public String toString() {
            return "mService: " + mService + " created: " + (mCreated.getCount() == 0)
                    + " destroyed: " + (mDestroyed.getCount() == 0);
        }
    }
}
