/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.autofillservice.cts;

import static android.autofillservice.cts.CannedFillResponse.ResponseType.FAILURE;
import static android.autofillservice.cts.CannedFillResponse.ResponseType.NULL;
import static android.autofillservice.cts.CannedFillResponse.ResponseType.TIMEOUT;
import static android.autofillservice.cts.Helper.dumpStructure;
import static android.autofillservice.cts.Helper.getActivityName;
import static android.autofillservice.cts.Timeouts.CONNECTION_TIMEOUT;
import static android.autofillservice.cts.Timeouts.FILL_EVENTS_TIMEOUT;
import static android.autofillservice.cts.Timeouts.FILL_TIMEOUT;
import static android.autofillservice.cts.Timeouts.IDLE_UNBIND_TIMEOUT;
import static android.autofillservice.cts.Timeouts.RESPONSE_DELAY_MS;
import static android.autofillservice.cts.Timeouts.SAVE_TIMEOUT;

import static com.google.common.truth.Truth.assertThat;

import android.app.assist.AssistStructure;
import android.autofillservice.cts.CannedFillResponse.CannedDataset;
import android.autofillservice.cts.CannedFillResponse.ResponseType;
import android.content.ComponentName;
import android.content.IntentSender;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.service.autofill.AutofillService;
import android.service.autofill.Dataset;
import android.service.autofill.FillCallback;
import android.service.autofill.FillContext;
import android.service.autofill.FillEventHistory;
import android.service.autofill.FillEventHistory.Event;
import android.service.autofill.FillResponse;
import android.service.autofill.SaveCallback;
import android.util.Log;
import android.view.inputmethod.InlineSuggestionsRequest;

import androidx.annotation.Nullable;

import com.android.compatibility.common.util.RetryableException;
import com.android.compatibility.common.util.TestNameUtils;
import com.android.compatibility.common.util.Timeout;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Implementation of {@link AutofillService} used in the tests.
 */
public class InstrumentedAutoFillService extends AutofillService {

    static final String SERVICE_PACKAGE = Helper.MY_PACKAGE;
    static final String SERVICE_CLASS = "InstrumentedAutoFillService";

    static final String SERVICE_NAME = SERVICE_PACKAGE + "/." + SERVICE_CLASS;

    // TODO(b/125844305): remove once fixed
    private static final boolean FAIL_ON_INVALID_CONNECTION_STATE = false;

    private static final String TAG = "InstrumentedAutoFillService";

    private static final boolean DUMP_FILL_REQUESTS = false;
    private static final boolean DUMP_SAVE_REQUESTS = false;

    protected static final AtomicReference<InstrumentedAutoFillService> sInstance =
            new AtomicReference<>();
    private static final Replier sReplier = new Replier();

    private static AtomicBoolean sConnected = new AtomicBoolean(false);

    protected static String sServiceLabel = SERVICE_CLASS;

    // We must handle all requests in a separate thread as the service's main thread is the also
    // the UI thread of the test process and we don't want to hose it in case of failures here
    private static final HandlerThread sMyThread = new HandlerThread("MyServiceThread");
    private final Handler mHandler;

    private boolean mConnected;

    static {
        Log.i(TAG, "Starting thread " + sMyThread);
        sMyThread.start();
    }

    public InstrumentedAutoFillService() {
        sInstance.set(this);
        sServiceLabel = SERVICE_CLASS;
        mHandler = Handler.createAsync(sMyThread.getLooper());
        sReplier.setHandler(mHandler);
    }

    private static InstrumentedAutoFillService peekInstance() {
        return sInstance.get();
    }

    /**
     * Gets the list of fill events in the {@link FillEventHistory}, waiting until it has the
     * expected size.
     */
    public static List<Event> getFillEvents(int expectedSize) throws Exception {
        final List<Event> events = getFillEventHistory(expectedSize).getEvents();
        // Sanity check
        if (expectedSize > 0 && events == null || events.size() != expectedSize) {
            throw new IllegalStateException("INTERNAL ERROR: events should have " + expectedSize
                    + ", but it is: " + events);
        }
        return events;
    }

