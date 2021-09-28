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

import static android.apppredictionservice.cts.PredictionService.EXTRA_REPORTER;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.prediction.AppPredictionContext;
import android.app.prediction.AppPredictionManager;
import android.app.prediction.AppPredictor;
import android.app.prediction.AppTarget;
import android.app.prediction.AppTargetEvent;
import android.app.prediction.AppTargetId;
import android.apppredictionservice.cts.ServiceReporter.RequestVerifier;
import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserHandle;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.RequiredServiceRule;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Executors;

/**
 * atest CtsAppPredictionServiceTestCases:AppPredictionServiceTest
 */
@RunWith(AndroidJUnit4.class)
public class AppPredictionServiceTest {

    private static final String TAG = "AppPredictionServiceTest";

    private static final String APP_PREDICTION_SERVICE = "app_prediction";

    private static final String TEST_UI_SURFACE = "testSysUiSurface";
    private static final int TEST_NUM_PREDICTIONS = 10;
    private static final String TEST_LAUNCH_LOCATION = "testCollapsedLocation";
    private static final int TEST_ACTION = 2;

    @Rule
    public final RequiredServiceRule mRequiredServiceRule =
            new RequiredServiceRule(APP_PREDICTION_SERVICE);

    private ServiceReporter mReporter;
    private Bundle mPredictionContextExtras;

    @Before
    public void setUp() throws Exception {
        // Enable the prediction service
        setService(PredictionService.SERVICE_NAME);
        mReporter = new ServiceReporter();
        mPredictionContextExtras = new Bundle();
        mPredictionContextExtras.putBinder(EXTRA_REPORTER, mReporter);
    }

    @After
    public void tearDown() throws Exception {
        // Reset the prediction service
        setService(null);
        mReporter = null;
        SystemClock.sleep(1000);
    }

    @Test
    public void testCreateDestroySession() {
        AppPredictionContext context =  createTestPredictionContext();
        AppPredictor client = createTestPredictor(context);

        // Wait for the service to bind and the session to be created
        mReporter.awaitOnCreatePredictionSession();
        mReporter.assertActiveSession(client.getSessionId());
        assertEquals(mReporter.getPredictionContext(client.getSessionId()), context);

        // Create another session, and ensure that the session ids differ
        AppPredictionContext context2 =  createTestPredictionContext();
        AppPredictor client2 = createTestPredictor(context2);

        mReporter.awaitOnCreatePredictionSession();
        mReporter.assertActiveSession(client2.getSessionId());
        assertEquals(mReporter.getPredictionContext(client2.getSessionId()), context2);
        assertNotEquals(client.getSessionId(), client2.getSessionId());

        // Destroy both sessions
        client.destroy();
        mReporter.awaitOnDestroyPredictionSession();
        client2.destroy();
        mReporter.awaitOnDestroyPredictionSession();

        // Ensure that the session are no longer active
        assertFails(() -> mReporter.assertActiveSession(client.getSessionId()));
        assertFails(() -> mReporter.assertActiveSession(client2.getSessionId()));

        // Ensure that future calls to the client fail
        assertFails(() -> client.notifyAppTargetEvent(null));
        assertFails(() -> client.notifyLaunchLocationShown(null, null));
        assertFails(() -> client.registerPredictionUpdates(null, null));
        assertFails(() -> client.unregisterPredictionUpdates(null));
        assertFails(() -> client.requestPredictionUpdate());
        assertFails(() -> client.sortTargets(null, null, null));
        assertFails(() -> client.destroy());
    }

    @Test
    public void testRegisterPredictionUpdatesLifecycle() {
        AppPredictionContext context = createTestPredictionContext();
        AppPredictor client = createTestPredictor(context);

        RequestVerifier cb = new RequestVerifier(mReporter);
        client.registerPredictionUpdates(Executors.newSingleThreadExecutor(), cb);

        // Verify some updates
        assertTrue(cb.requestAndWaitForTargets(createPredictions(),
                () -> client.requestPredictionUpdate()));
        assertTrue(cb.requestAndWaitForTargets(createPredictions(),
                () -> client.requestPredictionUpdate()));
        assertTrue(cb.requestAndWaitForTargets(createPredictions(),
                () -> client.requestPredictionUpdate()));

        client.unregisterPredictionUpdates(cb);

        // Ensure we don't get updates after the listeners are unregistered
        assertFalse(cb.requestAndWaitForTargets(createPredictions(),
                () -> client.requestPredictionUpdate()));

        // Clients must be destroyed at end of test.
        client.destroy();
    }

    @Test
    public void testAppTargetEvent() {
        AppPredictionContext context = createTestPredictionContext();
        AppPredictor client = createTestPredictor(context);

        List<AppTarget> targets = createPredictions();
        List<AppTargetEvent> events = new ArrayList<>();
        for (AppTarget target : targets) {
            AppTargetEvent event = new AppTargetEvent.Builder(target, TEST_ACTION)
                    .setLaunchLocation(TEST_LAUNCH_LOCATION)
                    .build();
            events.add(event);
            client.notifyAppTargetEvent(event);
            mReporter.awaitOnAppTargetEvent();
        }
        assertEquals(mReporter.mEvents, events);

        // Clients must be destroyed at end of test.
        client.destroy();
    }

