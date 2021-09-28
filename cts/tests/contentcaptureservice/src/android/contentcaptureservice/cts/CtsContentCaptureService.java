/*
 * Copyright (C) 2018 The Android Open Source Project
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
package android.contentcaptureservice.cts;

import static android.contentcaptureservice.cts.Helper.MY_PACKAGE;
import static android.contentcaptureservice.cts.Helper.await;
import static android.contentcaptureservice.cts.Helper.componentNameFor;

import static com.google.common.truth.Truth.assertWithMessage;

import android.content.ComponentName;
import android.os.ParcelFileDescriptor;
import android.service.contentcapture.ActivityEvent;
import android.service.contentcapture.ContentCaptureService;
import android.service.contentcapture.DataShareCallback;
import android.service.contentcapture.DataShareReadAdapter;
import android.util.ArrayMap;
import android.util.ArraySet;
import android.util.Log;
import android.util.Pair;
import android.view.contentcapture.ContentCaptureContext;
import android.view.contentcapture.ContentCaptureEvent;
import android.view.contentcapture.ContentCaptureSessionId;
import android.view.contentcapture.DataRemovalRequest;
import android.view.contentcapture.DataShareRequest;
import android.view.contentcapture.ViewNode;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

// TODO(b/123540602): if we don't move this service to a separate package, we need to handle the
// onXXXX methods in a separate thread
// Either way, we need to make sure its methods are thread safe

public class CtsContentCaptureService extends ContentCaptureService {

    private static final String TAG = CtsContentCaptureService.class.getSimpleName();

    public static final String SERVICE_NAME = MY_PACKAGE + "/."
            + CtsContentCaptureService.class.getSimpleName();
    public static final ComponentName CONTENT_CAPTURE_SERVICE_COMPONENT_NAME =
            componentNameFor(CtsContentCaptureService.class);

    private static final Executor sExecutor = Executors.newCachedThreadPool();

    private static int sIdCounter;

    private static ServiceWatcher sServiceWatcher;

    private final int mId = ++sIdCounter;

    private static final ArrayList<Throwable> sExceptions = new ArrayList<>();

    private final CountDownLatch mConnectedLatch = new CountDownLatch(1);
    private final CountDownLatch mDisconnectedLatch = new CountDownLatch(1);

    /**
     * List of all sessions started - never reset.
     */
    private final ArrayList<ContentCaptureSessionId> mAllSessions = new ArrayList<>();

    /**
     * Map of all sessions started but not finished yet - sessions are removed as they're finished.
     */
    private final ArrayMap<ContentCaptureSessionId, Session> mOpenSessions = new ArrayMap<>();

    /**
     * Map of all sessions finished.
     */
    private final ArrayMap<ContentCaptureSessionId, Session> mFinishedSessions = new ArrayMap<>();

    /**
     * Map of latches for sessions that started but haven't finished yet.
     */
    private final ArrayMap<ContentCaptureSessionId, CountDownLatch> mUnfinishedSessionLatches =
            new ArrayMap<>();

    /**
     * Counter of onCreate() / onDestroy() events.
     */
    private int mLifecycleEventsCounter;

    /**
     * Counter of received {@link ActivityEvent} events.
     */
    private int mActivityEventsCounter;

    // NOTE: we could use the same counter for mLifecycleEventsCounter and mActivityEventsCounter,
    // but that would make the tests flaker.

    /**
     * Used for testing onUserDataRemovalRequest.
     */
    private DataRemovalRequest mRemovalRequest;

    /**
     * List of activity lifecycle events received.
     */
    private final ArrayList<MyActivityEvent> mActivityEvents = new ArrayList<>();

    /**
     * Optional listener for {@code onDisconnect()}.
     */
    @Nullable
    private DisconnectListener mOnDisconnectListener;

    /**
     * When set, doesn't throw exceptions when it receives an event from a session that doesn't
     * exist.
     */
    private boolean mIgnoreOrphanSessionEvents;

    /**
     * Whether the service should accept a data share session.
     */
    private boolean mDataSharingEnabled = false;

    /**
     * Bytes that were shared during the content capture
     */
    byte[] mDataShared = new byte[20_000];

    /**
     * The fields below represent state of the content capture data sharing session.
     */
    boolean mDataShareSessionStarted = false;
    boolean mDataShareSessionFinished = false;
    boolean mDataShareSessionSucceeded = false;
    int mDataShareSessionErrorCode = 0;
    DataShareRequest mDataShareRequest;

    @NonNull
    public static ServiceWatcher setServiceWatcher() {
        if (sServiceWatcher != null) {
            throw new IllegalStateException("There Can Be Only One!");
        }
        sServiceWatcher = new ServiceWatcher();
        return sServiceWatcher;
    }

    public static void resetStaticState() {
        sExceptions.clear();
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

    public static void clearServiceWatcher() {
        if (sServiceWatcher != null) {
            if (sServiceWatcher.mReadyToClear) {
                sServiceWatcher.mService = null;
                sServiceWatcher = null;
            } else {
                sServiceWatcher.mReadyToClear = true;
            }
        }
    }

    /**
     * When set, doesn't throw exceptions when it receives an event from a session that doesn't
     * exist.
     */
    // TODO: try to refactor WhitelistTest so it doesn't need this hack.
    public void setIgnoreOrphanSessionEvents(boolean newValue) {
        Log.d(TAG, "setIgnoreOrphanSessionEvents(): changing from " + mIgnoreOrphanSessionEvents
                + " to " + newValue);
        mIgnoreOrphanSessionEvents = newValue;
    }

    @Override
    public void onConnected() {
        Log.i(TAG, "onConnected(id=" + mId + "): sServiceWatcher=" + sServiceWatcher);

        if (sServiceWatcher == null) {
            addException("onConnected() without a watcher");
            return;
        }

        if (!sServiceWatcher.mReadyToClear && sServiceWatcher.mService != null) {
            addException("onConnected(): already created: %s", sServiceWatcher);
            return;
        }

        sServiceWatcher.mService = this;
        sServiceWatcher.mCreated.countDown();
        sServiceWatcher.mReadyToClear = false;

        if (mConnectedLatch.getCount() == 0) {
            addException("already connected: %s", mConnectedLatch);
        }
        mConnectedLatch.countDown();
    }

    @Override
    public void onDisconnected() {
        Log.i(TAG, "onDisconnected(id=" + mId + "): sServiceWatcher=" + sServiceWatcher);

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
        // Notify test case as well
        if (mOnDisconnectListener != null) {
            final CountDownLatch latch = mOnDisconnectListener.mLatch;
            mOnDisconnectListener = null;
            latch.countDown();
        }
        sServiceWatcher.mDestroyed.countDown();
        clearServiceWatcher();
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
    public void onCreateContentCaptureSession(ContentCaptureContext context,
            ContentCaptureSessionId sessionId) {
        Log.i(TAG, "onCreateContentCaptureSession(id=" + mId + ", ignoreOrpahn="
                + mIgnoreOrphanSessionEvents + ", ctx=" + context + ", session=" + sessionId);
        if (mIgnoreOrphanSessionEvents) return;
        mAllSessions.add(sessionId);

        safeRun(() -> {
            final Session session = mOpenSessions.get(sessionId);
            if (session != null) {
                throw new IllegalStateException("Already contains session for " + sessionId
                        + ": " + session);
            }
            mUnfinishedSessionLatches.put(sessionId, new CountDownLatch(1));
            mOpenSessions.put(sessionId, new Session(sessionId, context));
        });
    }

    @Override
    public void onDestroyContentCaptureSession(ContentCaptureSessionId sessionId) {
        Log.i(TAG, "onDestroyContentCaptureSession(id=" + mId + ", ignoreOrpahn="
                + mIgnoreOrphanSessionEvents + ", session=" + sessionId + ")");
        if (mIgnoreOrphanSessionEvents) return;
        safeRun(() -> {
            final Session session = getExistingSession(sessionId);
            session.finish();
            mOpenSessions.remove(sessionId);
            if (mFinishedSessions.containsKey(sessionId)) {
                throw new IllegalStateException("Already destroyed " + sessionId);
            } else {
                mFinishedSessions.put(sessionId, session);
                final CountDownLatch latch = getUnfinishedSessionLatch(sessionId);
                latch.countDown();
            }
        });
    }

    @Override
    public void onContentCaptureEvent(ContentCaptureSessionId sessionId,
            ContentCaptureEvent event) {
        Log.i(TAG, "onContentCaptureEventsRequest(id=" + mId + ", ignoreOrpahn="
                + mIgnoreOrphanSessionEvents + ", session=" + sessionId + "): " + event);
        if (mIgnoreOrphanSessionEvents) return;
        final ViewNode node = event.getViewNode();
        if (node != null) {
            Log.v(TAG, "onContentCaptureEvent(): parentId=" + node.getParentAutofillId());
        }
        safeRun(() -> {
            final Session session = getExistingSession(sessionId);
            session.mEvents.add(event);
        });
    }

    @Override
    public void onDataRemovalRequest(DataRemovalRequest request) {
        Log.i(TAG, "onUserDataRemovalRequest(id=" + mId + ",req=" + request + ")");
        mRemovalRequest = request;
    }

    @Override
    public void onDataShareRequest(DataShareRequest request, DataShareCallback callback) {
        if (mDataSharingEnabled) {
            mDataShareRequest = request;
            callback.onAccept(sExecutor, new DataShareReadAdapter() {
                @Override
                public void onStart(ParcelFileDescriptor fd) {
                    mDataShareSessionStarted = true;

                    int bytesReadTotal = 0;
                    try (InputStream fis = new ParcelFileDescriptor.AutoCloseInputStream(fd)) {
                        while (true) {
                            int bytesRead = fis.read(mDataShared, bytesReadTotal,
                                    mDataShared.length - bytesReadTotal);
                            if (bytesRead == -1) {
                                break;
                            }
                            bytesReadTotal += bytesRead;
                        }
                        mDataShareSessionFinished = true;
                        mDataShareSessionSucceeded = true;
                    } catch (IOException e) {
                        // fall through. dataShareSessionSucceeded will stay false.
                    }
                }

                @Override
                public void onError(int errorCode) {
                    mDataShareSessionFinished = true;
                    mDataShareSessionErrorCode = errorCode;
                }
            });
        } else {
            callback.onReject();
            mDataShareSessionStarted = mDataShareSessionFinished = true;
        }
    }

    @Override
    public void onActivityEvent(ActivityEvent event) {
        Log.i(TAG, "onActivityEvent(): " + event);
        mActivityEvents.add(new MyActivityEvent(event));
    }

    /**
     * Gets the cached UserDataRemovalRequest for testing.
     */
    public DataRemovalRequest getRemovalRequest() {
        return mRemovalRequest;
    }

    /**
     * Gets the finished session for the given session id.
     *
     * @throws IllegalStateException if the session didn't finish yet.
     */
    @NonNull
    public Session getFinishedSession(@NonNull ContentCaptureSessionId sessionId)
            throws InterruptedException {
        final CountDownLatch latch = getUnfinishedSessionLatch(sessionId);
        await(latch, "session %s not finished yet", sessionId);

        final Session session = mFinishedSessions.get(sessionId);
        if (session == null) {
            throwIllegalSessionStateException("No finished session for id %s", sessionId);
        }
        return session;
    }

    /**
     * Gets the finished session when only one session is expected.
     *
     * <p>Should be used when the test case doesn't known in advance the id of the session.
     */
    @NonNull
    public Session getOnlyFinishedSession() throws InterruptedException {
        final ArrayList<ContentCaptureSessionId> allSessions = mAllSessions;
        assertWithMessage("Wrong number of sessions").that(allSessions).hasSize(1);
        final ContentCaptureSessionId id = allSessions.get(0);
        Log.d(TAG, "getOnlyFinishedSession(): id=" + id);
        return getFinishedSession(id);
    }

    /**
     * Gets all sessions that have been created so far.
     */
    @NonNull
    public List<ContentCaptureSessionId> getAllSessionIds() {
        return Collections.unmodifiableList(mAllSessions);
    }

    /**
     * Sets a listener to wait until the service disconnects.
     */
    @NonNull
    public DisconnectListener setOnDisconnectListener() {
        if (mOnDisconnectListener != null) {
            throw new IllegalStateException("already set");
        }
        mOnDisconnectListener = new DisconnectListener();
        return mOnDisconnectListener;
    }

    public void setDataSharingEnabled(boolean enabled) {
        this.mDataSharingEnabled = enabled;
    }

    @Override
    protected void dump(FileDescriptor fd, PrintWriter pw, String[] args) {
        super.dump(fd, pw, args);

        pw.print("sServiceWatcher: "); pw.println(sServiceWatcher);
        pw.print("sExceptions: "); pw.println(sExceptions);
        pw.print("sIdCounter: "); pw.println(sIdCounter);
        pw.print("mId: "); pw.println(mId);
        pw.print("mConnectedLatch: "); pw.println(mConnectedLatch);
        pw.print("mDisconnectedLatch: "); pw.println(mDisconnectedLatch);
        pw.print("mAllSessions: "); pw.println(mAllSessions);
        pw.print("mOpenSessions: "); pw.println(mOpenSessions);
        pw.print("mFinishedSessions: "); pw.println(mFinishedSessions);
        pw.print("mUnfinishedSessionLatches: "); pw.println(mUnfinishedSessionLatches);
        pw.print("mLifecycleEventsCounter: "); pw.println(mLifecycleEventsCounter);
        pw.print("mActivityEventsCounter: "); pw.println(mActivityEventsCounter);
        pw.print("mActivityLifecycleEvents: "); pw.println(mActivityEvents);
        pw.print("mIgnoreOrphanSessionEvents: "); pw.println(mIgnoreOrphanSessionEvents);
    }

    @NonNull
    private CountDownLatch getUnfinishedSessionLatch(final ContentCaptureSessionId sessionId) {
        final CountDownLatch latch = mUnfinishedSessionLatches.get(sessionId);
        if (latch == null) {
            throwIllegalSessionStateException("no latch for %s", sessionId);
        }
        return latch;
    }

    /**
     * Gets the exceptions that were thrown while the service handlded requests.
     */
    public static List<Throwable> getExceptions() throws Exception {
        return Collections.unmodifiableList(sExceptions);
    }

    private void throwIllegalSessionStateException(@NonNull String fmt, @Nullable Object...args) {
        throw new IllegalStateException(String.format(fmt, args)
                + ".\nID=" + mId
                + ".\nAll=" + mAllSessions
                + ".\nOpen=" + mOpenSessions
                + ".\nLatches=" + mUnfinishedSessionLatches
                + ".\nFinished=" + mFinishedSessions
                + ".\nLifecycles=" + mActivityEvents
                + ".\nIgnoringOrphan=" + mIgnoreOrphanSessionEvents);
    }

    private Session getExistingSession(@NonNull ContentCaptureSessionId sessionId) {
        final Session session = mOpenSessions.get(sessionId);
        if (session == null) {
            throwIllegalSessionStateException("No open session with id %s", sessionId);
        }
        if (session.finished) {
            throw new IllegalStateException("session already finished: " + session);
        }

        return session;
    }

    private void safeRun(@NonNull Runnable r) {
        try {
            r.run();
        } catch (Throwable t) {
            Log.e(TAG, "Exception handling service callback: " + t);
            sExceptions.add(t);
        }
    }

    private static void addException(@NonNull String fmt, @Nullable Object...args) {
        final String msg = String.format(fmt, args);
        Log.e(TAG, msg);
        sExceptions.add(new IllegalStateException(msg));
    }

    public final class Session {
        public final ContentCaptureSessionId id;
        public final ContentCaptureContext context;
        public final int creationOrder;
        private final List<ContentCaptureEvent> mEvents = new ArrayList<>();
        public boolean finished;
        public int destructionOrder;

        private Session(ContentCaptureSessionId id, ContentCaptureContext context) {
            this.id = id;
            this.context = context;
            creationOrder = ++mLifecycleEventsCounter;
            Log.d(TAG, "create(" + id  + "): order=" + creationOrder);
        }

        private void finish() {
            finished = true;
            destructionOrder = ++mLifecycleEventsCounter;
            Log.d(TAG, "finish(" + id  + "): order=" + destructionOrder);
        }

        // TODO(b/123540602): currently we're only interested on all events, but eventually we
        // should track individual requests as well to make sure they're probably batch (it will
        // require adding a Settings to tune the buffer parameters.
        public List<ContentCaptureEvent> getEvents() {
            return Collections.unmodifiableList(mEvents);
        }

        @Override
        public String toString() {
            return "[id=" + id + ", context=" + context + ", events=" + mEvents.size()
                    + ", finished=" + finished + "]";
        }
    }

    private final class MyActivityEvent {
        public final int order;
        public final ActivityEvent event;

        private MyActivityEvent(ActivityEvent event) {
            order = ++mActivityEventsCounter;
            this.event = event;
        }

        @Override
        public String toString() {
            return order + "-" + event;
        }
    }

    public static final class ServiceWatcher {

        private final CountDownLatch mCreated = new CountDownLatch(1);
        private final CountDownLatch mDestroyed = new CountDownLatch(1);
        private boolean mReadyToClear = true;
        private Pair<Set<String>, Set<ComponentName>> mWhitelist;

        private CtsContentCaptureService mService;

        @NonNull
        public CtsContentCaptureService waitOnCreate() throws InterruptedException {
            await(mCreated, "not created");

            if (mService == null) {
                throw new IllegalStateException("not created");
            }

            if (mWhitelist != null) {
                Log.d(TAG, "Whitelisting after created: " + mWhitelist);
                mService.setContentCaptureWhitelist(mWhitelist.first, mWhitelist.second);
            }

            return mService;
        }

        public void waitOnDestroy() throws InterruptedException {
            await(mDestroyed, "not destroyed");
        }

        /**
         * Whitelists stuff when the service connects.
         */
        public void whitelist(@Nullable Pair<Set<String>, Set<ComponentName>> whitelist) {
            mWhitelist = whitelist;
        }

       /**
        * Whitelists just this package.
        */
        public void whitelistSelf() {
            final ArraySet<String> pkgs = new ArraySet<>(1);
            pkgs.add(MY_PACKAGE);
            whitelist(new Pair<>(pkgs, null));
        }

        @Override
        public String toString() {
            return "mService: " + mService + " created: " + (mCreated.getCount() == 0)
                    + " destroyed: " + (mDestroyed.getCount() == 0)
                    + " whitelist: " + mWhitelist;
        }
    }

    /**
     * Listener used to block until the service is disconnected.
     */
    public class DisconnectListener {
        private final CountDownLatch mLatch = new CountDownLatch(1);

        /**
         * Wait or die!
         */
        public void waitForOnDisconnected() {
            try {
                await(mLatch, "not disconnected");
            } catch (Exception e) {
                addException("DisconnectListener: onDisconnected() not called: " + e);
            }
        }
    }

    // TODO: make logic below more generic so it can be used for other events (and possibly move
    // it to another helper class)

    @NonNull
    public EventsAssertor assertThat() {
        return new EventsAssertor(mActivityEvents);
    }

    public static final class EventsAssertor {
        private final List<MyActivityEvent> mEvents;
        private int mNextEvent = 0;

        private EventsAssertor(ArrayList<MyActivityEvent> events) {
            mEvents = Collections.unmodifiableList(events);
            Log.v(TAG, "EventsAssertor: " + mEvents);
        }

        @NonNull
        public EventsAssertor activityResumed(@NonNull ComponentName expectedActivity) {
            assertNextEvent((event) -> assertActivityEvent(event, expectedActivity,
                    ActivityEvent.TYPE_ACTIVITY_RESUMED), "no ACTIVITY_RESUMED event for %s",
                    expectedActivity);
            return this;
        }

        @NonNull
        public EventsAssertor activityPaused(@NonNull ComponentName expectedActivity) {
            assertNextEvent((event) -> assertActivityEvent(event, expectedActivity,
                    ActivityEvent.TYPE_ACTIVITY_PAUSED), "no ACTIVITY_PAUSED event for %s",
                    expectedActivity);
            return this;
        }

        @NonNull
        public EventsAssertor activityStopped(@NonNull ComponentName expectedActivity) {
            assertNextEvent((event) -> assertActivityEvent(event, expectedActivity,
                    ActivityEvent.TYPE_ACTIVITY_STOPPED), "no ACTIVITY_STOPPED event for %s",
                    expectedActivity);
            return this;
        }

        @NonNull
        public EventsAssertor activityDestroyed(@NonNull ComponentName expectedActivity) {
            assertNextEvent((event) -> assertActivityEvent(event, expectedActivity,
                    ActivityEvent.TYPE_ACTIVITY_DESTROYED), "no ACTIVITY_DESTROYED event for %s",
                    expectedActivity);
            return this;
        }

        private void assertNextEvent(@NonNull EventAssertion assertion, @NonNull String errorFormat,
                @Nullable Object... errorArgs) {
            if (mNextEvent >= mEvents.size()) {
                throw new AssertionError("Reached the end of the events: "
                        + String.format(errorFormat, errorArgs) + "\n. Events("
                        + mEvents.size() + "): " + mEvents);
            }
            do {
                final int index = mNextEvent++;
                final MyActivityEvent event = mEvents.get(index);
                final String error = assertion.getErrorMessage(event);
                if (error == null) return;
                Log.w(TAG, "assertNextEvent(): ignoring event #" + index + "(" + event + "): "
                        + error);
            } while (mNextEvent < mEvents.size());
            throw new AssertionError(String.format(errorFormat, errorArgs) + "\n. Events("
                    + mEvents.size() + "): " + mEvents);
        }
    }

    @Nullable
    public static String assertActivityEvent(@NonNull MyActivityEvent myEvent,
            @NonNull ComponentName expectedActivity, int expectedType) {
        if (myEvent == null) {
            return "myEvent is null";
        }
        final ActivityEvent event = myEvent.event;
        if (event == null) {
            return "event is null";
        }
        final int actualType = event.getEventType();
        if (actualType != expectedType) {
            return String.format("wrong event type for %s: expected %s, got %s", event,
                    expectedType, actualType);
        }
        final ComponentName actualActivity = event.getComponentName();
        if (!expectedActivity.equals(actualActivity)) {
            return String.format("wrong activity for %s: expected %s, got %s", event,
                    expectedActivity, actualActivity);
        }
        return null;
    }

    private interface EventAssertion {
        @Nullable
        String getErrorMessage(@NonNull MyActivityEvent event);
    }
}