    /**
     * Gets the {@link FillEventHistory}, waiting until it has the expected size.
     */
    public static FillEventHistory getFillEventHistory(int expectedSize) throws Exception {
        final InstrumentedAutoFillService service = peekInstance();

        if (expectedSize == 0) {
            // Need to always sleep as there is no condition / callback to be used to wait until
            // expected number of events is set.
            SystemClock.sleep(FILL_EVENTS_TIMEOUT.ms());
            final FillEventHistory history = service.getFillEventHistory();
            assertThat(history.getEvents()).isNull();
            return history;
        }

        return FILL_EVENTS_TIMEOUT.run("getFillEvents(" + expectedSize + ")", () -> {
            final FillEventHistory history = service.getFillEventHistory();
            if (history == null) {
                return null;
            }
            final List<Event> events = history.getEvents();
            if (events != null) {
                if (events.size() != expectedSize) {
                    Log.v(TAG, "Didn't get " + expectedSize + " events yet: " + events);
                    return null;
                }
            } else {
                Log.v(TAG, "Events is still null (expecting " + expectedSize + ")");
                return null;
            }
            return history;
        });
    }

    /**
     * Asserts there is no {@link FillEventHistory}.
     */
    public static void assertNoFillEventHistory() {
        // Need to always sleep as there is no condition / callback to be used to wait until
        // expected number of events is set.
        SystemClock.sleep(FILL_EVENTS_TIMEOUT.ms());
        assertThat(peekInstance().getFillEventHistory()).isNull();

    }

    /**
     * Gets the service label associated with the current instance.
     */
    public static String getServiceLabel() {
        return sServiceLabel;
    }

    private void handleConnected(boolean connected) {
        Log.v(TAG, "handleConnected(): from " + sConnected.get() + " to " + connected);
        sConnected.set(connected);
    }

    @Override
    public void onConnected() {
        Log.v(TAG, "onConnected");
        if (mConnected && FAIL_ON_INVALID_CONNECTION_STATE) {
            dumpSelf();
            sReplier.addException(new IllegalStateException("onConnected() called again"));
        }
        mConnected = true;
        mHandler.post(() -> handleConnected(true));
    }

    @Override
    public void onDisconnected() {
        Log.v(TAG, "onDisconnected");
        if (!mConnected && FAIL_ON_INVALID_CONNECTION_STATE) {
            dumpSelf();
            sReplier.addException(
                    new IllegalStateException("onDisconnected() called when disconnected"));
        }
        mConnected = false;
        mHandler.post(() -> handleConnected(false));
    }

    @Override
    public void onFillRequest(android.service.autofill.FillRequest request,
            CancellationSignal cancellationSignal, FillCallback callback) {
        final ComponentName component = getLastActivityComponent(request.getFillContexts());
        if (DUMP_FILL_REQUESTS) {
            dumpStructure("onFillRequest()", request.getFillContexts());
        } else {
            Log.i(TAG, "onFillRequest() for " + component.toShortString());
        }
        if (!mConnected && FAIL_ON_INVALID_CONNECTION_STATE) {
            dumpSelf();
            sReplier.addException(
                    new IllegalStateException("onFillRequest() called when disconnected"));
        }

        if (!TestNameUtils.isRunningTest()) {
            Log.e(TAG, "onFillRequest(" + component + ") called after tests finished");
            return;
        }
        if (!fromSamePackage(component))  {
            Log.w(TAG, "Ignoring onFillRequest() from different package: " + component);
            return;
        }
        mHandler.post(
                () -> sReplier.onFillRequest(request.getFillContexts(), request.getClientState(),
                        cancellationSignal, callback, request.getFlags(),
                        request.getInlineSuggestionsRequest(), request.getId()));
    }