    @Test
    public void testNotifyLocationShown() {
        AppPredictionContext context = createTestPredictionContext();
        AppPredictor client = createTestPredictor(context);

        List<AppTarget> targets = createPredictions();
        List<AppTargetId> targetIds = new ArrayList<>();
        for (AppTarget target : targets) {
            AppTargetId id = target.getId();
            targetIds.add(id);
        }
        client.notifyLaunchLocationShown(TEST_LAUNCH_LOCATION, targetIds);
        mReporter.awaitOnLocationShown();
        assertEquals(mReporter.mLocationsShown, TEST_LAUNCH_LOCATION);
        assertEquals(mReporter.mLocationsShownTargets, targetIds);

        // Clients must be destroyed at end of test.
        client.destroy();
    }

    @Test
    public void testSortTargets() {
        AppPredictionContext context = createTestPredictionContext();
        AppPredictor client = createTestPredictor(context);

        List<AppTarget> sortedTargets = createPredictions();
        List<AppTarget> shuffledTargets = new ArrayList<>(sortedTargets);
        Collections.shuffle(shuffledTargets);

        // We call sortTargets below with the shuffled targets, ensure that the service receives the
        // shuffled targets, and return the sorted targets to the RequestVerifier below
        mReporter.setSortedPredictionsProvider((targets) -> {
            assertEquals(targets, shuffledTargets);
            return sortedTargets;
        });
        RequestVerifier cb = new RequestVerifier(mReporter);
        assertTrue(cb.requestAndWaitForTargets(sortedTargets,
                () -> client.sortTargets(shuffledTargets,
                        Executors.newSingleThreadExecutor(), cb)));

        // Clients must be destroyed at end of test.
        client.destroy();
    }

    @Test
    public void testFailureWithoutPermission() {
        setService(null);
        InstrumentationRegistry.getInstrumentation().getUiAutomation().revokeRuntimePermission(
                InstrumentationRegistry.getTargetContext().getPackageName(),
                android.Manifest.permission.PACKAGE_USAGE_STATS);
        assertFails(() -> createTestPredictor(createTestPredictionContext()));
    }

    @Test
    public void testSuccessWithPermission() {
        setService(null);
        InstrumentationRegistry.getInstrumentation().getUiAutomation().grantRuntimePermission(
                InstrumentationRegistry.getTargetContext().getPackageName(),
                android.Manifest.permission.PACKAGE_USAGE_STATS);
        AppPredictor predictor = createTestPredictor(createTestPredictionContext());
        predictor.destroy();
    }

    private void assertFails(Runnable r) {
        try {
            r.run();
        } catch (Exception|Error e) {
            // Expected failure
            return;
        }
        fail("Expected failure");
    }

    /** Creates a random number of targets by increasing id */
    private List<AppTarget> createPredictions() {
        List<AppTarget> targets = new ArrayList<>();
        int n = (int) (Math.random() * 20);
        for (int i = 0; i < n; i++) {
            targets.add(new AppTarget.Builder(new AppTargetId(String.valueOf(i)), "test.pkg",
                    UserHandle.CURRENT)
                    .setClassName("test.class." + i)
                    .build());
        }
        return targets;
    }

    private AppPredictionContext createTestPredictionContext() {
        return new AppPredictionContext.Builder(InstrumentationRegistry.getTargetContext())
                .setExtras(mPredictionContextExtras)
                .setUiSurface(TEST_UI_SURFACE)
                .setPredictedTargetCount(TEST_NUM_PREDICTIONS)
                .build();
    }

    private AppPredictor createTestPredictor(AppPredictionContext context) {
        Context ctx = InstrumentationRegistry.getTargetContext();
        AppPredictionManager mgr = (AppPredictionManager) ctx.getSystemService(
                APP_PREDICTION_SERVICE);
        return mgr.createAppPredictionSession(context);
    }

    private void setService(String service) {
        Log.d(TAG, "Setting app prediction service to " + service);
        int userId = android.os.Process.myUserHandle().getIdentifier();
        if (service != null) {
            runShellCommand("cmd app_prediction set temporary-service "
                    + userId + " " + service + " 12000");
        } else {
            runShellCommand("cmd app_prediction set temporary-service " + userId);
        }
    }

    private void runShellCommand(String command) {
        Log.d(TAG, "runShellCommand(): " + command);
        try {
            SystemUtil.runShellCommand(InstrumentationRegistry.getInstrumentation(), command);
        } catch (Exception e) {
            throw new RuntimeException("Command '" + command + "' failed: ", e);
        }
    }
}
