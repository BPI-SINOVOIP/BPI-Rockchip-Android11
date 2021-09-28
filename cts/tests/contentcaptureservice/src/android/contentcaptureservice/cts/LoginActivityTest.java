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

import static android.contentcaptureservice.cts.Assertions.assertChildSessionContext;
import static android.contentcaptureservice.cts.Assertions.assertContextUpdated;
import static android.contentcaptureservice.cts.Assertions.assertDecorViewAppeared;
import static android.contentcaptureservice.cts.Assertions.assertMainSessionContext;
import static android.contentcaptureservice.cts.Assertions.assertRightActivity;
import static android.contentcaptureservice.cts.Assertions.assertRightRelationship;
import static android.contentcaptureservice.cts.Assertions.assertSessionId;
import static android.contentcaptureservice.cts.Assertions.assertSessionPaused;
import static android.contentcaptureservice.cts.Assertions.assertSessionResumed;
import static android.contentcaptureservice.cts.Assertions.assertViewAppeared;
import static android.contentcaptureservice.cts.Assertions.assertViewTextChanged;
import static android.contentcaptureservice.cts.Assertions.assertViewTreeFinished;
import static android.contentcaptureservice.cts.Assertions.assertViewTreeStarted;
import static android.contentcaptureservice.cts.Assertions.assertViewsOptionallyDisappeared;
import static android.contentcaptureservice.cts.Helper.MY_PACKAGE;
import static android.contentcaptureservice.cts.Helper.newImportantView;
import static android.view.contentcapture.DataRemovalRequest.FLAG_IS_PREFIX;

import static com.android.compatibility.common.util.ActivitiesWatcher.ActivityLifecycle.DESTROYED;
import static com.android.compatibility.common.util.ActivitiesWatcher.ActivityLifecycle.RESUMED;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.content.ComponentName;
import android.content.LocusId;
import android.contentcaptureservice.cts.CtsContentCaptureService.Session;
import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;
import android.util.ArraySet;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.view.autofill.AutofillId;
import android.view.contentcapture.ContentCaptureContext;
import android.view.contentcapture.ContentCaptureEvent;
import android.view.contentcapture.ContentCaptureSession;
import android.view.contentcapture.ContentCaptureSessionId;
import android.view.contentcapture.DataRemovalRequest;
import android.view.contentcapture.DataRemovalRequest.LocusIdRequest;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.ActivitiesWatcher.ActivityWatcher;
import com.android.compatibility.common.util.DoubleVisitor;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReference;