    @Override
    public void onSaveRequest(android.service.autofill.SaveRequest request,
            SaveCallback callback) {
        if (!mConnected && FAIL_ON_INVALID_CONNECTION_STATE) {
            dumpSelf();
            sReplier.addException(
                    new IllegalStateException("onSaveRequest() called when disconnected"));
        }
        mHandler.post(()->handleSaveRequest(request, callback));
    }

    private void handleSaveRequest(android.service.autofill.SaveRequest request,
            SaveCallback callback) {
        final ComponentName component = getLastActivityComponent(request.getFillContexts());
        if (!TestNameUtils.isRunningTest()) {
            Log.e(TAG, "onSaveRequest(" + component + ") called after tests finished");
            return;
        }
        if (!fromSamePackage(component)) {
            Log.w(TAG, "Ignoring onSaveRequest() from different package: " + component);
            return;
        }
        if (DUMP_SAVE_REQUESTS) {
            dumpStructure("onSaveRequest()", request.getFillContexts());
        } else {
            Log.i(TAG, "onSaveRequest() for " + component.toShortString());
        }
        mHandler.post(() -> sReplier.onSaveRequest(request.getFillContexts(),
                request.getClientState(), callback,
                request.getDatasetIds()));
    }

    public static boolean isConnected() {
        return sConnected.get();
    }

    private boolean fromSamePackage(ComponentName component) {
        final String actualPackage = component.getPackageName();
        if (!actualPackage.equals(getPackageName())
                && !actualPackage.equals(sReplier.mAcceptedPackageName)) {
            Log.w(TAG, "Got request from package " + actualPackage);
            return false;
        }
        return true;
    }

    private ComponentName getLastActivityComponent(List<FillContext> contexts) {
        return contexts.get(contexts.size() - 1).getStructure().getActivityComponent();
    }

    private void dumpSelf()  {
        try {
            try (StringWriter sw = new StringWriter(); PrintWriter pw = new PrintWriter(sw)) {
                dump(null, pw, null);
                pw.flush();
                final String dump = sw.toString();
                Log.e(TAG, "dumpSelf(): " + dump);
            }
        } catch (IOException e) {
            Log.e(TAG, "I don't always fail to dump, but when I do, I dump the failure", e);
        }
    }

    @Override
    protected void dump(FileDescriptor fd, PrintWriter pw, String[] args) {
        pw.print("sConnected: "); pw.println(sConnected);
        pw.print("mConnected: "); pw.println(mConnected);
        pw.print("sInstance: "); pw.println(sInstance);
        pw.println("sReplier: "); sReplier.dump(pw);
    }

    /**
     * Waits until {@link #onConnected()} is called, or fails if it times out.
     *
     * <p>This method is useful on tests that explicitly verifies the connection, but should be
     * avoided in other tests, as it adds extra time to the test execution (and flakiness in cases
     * where the service might have being disconnected already; for example, if the fill request
     * was replied with a {@code null} response) - if a text needs to block until the service
     * receives a callback, it should use {@link Replier#getNextFillRequest()} instead.
     */
    public static void waitUntilConnected() throws Exception {
        waitConnectionState(CONNECTION_TIMEOUT, true);
    }

    /**
     * Waits until {@link #onDisconnected()} is called, or fails if it times out.
     *
     * <p>This method is useful on tests that explicitly verifies the connection, but should be
     * avoided in other tests, as it adds extra time to the test execution.
     */
    public static void waitUntilDisconnected() throws Exception {
        waitConnectionState(IDLE_UNBIND_TIMEOUT, false);
    }

    private static void waitConnectionState(Timeout timeout, boolean expected) throws Exception {
        timeout.run("wait for connected=" + expected,  () -> {
            return isConnected() == expected ? Boolean.TRUE : null;
        });
    }

    /**
     * Gets the {@link Replier} singleton.
     */
    static Replier getReplier() {
        return sReplier;
    }