@AppModeFull(reason = "BlankWithTitleActivityTest is enough")
public class LoginActivityTest
        extends AbstractContentCaptureIntegrationAutoActivityLaunchTest<LoginActivity> {

    private static final String TAG = LoginActivityTest.class.getSimpleName();

    private static final int NO_FLAGS = 0;

    private static final ActivityTestRule<LoginActivity> sActivityRule = new ActivityTestRule<>(
            LoginActivity.class, false, false);

    public LoginActivityTest() {
        super(LoginActivity.class);
    }

    @Override
    protected ActivityTestRule<LoginActivity> getActivityTestRule() {
        return sActivityRule;
    }

    @Before
    @After
    public void resetActivityStaticState() {
        LoginActivity.onRootView(null);
    }

    @Test
    public void testSimpleLifecycle_defaultSession() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        Log.v(TAG, "session id: " + session.id);

        activity.assertDefaultEvents(session);

        final ComponentName name = activity.getComponentName();
        service.assertThat()
                .activityResumed(name)
                .activityPaused(name);
    }

    @Test
    public void testContentCaptureSessionCache() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        final ContentCaptureContext clientContext = newContentCaptureContext();

        final AtomicReference<ContentCaptureSession> mainSessionRef = new AtomicReference<>();
        final AtomicReference<ContentCaptureSession> childSessionRef = new AtomicReference<>();

        LoginActivity.onRootView((activity, rootView) -> {
            final ContentCaptureSession mainSession = rootView.getContentCaptureSession();
            mainSessionRef.set(mainSession);
            final ContentCaptureSession childSession = mainSession
                    .createContentCaptureSession(clientContext);
            childSessionRef.set(childSession);

            rootView.setContentCaptureSession(childSession);
            // Already called getContentCaptureSession() earlier, use cached session (main).
            assertThat(rootView.getContentCaptureSession()).isEqualTo(childSession);

            rootView.setContentCaptureSession(mainSession);
            assertThat(rootView.getContentCaptureSession()).isEqualTo(mainSession);

            rootView.setContentCaptureSession(childSession);
            assertThat(rootView.getContentCaptureSession()).isEqualTo(childSession);
        });

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final ContentCaptureSessionId childSessionId = childSessionRef.get()
                .getContentCaptureSessionId();

        assertSessionId(childSessionId, activity.getRootView());
    }

    @Test
    public void testSimpleLifecycle_rootViewSession() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        final ContentCaptureContext clientContext = newContentCaptureContext();

        final AtomicReference<ContentCaptureSession> mainSessionRef = new AtomicReference<>();
        final AtomicReference<ContentCaptureSession> childSessionRef = new AtomicReference<>();

        LoginActivity.onRootView((activity, rootView) -> {
            final ContentCaptureSession mainSession = rootView.getContentCaptureSession();
            mainSessionRef.set(mainSession);
            final ContentCaptureSession childSession = mainSession
                    .createContentCaptureSession(clientContext);
            childSessionRef.set(childSession);
            Log.i(TAG, "Setting root view (" + rootView + ") session to " + childSession);
            rootView.setContentCaptureSession(childSession);
        });

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final ContentCaptureSessionId mainSessionId = mainSessionRef.get()
                .getContentCaptureSessionId();
        final ContentCaptureSessionId childSessionId = childSessionRef.get()
                .getContentCaptureSessionId();
        Log.v(TAG, "session ids: main=" + mainSessionId + ", child=" + childSessionId);

        // Sanity checks
        assertSessionId(childSessionId, activity.getRootView());
        assertSessionId(childSessionId, activity.mUsernameLabel);
        assertSessionId(childSessionId, activity.mUsername);
        assertSessionId(childSessionId, activity.mPassword);
        assertSessionId(childSessionId, activity.mPasswordLabel);

        // Get the sessions
        final Session mainSession = service.getFinishedSession(mainSessionId);
        final Session childSession = service.getFinishedSession(childSessionId);

        assertRightActivity(mainSession, mainSessionId, activity);
        assertRightRelationship(mainSession, childSession);

        // Sanity check
        final List<ContentCaptureSessionId> allSessionIds = service.getAllSessionIds();
        assertThat(allSessionIds).containsExactly(mainSessionId, childSessionId);

        /*
         * Asserts main session
         */

        // Checks context
        assertMainSessionContext(mainSession, activity);

        // Check events
        final List<ContentCaptureEvent> mainEvents = mainSession.getEvents();
        Log.v(TAG, "events(" + mainEvents.size() + ") for main session: " + mainEvents);

        final View grandpa1 = activity.getGrandParent();
        final View grandpa2 = activity.getGrandGrandParent();
        final View decorView = activity.getDecorView();
        final AutofillId rootId = activity.getRootView().getAutofillId();

        final int minEvents = 7; // TODO(b/122315042): disappeared not always sent
        assertThat(mainEvents.size()).isAtLeast(minEvents);
        assertSessionResumed(mainEvents, 0);
        assertViewTreeStarted(mainEvents, 1);
        assertDecorViewAppeared(mainEvents, 2, decorView);
        assertViewAppeared(mainEvents, 3, grandpa2, decorView.getAutofillId());
        assertViewAppeared(mainEvents, 4, grandpa1, grandpa2.getAutofillId());
        assertViewTreeFinished(mainEvents, 5);
        // TODO(b/122315042): these assertions are currently a mess, so let's disable for now and
        // properly fix them later...
        if (false) {
            int pausedIndex = 6;
            final boolean disappeared = assertViewsOptionallyDisappeared(mainEvents, pausedIndex,
                    decorView.getAutofillId(),
                    grandpa2.getAutofillId(), grandpa1.getAutofillId());
            if (disappeared) {
                pausedIndex += 3;
            }
            assertSessionPaused(mainEvents, pausedIndex);
        }

        /*
         * Asserts child session
         */

        // Checks context
        assertChildSessionContext(childSession, "file://dev/null");

        assertContentCaptureContext(childSession.context);

        // Check events
        final List<ContentCaptureEvent> childEvents = childSession.getEvents();
        Log.v(TAG, "events for child session: " + childEvents);
        final int minChildEvents = 5;
        assertThat(childEvents.size()).isAtLeast(minChildEvents);
        assertViewAppeared(childEvents, 0, childSessionId, activity.getRootView(),
                grandpa1.getAutofillId());
        assertViewAppeared(childEvents, 1, childSessionId, activity.mUsernameLabel, rootId);
        assertViewAppeared(childEvents, 2, childSessionId, activity.mUsername, rootId);
        assertViewAppeared(childEvents, 3, childSessionId, activity.mPasswordLabel, rootId);
        assertViewAppeared(childEvents, 4, childSessionId, activity.mPassword, rootId);

        assertViewsOptionallyDisappeared(childEvents, minChildEvents,
                rootId,
                activity.mUsernameLabel.getAutofillId(), activity.mUsername.getAutofillId(),
                activity.mPasswordLabel.getAutofillId(), activity.mPassword.getAutofillId());
    }

    @Test
    public void testSimpleLifecycle_changeContextAfterCreate() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        final ContentCaptureContext newContext1 = newContentCaptureContext();
        final ContentCaptureContext newContext2 = null;

        final View rootView = activity.getRootView();
        final ContentCaptureSession mainSession = rootView.getContentCaptureSession();
        assertThat(mainSession).isNotNull();
        Log.i(TAG, "Updating root view (" + rootView + ") context to " + newContext1);
        mainSession.setContentCaptureContext(newContext1);
        assertContentCaptureContext(mainSession.getContentCaptureContext());

        Log.i(TAG, "Updating root view (" + rootView + ") context to " + newContext2);
        mainSession.setContentCaptureContext(newContext2);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        Log.v(TAG, "session id: " + session.id);

        final int additionalEvents = 2;
        final List<ContentCaptureEvent> events = activity.assertInitialViewsAppeared(session,
                additionalEvents);

        final ContentCaptureEvent event1 = assertContextUpdated(events, LoginActivity.MIN_EVENTS);
        final ContentCaptureContext actualContext = event1.getContentCaptureContext();
        assertContentCaptureContext(actualContext);

        final ContentCaptureEvent event2 = assertContextUpdated(events,
                LoginActivity.MIN_EVENTS + 1);
        assertThat(event2.getContentCaptureContext()).isNull();
    }

    @Test
    public void testSimpleLifecycle_changeContextOnCreate() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        final ContentCaptureContext newContext = newContentCaptureContext();

        LoginActivity.onRootView((activity, rootView) -> {
            final ContentCaptureSession mainSession = rootView.getContentCaptureSession();
            Log.i(TAG, "Setting root view (" + rootView + ") context to " + newContext);
            mainSession.setContentCaptureContext(newContext);
            assertContentCaptureContext(mainSession.getContentCaptureContext());
        });

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        Log.v(TAG, "session id: " + session.id);
        final ContentCaptureSessionId sessionId = session.id;
        assertRightActivity(session, sessionId, activity);

        // Sanity check

        final List<ContentCaptureEvent> events = session.getEvents();
        Log.v(TAG, "events(" + events.size() + "): " + events);
        // TODO(b/123540067): ideally it should be X so it reflects just the views defined
        // in the layout - right now it's generating events for 2 intermediate parents
        // (android:action_mode_bar_stub and android:content), we should try to create an
        // activity without them

        final AutofillId rootId = activity.getRootView().getAutofillId();

        assertThat(events.size()).isAtLeast(11);

        // TODO(b/123540067): get rid of those intermediated parents
        final View grandpa1 = activity.getGrandParent();
        final View grandpa2 = activity.getGrandGrandParent();
        final View decorView = activity.getDecorView();
        final View rootView = activity.getRootView();

        final ContentCaptureEvent ctxUpdatedEvent = assertContextUpdated(events, 0);
        final ContentCaptureContext actualContext = ctxUpdatedEvent.getContentCaptureContext();
        assertContentCaptureContext(actualContext);

        assertSessionResumed(events, 1);
        assertViewTreeStarted(events, 2);
        assertDecorViewAppeared(events, 3, decorView);
        assertViewAppeared(events, 4, grandpa2, decorView.getAutofillId());
        assertViewAppeared(events, 5, grandpa1, grandpa2.getAutofillId());
        assertViewAppeared(events, 6, sessionId, rootView, grandpa1.getAutofillId());
        assertViewAppeared(events, 7, sessionId, activity.mUsernameLabel, rootId);
        assertViewAppeared(events, 8, sessionId, activity.mUsername, rootId);
        assertViewAppeared(events, 9, sessionId, activity.mPasswordLabel, rootId);
        assertViewAppeared(events, 10, sessionId, activity.mPassword, rootId);
    }

    @Test
    public void testTextChanged() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        LoginActivity.onRootView((activity, rootView) -> ((LoginActivity) activity).mUsername
                .setText("user"));

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.syncRunOnUiThread(() -> {
            activity.mUsername.setText("USER");
            activity.mPassword.setText("PASS");
        });

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        final ContentCaptureSessionId sessionId = session.id;

        assertRightActivity(session, sessionId, activity);

        final int additionalEvents = 2;
        final List<ContentCaptureEvent> events = activity.assertInitialViewsAppeared(session,
                additionalEvents);

        final int i = LoginActivity.MIN_EVENTS;

        assertViewTextChanged(events, i, activity.mUsername.getAutofillId(), "USER");
        assertViewTextChanged(events, i + 1, activity.mPassword.getAutofillId(), "PASS");

        activity.assertInitialViewsDisappeared(events, additionalEvents);
    }

    @Test
    public void testTextChangeBuffer() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        LoginActivity.onRootView((activity, rootView) -> ((LoginActivity) activity).mUsername
                .setText(""));

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.syncRunOnUiThread(() -> {
            activity.mUsername.setText("a");
            activity.mUsername.setText("ab");

            activity.mPassword.setText("d");
            activity.mPassword.setText("de");

            activity.mUsername.setText("abc");
        });

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        final ContentCaptureSessionId sessionId = session.id;

        assertRightActivity(session, sessionId, activity);

        final int additionalEvents = 3;
        final List<ContentCaptureEvent> events = activity.assertInitialViewsAppeared(session,
                additionalEvents);

        final int i = LoginActivity.MIN_EVENTS;

        assertViewTextChanged(events, i, activity.mUsername.getAutofillId(), "ab");
        assertViewTextChanged(events, i + 1, activity.mPassword.getAutofillId(), "de");
        assertViewTextChanged(events, i + 2, activity.mUsername.getAutofillId(), "abc");

        activity.assertInitialViewsDisappeared(events, additionalEvents);
    }

    @Test
    public void testDisabledByFlagSecure() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        LoginActivity.onRootView((activity, rootView) -> activity.getWindow()
                .addFlags(WindowManager.LayoutParams.FLAG_SECURE));

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        assertThat((session.context.getFlags()
                & ContentCaptureContext.FLAG_DISABLED_BY_FLAG_SECURE) != 0).isTrue();
        final ContentCaptureSessionId sessionId = session.id;
        Log.v(TAG, "session id: " + sessionId);

        assertRightActivity(session, sessionId, activity);

        final List<ContentCaptureEvent> events = session.getEvents();
        assertThat(events).isEmpty();
    }

    @Test
    public void testDisabledByApp() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        LoginActivity.onRootView((activity, rootView) -> activity.getContentCaptureManager()
                .setContentCaptureEnabled(false));

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        assertThat(activity.getContentCaptureManager().isContentCaptureEnabled()).isFalse();

        activity.syncRunOnUiThread(() -> activity.mUsername.setText("D'OH"));

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        assertThat((session.context.getFlags()
                & ContentCaptureContext.FLAG_DISABLED_BY_APP) != 0).isTrue();
        final ContentCaptureSessionId sessionId = session.id;
        Log.v(TAG, "session id: " + sessionId);

        assertRightActivity(session, sessionId, activity);

        final List<ContentCaptureEvent> events = session.getEvents();
        assertThat(events).isEmpty();
    }

    @Test
    public void testDisabledFlagSecureAndByApp() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        LoginActivity.onRootView((activity, rootView) -> {
            activity.getContentCaptureManager().setContentCaptureEnabled(false);
            activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        });

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        assertThat(activity.getContentCaptureManager().isContentCaptureEnabled()).isFalse();
        activity.syncRunOnUiThread(() -> activity.mUsername.setText("D'OH"));

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        assertThat((session.context.getFlags()
                & ContentCaptureContext.FLAG_DISABLED_BY_APP) != 0).isTrue();
        assertThat((session.context.getFlags()
                & ContentCaptureContext.FLAG_DISABLED_BY_FLAG_SECURE) != 0).isTrue();
        final ContentCaptureSessionId sessionId = session.id;
        Log.v(TAG, "session id: " + sessionId);

        assertRightActivity(session, sessionId, activity);

        final List<ContentCaptureEvent> events = session.getEvents();
        assertThat(events).isEmpty();
    }

    @Test
    public void testUserDataRemovalRequest_forEverything() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        LoginActivity.onRootView((activity, rootView) -> activity.getContentCaptureManager()
                .removeData(new DataRemovalRequest.Builder().forEverything()
                        .build()));

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        DataRemovalRequest request = service.getRemovalRequest();
        assertThat(request).isNotNull();
        assertThat(request.isForEverything()).isTrue();
        assertThat(request.getLocusIdRequests()).isNull();
        assertThat(request.getPackageName()).isEqualTo(MY_PACKAGE);
    }

    @Test
    public void testUserDataRemovalRequest_oneId() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        final LocusId locusId = new LocusId("com.example");

        LoginActivity.onRootView((activity, rootView) -> activity.getContentCaptureManager()
                .removeData(new DataRemovalRequest.Builder()
                        .addLocusId(locusId, NO_FLAGS)
                        .build()));

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        DataRemovalRequest request = service.getRemovalRequest();
        assertThat(request).isNotNull();
        assertThat(request.isForEverything()).isFalse();
        assertThat(request.getPackageName()).isEqualTo(MY_PACKAGE);

        final List<LocusIdRequest> requests = request.getLocusIdRequests();
        assertThat(requests.size()).isEqualTo(1);

        final LocusIdRequest actualRequest = requests.get(0);
        assertThat(actualRequest.getLocusId()).isEqualTo(locusId);
        assertThat(actualRequest.getFlags()).isEqualTo(NO_FLAGS);
    }

    @Test
    public void testUserDataRemovalRequest_manyIds() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        final LocusId locusId1 = new LocusId("com.example");
        final LocusId locusId2 = new LocusId("com.example2");

        LoginActivity.onRootView((activity, rootView) -> activity.getContentCaptureManager()
                .removeData(new DataRemovalRequest.Builder()
                        .addLocusId(locusId1, NO_FLAGS)
                        .addLocusId(locusId2, FLAG_IS_PREFIX)
                        .build()));

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final DataRemovalRequest request = service.getRemovalRequest();
        assertThat(request).isNotNull();
        assertThat(request.isForEverything()).isFalse();
        assertThat(request.getPackageName()).isEqualTo(MY_PACKAGE);

        final List<LocusIdRequest> requests = request.getLocusIdRequests();
        assertThat(requests.size()).isEqualTo(2);

        final LocusIdRequest actualRequest1 = requests.get(0);
        assertThat(actualRequest1.getLocusId()).isEqualTo(locusId1);
        assertThat(actualRequest1.getFlags()).isEqualTo(NO_FLAGS);

        final LocusIdRequest actualRequest2 = requests.get(1);
        assertThat(actualRequest2.getLocusId()).isEqualTo(locusId2);
        assertThat(actualRequest2.getFlags()).isEqualTo(FLAG_IS_PREFIX);
    }

    @Test
    public void testAddChildren_rightAway() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();
        final View[] children = new View[2];

        final DoubleVisitor<AbstractRootViewActivity, LinearLayout> visitor = (activity,
                rootView) -> {
            final TextView child1 = newImportantView(activity, "c1");
            children[0] = child1;
            Log.v(TAG, "Adding child1(" + child1.getAutofillId() + "): " + child1);
            rootView.addView(child1);
            final TextView child2 = newImportantView(activity, "c1");
            children[1] = child2;
            Log.v(TAG, "Adding child2(" + child2.getAutofillId() + "): " + child2);
            rootView.addView(child2);
        };
        LoginActivity.onRootView(visitor);

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        Log.v(TAG, "session id: " + session.id);

        final ContentCaptureSessionId sessionId = session.id;
        assertRightActivity(session, sessionId, activity);

        final List<ContentCaptureEvent> events = activity.assertJustInitialViewsAppeared(session,
                /* additionalEvents= */ 2);
        final AutofillId rootId = activity.getRootView().getAutofillId();
        int i = LoginActivity.MIN_EVENTS - 1;
        assertViewAppeared(events, i, sessionId, children[0], rootId);
        assertViewAppeared(events, i + 1, sessionId, children[1], rootId);
        assertViewTreeFinished(events, i + 2);

        activity.assertInitialViewsDisappeared(events, children.length);
    }

    @Test
    public void testAddChildren_afterAnimation() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();
        final View[] children = new View[2];

        final DoubleVisitor<AbstractRootViewActivity, LinearLayout> visitor = (activity,
                rootView) -> {
            final TextView child1 = newImportantView(activity, "c1");
            children[0] = child1;
            Log.v(TAG, "Adding child1(" + child1.getAutofillId() + "): " + child1);
            rootView.addView(child1);
            final TextView child2 = newImportantView(activity, "c1");
            children[1] = child2;
            Log.v(TAG, "Adding child2(" + child2.getAutofillId() + "): " + child2);
            rootView.addView(child2);
        };
        LoginActivity.onAnimationComplete(visitor);

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        Log.v(TAG, "session id: " + session.id);

        final ContentCaptureSessionId sessionId = session.id;
        assertRightActivity(session, sessionId, activity);
        final int additionalEvents = 2; // 2 children views
        final List<ContentCaptureEvent> events = activity.assertJustInitialViewsAppeared(session,
                additionalEvents);
        assertThat(events.size()).isAtLeast(LoginActivity.MIN_EVENTS + 5);
        final View decorView = activity.getDecorView();
        final View grandpa1 = activity.getGrandParent();
        final View grandpa2 = activity.getGrandGrandParent();
        final AutofillId rootId = activity.getRootView().getAutofillId();
        int i = LoginActivity.MIN_EVENTS - 1;

        assertViewTreeFinished(events, i);
        assertViewTreeStarted(events, i + 1);
        assertViewAppeared(events, i + 2, sessionId, children[0], rootId);
        assertViewAppeared(events, i + 3, sessionId, children[1], rootId);
        assertViewTreeFinished(events, i + 4);

        // TODO(b/122315042): assert parents disappeared
        if (true) return;

        // TODO(b/122315042): sometimes we get decor view disappareared events, sometimes we don't
        // As we don't really care about those, let's fix it!
        try {
            assertViewsOptionallyDisappeared(events, LoginActivity.MIN_EVENTS + additionalEvents,
                    rootId,
                    grandpa1.getAutofillId(), grandpa2.getAutofillId(),
                    activity.mUsernameLabel.getAutofillId(), activity.mUsername.getAutofillId(),
                    activity.mPasswordLabel.getAutofillId(), activity.mPassword.getAutofillId(),
                    children[0].getAutofillId(), children[1].getAutofillId());
        } catch (AssertionError e) {
            Log.e(TAG, "Hack-ignoring assertion without decor view: " + e);
            // Try again removing it...
            assertViewsOptionallyDisappeared(events, LoginActivity.MIN_EVENTS + additionalEvents,
                    rootId,
                    grandpa1.getAutofillId(), grandpa2.getAutofillId(),
                    decorView.getAutofillId(),
                    activity.mUsernameLabel.getAutofillId(), activity.mUsername.getAutofillId(),
                    activity.mPasswordLabel.getAutofillId(), activity.mPassword.getAutofillId(),
                    children[0].getAutofillId(), children[1].getAutofillId());

        }
    }

    @Test
    public void testWhitelist_packageNotWhitelisted() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        service.setContentCaptureWhitelist((Set) null, (Set) null);

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        assertThat(service.getAllSessionIds()).isEmpty();
    }

    @Test
    public void testWhitelist_activityNotWhitelisted() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ArraySet<ComponentName> components = new ArraySet<>();
        components.add(new ComponentName(MY_PACKAGE, "some.activity"));
        service.setContentCaptureWhitelist(null, components);
        final ActivityWatcher watcher = startWatcher();

        final LoginActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        assertThat(service.getAllSessionIds()).isEmpty();
    }

    /**
     * Creates a context that can be assert by
     * {@link #assertContentCaptureContext(ContentCaptureContext)}.
     */
    private ContentCaptureContext newContentCaptureContext() {
        final String id = "file://dev/null";
        final Bundle bundle = new Bundle();
        bundle.putString("DUDE", "SWEET");
        return new ContentCaptureContext.Builder(new LocusId(id)).setExtras(bundle).build();
    }

    /**
     * Asserts a context that can has been created by {@link #newContentCaptureContext()}.
     */
    private void assertContentCaptureContext(@NonNull ContentCaptureContext context) {
        assertWithMessage("null context").that(context).isNotNull();
        assertWithMessage("wrong ID on context %s", context).that(context.getLocusId().getId())
                .isEqualTo("file://dev/null");
        final Bundle extras = context.getExtras();
        assertWithMessage("no extras on context %s", context).that(extras).isNotNull();
        assertWithMessage("wrong number of extras on context %s", context).that(extras.size())
                .isEqualTo(1);
        assertWithMessage("wrong extras on context %s", context).that(extras.getString("DUDE"))
                .isEqualTo("SWEET");
    }

    // TODO(b/123540602): add moar test cases for different sessions:
    // - session1 on rootView, session2 on children
    // - session1 on rootView, session2 on child1, session3 on child2
    // - combination above where the CTS test explicitly finishes a session

    // TODO(b/123540602): add moar test cases for different scenarios, like:
    // - dynamically adding /
    // - removing views
    // - pausing / resuming activity / tapping home
    // - changing text
    // - secure flag with child sessions
    // - making sure events are flushed when activity pause / resume

    // TODO(b/126262658): moar lifecycle events, like multiple activities.

}