    static void resetStaticState() {
        sInstance.set(null);
        sConnected.set(false);
        sServiceLabel = SERVICE_CLASS;
    }

    /**
     * POJO representation of the contents of a
     * {@link AutofillService#onFillRequest(android.service.autofill.FillRequest,
     * CancellationSignal, FillCallback)} that can be asserted at the end of a test case.
     */
    public static final class FillRequest {
        public final AssistStructure structure;
        public final List<FillContext> contexts;
        public final Bundle data;
        public final CancellationSignal cancellationSignal;
        public final FillCallback callback;
        public final int flags;
        public final InlineSuggestionsRequest inlineRequest;

        private FillRequest(List<FillContext> contexts, Bundle data,
                CancellationSignal cancellationSignal, FillCallback callback, int flags,
                InlineSuggestionsRequest inlineRequest) {
            this.contexts = contexts;
            this.data = data;
            this.cancellationSignal = cancellationSignal;
            this.callback = callback;
            this.flags = flags;
            this.structure = contexts.get(contexts.size() - 1).getStructure();
            this.inlineRequest = inlineRequest;
        }

        @Override
        public String toString() {
            return "FillRequest[activity=" + getActivityName(contexts) + ", flags=" + flags
                    + ", bundle=" + data + ", structure=" + Helper.toString(structure) + "]";
        }
    }

    /**
     * POJO representation of the contents of a
     * {@link AutofillService#onSaveRequest(android.service.autofill.SaveRequest, SaveCallback)}
     * that can be asserted at the end of a test case.
     */
    public static final class SaveRequest {
        public final List<FillContext> contexts;
        public final AssistStructure structure;
        public final Bundle data;
        public final SaveCallback callback;
        public final List<String> datasetIds;

        private SaveRequest(List<FillContext> contexts, Bundle data, SaveCallback callback,
                List<String> datasetIds) {
            if (contexts != null && contexts.size() > 0) {
                structure = contexts.get(contexts.size() - 1).getStructure();
            } else {
                structure = null;
            }
            this.contexts = contexts;
            this.data = data;
            this.callback = callback;
            this.datasetIds = datasetIds;
        }

        @Override
        public String toString() {
            return "SaveRequest:" + getActivityName(contexts);
        }
    }

    /**
     * Object used to answer a
     * {@link AutofillService#onFillRequest(android.service.autofill.FillRequest,
     * CancellationSignal, FillCallback)}
     * on behalf of a unit test method.
     */
    public static final class Replier {

        private final BlockingQueue<CannedFillResponse> mResponses = new LinkedBlockingQueue<>();
        private final BlockingQueue<FillRequest> mFillRequests = new LinkedBlockingQueue<>();
        private final BlockingQueue<SaveRequest> mSaveRequests = new LinkedBlockingQueue<>();

        private List<Throwable> mExceptions;
        private IntentSender mOnSaveIntentSender;
        private String mAcceptedPackageName;

        private Handler mHandler;

        private boolean mReportUnhandledFillRequest = true;
        private boolean mReportUnhandledSaveRequest = true;

        private Replier() {
        }

        private IdMode mIdMode = IdMode.RESOURCE_ID;

        public void setIdMode(IdMode mode) {
            this.mIdMode = mode;
        }

        public void acceptRequestsFromPackage(String packageName) {
            mAcceptedPackageName = packageName;
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
         * Sets the expectation for the next {@code onFillRequest} as {@link FillResponse} with just
         * one {@link Dataset}.
         */
        public Replier addResponse(CannedDataset dataset) {
            return addResponse(new CannedFillResponse.Builder()
                    .addDataset(dataset)
                    .build());
        }

        /**
         * Sets the expectation for the next {@code onFillRequest}.
         */
        public Replier addResponse(CannedFillResponse response) {
            if (response == null) {
                throw new IllegalArgumentException("Cannot be null - use NO_RESPONSE instead");
            }
            mResponses.add(response);
            return this;
        }

        /**
         * Sets the {@link IntentSender} that is passed to
         * {@link SaveCallback#onSuccess(IntentSender)}.
         */
        public Replier setOnSave(IntentSender intentSender) {
            mOnSaveIntentSender = intentSender;
            return this;
        }

        /**
         * Gets the next fill request, in the order received.
         */
        public FillRequest getNextFillRequest() {
            FillRequest request;
            try {
                request = mFillRequests.poll(FILL_TIMEOUT.ms(), TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new IllegalStateException("Interrupted", e);
            }
            if (request == null) {
                throw new RetryableException(FILL_TIMEOUT, "onFillRequest() not called");
            }
            return request;
        }

        /**
         * Asserts that {@link #onFillRequest(List, Bundle, CancellationSignal, FillCallback, int)}
         * was not called.
         *
         * <p>Should only be called in cases where it's not expected to be called, as it will
         * sleep for a few ms.
         */
        public void assertOnFillRequestNotCalled() {
            SystemClock.sleep(FILL_TIMEOUT.getMaxValue());
            assertThat(mFillRequests).isEmpty();
        }

        /**
         * Asserts all {@link AutofillService#onFillRequest(
         * android.service.autofill.FillRequest,  CancellationSignal, FillCallback) fill requests}
         * received by the service were properly {@link #getNextFillRequest() handled} by the test
         * case.
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
         * Gets the next save request, in the order received.
         *
         * <p>Typically called at the end of a test case, to assert the initial request.
         */
        public SaveRequest getNextSaveRequest() {
            SaveRequest request;
            try {
                request = mSaveRequests.poll(SAVE_TIMEOUT.ms(), TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new IllegalStateException("Interrupted", e);
            }
            if (request == null) {
                throw new RetryableException(SAVE_TIMEOUT, "onSaveRequest() not called");
            }
            return request;
        }

        /**
         * Asserts all
         * {@link AutofillService#onSaveRequest(android.service.autofill.SaveRequest, SaveCallback)
         * save requests} received by the service were properly
         * {@link #getNextFillRequest() handled} by the test case.
         */
        public void assertNoUnhandledSaveRequests() {
            if (mSaveRequests.isEmpty()) return; // Good job, test case!

            if (!mReportUnhandledSaveRequest) {
                // Just log, so it's not thrown again on @After if already thrown on main body
                Log.d(TAG, "assertNoUnhandledSaveRequests(): already reported, "
                        + "but logging just in case: " + mSaveRequests);
                return;
            }

            mReportUnhandledSaveRequest = false;
            throw new AssertionError(mSaveRequests.size() + " unhandled save requests: "
                    + mSaveRequests);
        }

        public void setHandler(Handler handler) {
            mHandler = handler;
        }

        /**
         * Resets its internal state.
         */
        public void reset() {
            mResponses.clear();
            mFillRequests.clear();
            mSaveRequests.clear();
            mExceptions = null;
            mOnSaveIntentSender = null;
            mAcceptedPackageName = null;
            mReportUnhandledFillRequest = true;
            mReportUnhandledSaveRequest = true;
        }

        private void onFillRequest(List<FillContext> contexts, Bundle data,
                CancellationSignal cancellationSignal, FillCallback callback, int flags,
                InlineSuggestionsRequest inlineRequest, int requestId) {
            try {
                CannedFillResponse response = null;
                try {
                    response = mResponses.poll(CONNECTION_TIMEOUT.ms(), TimeUnit.MILLISECONDS);
                } catch (InterruptedException e) {
                    Log.w(TAG, "Interrupted getting CannedResponse: " + e);
                    Thread.currentThread().interrupt();
                    addException(e);
                    return;
                }
                if (response == null) {
                    final String activityName = getActivityName(contexts);
                    final String msg = "onFillRequest() for activity " + activityName
                            + " received when no canned response was set.";
                    dumpStructure(msg, contexts);
                    return;
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

                if (response.getResponseType() == FAILURE) {
                    Log.d(TAG, "onFillRequest(): replying with failure");
                    callback.onFailure("D'OH!");
                    return;
                }

                if (response.getResponseType() == ResponseType.NO_MORE) {
                    Log.w(TAG, "onFillRequest(): replying with null when not expecting more");
                    addException(new IllegalStateException("got unexpected request"));
                    callback.onSuccess(null);
                    return;
                }

                final String failureMessage = response.getFailureMessage();
                if (failureMessage != null) {
                    Log.v(TAG, "onFillRequest(): failureMessage = " + failureMessage);
                    callback.onFailure(failureMessage);
                    return;
                }

                final FillResponse fillResponse;

                switch (mIdMode) {
                    case RESOURCE_ID:
                        fillResponse = response.asFillResponse(contexts,
                                (id) -> Helper.findNodeByResourceId(contexts, id));
                        break;
                    case HTML_NAME:
                        fillResponse = response.asFillResponse(contexts,
                                (name) -> Helper.findNodeByHtmlName(contexts, name));
                        break;
                    case HTML_NAME_OR_RESOURCE_ID:
                        fillResponse = response.asFillResponse(contexts,
                                (id) -> Helper.findNodeByHtmlNameOrResourceId(contexts, id));
                        break;
                    default:
                        throw new IllegalStateException("Unknown id mode: " + mIdMode);
                }

                if (response.getResponseType() == ResponseType.DELAY) {
                    mHandler.postDelayed(() -> {
                        Log.v(TAG,
                                "onFillRequest(" + requestId + "): fillResponse = " + fillResponse);
                        callback.onSuccess(fillResponse);
                        // Add a fill request to let test case know response was sent.
                        Helper.offer(mFillRequests,
                                new FillRequest(contexts, data, cancellationSignal, callback,
                                        flags, inlineRequest), CONNECTION_TIMEOUT.ms());
                    }, RESPONSE_DELAY_MS);
                } else {
                    Log.v(TAG, "onFillRequest(" + requestId + "): fillResponse = " + fillResponse);
                    callback.onSuccess(fillResponse);
                }
            } catch (Throwable t) {
                addException(t);
            } finally {
                Helper.offer(mFillRequests, new FillRequest(contexts, data, cancellationSignal,
                        callback, flags, inlineRequest), CONNECTION_TIMEOUT.ms());
            }
        }

        private void onSaveRequest(List<FillContext> contexts, Bundle data, SaveCallback callback,
                List<String> datasetIds) {
            Log.d(TAG, "onSaveRequest(): sender=" + mOnSaveIntentSender);

            try {
                if (mOnSaveIntentSender != null) {
                    callback.onSuccess(mOnSaveIntentSender);
                } else {
                    callback.onSuccess();
                }
            } finally {
                Helper.offer(mSaveRequests, new SaveRequest(contexts, data, callback, datasetIds),
                        CONNECTION_TIMEOUT.ms());
            }
        }

        private void dump(PrintWriter pw) {
            pw.print("mResponses: "); pw.println(mResponses);
            pw.print("mFillRequests: "); pw.println(mFillRequests);
            pw.print("mSaveRequests: "); pw.println(mSaveRequests);
            pw.print("mExceptions: "); pw.println(mExceptions);
            pw.print("mOnSaveIntentSender: "); pw.println(mOnSaveIntentSender);
            pw.print("mAcceptedPackageName: "); pw.println(mAcceptedPackageName);
            pw.print("mAcceptedPackageName: "); pw.println(mAcceptedPackageName);
            pw.print("mReportUnhandledFillRequest: "); pw.println(mReportUnhandledSaveRequest);
            pw.print("mIdMode: "); pw.println(mIdMode);
        }
    }
}
