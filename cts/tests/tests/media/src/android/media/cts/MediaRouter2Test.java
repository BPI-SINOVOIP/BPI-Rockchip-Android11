/*
 * Copyright 2019 The Android Open Source Project
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

package android.media.cts;

import static android.content.Context.AUDIO_SERVICE;
import static android.media.MediaRoute2Info.FEATURE_LIVE_AUDIO;
import static android.media.MediaRoute2Info.PLAYBACK_VOLUME_VARIABLE;
import static android.media.cts.StubMediaRoute2ProviderService.FEATURES_SPECIAL;
import static android.media.cts.StubMediaRoute2ProviderService.FEATURE_SAMPLE;
import static android.media.cts.StubMediaRoute2ProviderService.ROUTE_ID1;
import static android.media.cts.StubMediaRoute2ProviderService.ROUTE_ID2;
import static android.media.cts.StubMediaRoute2ProviderService.ROUTE_ID3_SESSION_CREATION_FAILED;
import static android.media.cts.StubMediaRoute2ProviderService.ROUTE_ID4_TO_SELECT_AND_DESELECT;
import static android.media.cts.StubMediaRoute2ProviderService.ROUTE_ID5_TO_TRANSFER_TO;
import static android.media.cts.StubMediaRoute2ProviderService.ROUTE_ID_SPECIAL_FEATURE;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.testng.Assert.assertThrows;

import android.annotation.NonNull;
import android.content.Context;
import android.media.AudioManager;
import android.media.MediaRoute2Info;
import android.media.MediaRouter2;
import android.media.MediaRouter2.ControllerCallback;
import android.media.MediaRouter2.OnGetControllerHintsListener;
import android.media.MediaRouter2.RouteCallback;
import android.media.MediaRouter2.RoutingController;
import android.media.MediaRouter2.TransferCallback;
import android.media.RouteDiscoveryPreference;
import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.LargeTest;
import android.text.TextUtils;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PollingCheck;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
@AppModeFull(reason = "The system should be able to bind to StubMediaRoute2ProviderService")
@LargeTest
@NonMediaMainlineTest
public class MediaRouter2Test {
    private static final String TAG = "MR2Test";
    Context mContext;
    private MediaRouter2 mRouter2;
    private Executor mExecutor;
    private AudioManager mAudioManager;
    private RouteCallback mRouterDummyCallback = new RouteCallback(){};
    private StubMediaRoute2ProviderService mService;

    private static final int TIMEOUT_MS = 5000;
    private static final int WAIT_MS = 2000;

    private static final String TEST_KEY = "test_key";
    private static final String TEST_VALUE = "test_value";
    private static final RouteDiscoveryPreference EMPTY_DISCOVERY_PREFERENCE =
            new RouteDiscoveryPreference.Builder(Collections.emptyList(), false).build();
    private static final RouteDiscoveryPreference LIVE_AUDIO_DISCOVERY_PREFERENCE =
            new RouteDiscoveryPreference.Builder(
                    Collections.singletonList(FEATURE_LIVE_AUDIO), false).build();

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mRouter2 = MediaRouter2.getInstance(mContext);
        mExecutor = Executors.newSingleThreadExecutor();
        mAudioManager = (AudioManager) mContext.getSystemService(AUDIO_SERVICE);

        MediaRouter2TestActivity.startActivity(mContext);

        // In order to make the system bind to the test service,
        // set a non-empty discovery preference while app is in foreground.
        List<String> features = new ArrayList<>();
        features.add("A test feature");
        RouteDiscoveryPreference preference =
                new RouteDiscoveryPreference.Builder(features, false).build();
        mRouter2.registerRouteCallback(mExecutor, mRouterDummyCallback, preference);

        new PollingCheck(TIMEOUT_MS) {
            @Override
            protected boolean check() {
                StubMediaRoute2ProviderService service =
                        StubMediaRoute2ProviderService.getInstance();
                if (service != null) {
                    mService = service;
                    return true;
                }
                return false;
            }
        }.run();
        mService.initializeRoutes();
        mService.publishRoutes();
    }

    @After
    public void tearDown() throws Exception {
        mRouter2.unregisterRouteCallback(mRouterDummyCallback);
        MediaRouter2TestActivity.finishActivity();
        if (mService != null) {
            mService.clear();
            mService = null;
        }
    }

    @Test
    public void testGetRoutesAfterCreation() {
        RouteCallback routeCallback = new RouteCallback() {};
        mRouter2.registerRouteCallback(mExecutor, routeCallback, LIVE_AUDIO_DISCOVERY_PREFERENCE);
        try {
            List<MediaRoute2Info> initialRoutes = mRouter2.getRoutes();
            assertFalse(initialRoutes.isEmpty());
            for (MediaRoute2Info route : initialRoutes) {
                assertTrue(route.getFeatures().contains(FEATURE_LIVE_AUDIO));
            }
        } finally {
            mRouter2.unregisterRouteCallback(routeCallback);
        }
    }

    /**
     * Tests if we get proper routes for application that has special route type.
     */
    @Test
    public void testGetRoutes() throws Exception {
        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(FEATURES_SPECIAL);

        int remoteRouteCount = 0;
        for (MediaRoute2Info route : routes.values()) {
            if (!route.isSystemRoute()) {
                remoteRouteCount++;
            }
        }

        assertEquals(1, remoteRouteCount);
        assertNotNull(routes.get(ROUTE_ID_SPECIAL_FEATURE));
    }

    @Test
    public void testRegisterTransferCallbackWithInvalidArguments() {
        Executor executor = mExecutor;
        TransferCallback callback = new TransferCallback() {};

        // Tests null executor
        assertThrows(NullPointerException.class,
                () -> mRouter2.registerTransferCallback(null, callback));

        // Tests null callback
        assertThrows(NullPointerException.class,
                () -> mRouter2.registerTransferCallback(executor, null));
    }

    @Test
    public void testUnregisterTransferCallbackWithNullCallback() {
        // Tests null callback
        assertThrows(NullPointerException.class,
                () -> mRouter2.unregisterTransferCallback(null));
    }

    @Test
    public void testTransferToSuccess() throws Exception {
        final List<String> sampleRouteFeature = new ArrayList<>();
        sampleRouteFeature.add(FEATURE_SAMPLE);

        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(sampleRouteFeature);
        MediaRoute2Info route = routes.get(ROUTE_ID1);
        assertNotNull(route);

        final CountDownLatch successLatch = new CountDownLatch(1);
        final CountDownLatch failureLatch = new CountDownLatch(1);
        final List<RoutingController> controllers = new ArrayList<>();

        // Create session with this route
        TransferCallback controllerCallback = new TransferCallback() {
            @Override
            public void onTransfer(RoutingController oldController,
                    RoutingController newController) {
                assertEquals(mRouter2.getSystemController(), oldController);
                assertTrue(createRouteMap(newController.getSelectedRoutes()).containsKey(
                        ROUTE_ID1));
                controllers.add(newController);
                successLatch.countDown();
            }

            @Override
            public void onTransferFailure(MediaRoute2Info requestedRoute) {
                failureLatch.countDown();
            }
        };

        // TODO: Remove this once the MediaRouter2 becomes always connected to the service.
        RouteCallback routeCallback = new RouteCallback() {};
        mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);

        try {
            mRouter2.registerTransferCallback(mExecutor, controllerCallback);
            mRouter2.transferTo(route);
            assertTrue(successLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));

            // onSessionCreationFailed should not be called.
            assertFalse(failureLatch.await(WAIT_MS, TimeUnit.MILLISECONDS));
        } finally {
            releaseControllers(controllers);
            mRouter2.unregisterRouteCallback(routeCallback);
            mRouter2.unregisterTransferCallback(controllerCallback);
        }
    }

    @Test
    public void testTransferToFailure() throws Exception {
        final List<String> sampleRouteType = new ArrayList<>();
        sampleRouteType.add(FEATURE_SAMPLE);

        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(sampleRouteType);
        MediaRoute2Info route = routes.get(ROUTE_ID3_SESSION_CREATION_FAILED);
        assertNotNull(route);

        final CountDownLatch successLatch = new CountDownLatch(1);
        final CountDownLatch failureLatch = new CountDownLatch(1);
        final List<RoutingController> controllers = new ArrayList<>();

        // Create session with this route
        TransferCallback transferCallback = new TransferCallback() {
            @Override
            public void onTransfer(RoutingController oldController,
                    RoutingController newController) {
                controllers.add(newController);
                successLatch.countDown();
            }

            @Override
            public void onTransferFailure(MediaRoute2Info requestedRoute) {
                assertEquals(route, requestedRoute);
                failureLatch.countDown();
            }
        };

        // TODO: Remove this once the MediaRouter2 becomes always connected to the service.
        RouteCallback routeCallback = new RouteCallback() {};
        mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);

        try {
            mRouter2.registerTransferCallback(mExecutor, transferCallback);
            mRouter2.transferTo(route);
            assertTrue(failureLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));

            // onTransfer should not be called.
            assertFalse(successLatch.await(WAIT_MS, TimeUnit.MILLISECONDS));
        } finally {
            releaseControllers(controllers);
            mRouter2.unregisterRouteCallback(routeCallback);
            mRouter2.unregisterTransferCallback(transferCallback);
        }
    }

    @Test
    public void testTransferToTwice() throws Exception {
        final List<String> sampleRouteType = new ArrayList<>();
        sampleRouteType.add(FEATURE_SAMPLE);

        final CountDownLatch successLatch1 = new CountDownLatch(1);
        final CountDownLatch successLatch2 = new CountDownLatch(1);
        final CountDownLatch failureLatch = new CountDownLatch(1);
        final CountDownLatch stopLatch = new CountDownLatch(1);
        final CountDownLatch onReleaseSessionLatch = new CountDownLatch(1);

        final List<RoutingController> createdControllers = new ArrayList<>();

        // Create session with this route
        TransferCallback transferCallback = new TransferCallback() {
            @Override
            public void onTransfer(RoutingController oldController,
                    RoutingController newController) {
                createdControllers.add(newController);
                if (successLatch1.getCount() > 0) {
                    successLatch1.countDown();
                } else {
                    successLatch2.countDown();
                }
            }

            @Override
            public void onTransferFailure(MediaRoute2Info requestedRoute) {
                failureLatch.countDown();
            }

            @Override
            public void onStop(RoutingController controller) {
                stopLatch.countDown();
            }
        };

        StubMediaRoute2ProviderService service = mService;
        if (service != null) {
            service.setProxy(new StubMediaRoute2ProviderService.Proxy() {
                @Override
                public void onReleaseSession(long requestId, String sessionId) {
                    onReleaseSessionLatch.countDown();
                }
            });
        }

        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(sampleRouteType);
        MediaRoute2Info route1 = routes.get(ROUTE_ID1);
        MediaRoute2Info route2 = routes.get(ROUTE_ID2);
        assertNotNull(route1);
        assertNotNull(route2);

        // TODO: Remove this once the MediaRouter2 becomes always connected to the service.
        RouteCallback routeCallback = new RouteCallback() {};
        mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);

        try {
            mRouter2.registerTransferCallback(mExecutor, transferCallback);
            mRouter2.transferTo(route1);
            assertTrue(successLatch1.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
            mRouter2.transferTo(route2);
            assertTrue(successLatch2.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));

            // onTransferFailure/onStop should not be called.
            assertFalse(failureLatch.await(WAIT_MS, TimeUnit.MILLISECONDS));
            assertFalse(stopLatch.await(WAIT_MS, TimeUnit.MILLISECONDS));

            // Created controllers should have proper info
            assertEquals(2, createdControllers.size());
            RoutingController controller1 = createdControllers.get(0);
            RoutingController controller2 = createdControllers.get(1);

            assertNotEquals(controller1.getId(), controller2.getId());
            assertTrue(createRouteMap(controller1.getSelectedRoutes()).containsKey(
                    ROUTE_ID1));
            assertTrue(createRouteMap(controller2.getSelectedRoutes()).containsKey(
                    ROUTE_ID2));

            // Transferred controllers shouldn't be obtainable.
            assertFalse(mRouter2.getControllers().contains(controller1));
            assertTrue(mRouter2.getControllers().contains(controller2));

            // Should be able to release transferred controllers.
            controller1.release();
            assertTrue(onReleaseSessionLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } finally {
            releaseControllers(createdControllers);
            mRouter2.unregisterRouteCallback(routeCallback);
            mRouter2.unregisterTransferCallback(transferCallback);
        }
    }

    @Test
    public void testSetOnGetControllerHintsListener() throws Exception {
        final List<String> sampleRouteFeature = new ArrayList<>();
        sampleRouteFeature.add(FEATURE_SAMPLE);

        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(sampleRouteFeature);
        MediaRoute2Info route = routes.get(ROUTE_ID1);
        assertNotNull(route);

        final Bundle controllerHints = new Bundle();
        controllerHints.putString(TEST_KEY, TEST_VALUE);
        final OnGetControllerHintsListener listener = route1 -> controllerHints;

        final CountDownLatch successLatch = new CountDownLatch(1);
        final CountDownLatch failureLatch = new CountDownLatch(1);
        final List<RoutingController> controllers = new ArrayList<>();

        // Create session with this route
        TransferCallback transferCallback = new TransferCallback() {
            @Override
            public void onTransfer(RoutingController oldController,
                    RoutingController newController) {
                assertTrue(createRouteMap(newController.getSelectedRoutes())
                        .containsKey(ROUTE_ID1));

                // The StubMediaRoute2ProviderService is supposed to set control hints
                // with the given controllerHints.
                Bundle controlHints = newController.getControlHints();
                assertNotNull(controlHints);
                assertTrue(controlHints.containsKey(TEST_KEY));
                assertEquals(TEST_VALUE, controlHints.getString(TEST_KEY));

                controllers.add(newController);
                successLatch.countDown();
            }

            @Override
            public void onTransferFailure(MediaRoute2Info requestedRoute) {
                failureLatch.countDown();
            }
        };

        // TODO: Remove this once the MediaRouter2 becomes always connected to the service.
        RouteCallback routeCallback = new RouteCallback() {};
        mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);

        try {
            mRouter2.registerTransferCallback(mExecutor, transferCallback);

            // The StubMediaRoute2ProviderService supposed to set control hints
            // with the given creationSessionHints.
            mRouter2.setOnGetControllerHintsListener(listener);
            mRouter2.transferTo(route);
            assertTrue(successLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));

            // onSessionCreationFailed should not be called.
            assertFalse(failureLatch.await(WAIT_MS, TimeUnit.MILLISECONDS));
        } finally {
            releaseControllers(controllers);
            mRouter2.unregisterRouteCallback(routeCallback);
            mRouter2.unregisterTransferCallback(transferCallback);
        }
    }

    @Test
    public void testSetSessionVolume() throws Exception {
        List<String> sampleRouteFeature = new ArrayList<>();
        sampleRouteFeature.add(FEATURE_SAMPLE);

        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(sampleRouteFeature);
        MediaRoute2Info route = routes.get(ROUTE_ID1);
        assertNotNull(route);

        CountDownLatch successLatch = new CountDownLatch(1);
        CountDownLatch volumeChangedLatch = new CountDownLatch(1);

        List<RoutingController> controllers = new ArrayList<>();

        // Create session with this route
        TransferCallback transferCallback = new TransferCallback() {
            @Override
            public void onTransfer(RoutingController oldController,
                    RoutingController newController) {
                controllers.add(newController);
                successLatch.countDown();
            }
        };

        // TODO: Remove this once the MediaRouter2 becomes always connected to the service.
        RouteCallback routeCallback = new RouteCallback() {};

        try {
            mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);
            mRouter2.registerTransferCallback(mExecutor, transferCallback);
            mRouter2.transferTo(route);

            assertTrue(successLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } finally {
            mRouter2.unregisterTransferCallback(transferCallback);
            mRouter2.unregisterRouteCallback(routeCallback);
        }

        assertEquals(1, controllers.size());
        // test requestSetSessionVolume

        RoutingController targetController = controllers.get(0);
        assertEquals(PLAYBACK_VOLUME_VARIABLE, targetController.getVolumeHandling());
        int currentVolume = targetController.getVolume();
        int maxVolume = targetController.getVolumeMax();
        int targetVolume = (currentVolume == maxVolume) ? currentVolume - 1 : (currentVolume + 1);

        ControllerCallback controllerCallback = new ControllerCallback() {
            @Override
            public void onControllerUpdated(MediaRouter2.RoutingController controller) {
                if (!TextUtils.equals(targetController.getId(), controller.getId())) {
                    return;
                }
                if (controller.getVolume() == targetVolume) {
                    volumeChangedLatch.countDown();
                }
            }
        };

        try {
            mRouter2.registerControllerCallback(mExecutor, controllerCallback);
            mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);
            targetController.setVolume(targetVolume);
            assertTrue(volumeChangedLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } finally {
            releaseControllers(controllers);
            mRouter2.unregisterRouteCallback(routeCallback);
            mRouter2.unregisterControllerCallback(controllerCallback);
        }
    }

    @Test
    public void testTransferCallbackIsNotCalledAfterUnregistered() throws Exception {
        final List<String> sampleRouteType = new ArrayList<>();
        sampleRouteType.add(FEATURE_SAMPLE);

        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(sampleRouteType);
        MediaRoute2Info route = routes.get(ROUTE_ID1);
        assertNotNull(route);

        final CountDownLatch successLatch = new CountDownLatch(1);
        final CountDownLatch failureLatch = new CountDownLatch(1);
        final List<RoutingController> controllers = new ArrayList<>();

        // Create session with this route
        TransferCallback transferCallback = new TransferCallback() {
            @Override
            public void onTransfer(RoutingController oldController,
                    RoutingController newController) {
                controllers.add(newController);
                successLatch.countDown();
            }

            @Override
            public void onTransferFailure(MediaRoute2Info requestedRoute) {
                failureLatch.countDown();
            }
        };

        // TODO: Remove this once the MediaRouter2 becomes always connected to the service.
        RouteCallback routeCallback = new RouteCallback() {};
        mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);

        try {
            mRouter2.registerTransferCallback(mExecutor, transferCallback);
            mRouter2.transferTo(route);

            // Unregisters transfer callback
            mRouter2.unregisterTransferCallback(transferCallback);

            // No transfer callback methods should be called.
            assertFalse(successLatch.await(WAIT_MS, TimeUnit.MILLISECONDS));
            assertFalse(failureLatch.await(WAIT_MS, TimeUnit.MILLISECONDS));
        } finally {
            releaseControllers(controllers);
            mRouter2.unregisterRouteCallback(routeCallback);
            mRouter2.unregisterTransferCallback(transferCallback);
        }
    }

    // TODO: Add tests for illegal inputs if needed (e.g. selecting already selected route)
    @Test
    public void testRoutingControllerSelectAndDeselectRoute() throws Exception {
        final List<String> sampleRouteType = new ArrayList<>();
        sampleRouteType.add(FEATURE_SAMPLE);

        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(sampleRouteType);
        MediaRoute2Info routeToBegin = routes.get(ROUTE_ID1);
        assertNotNull(routeToBegin);

        final CountDownLatch onTransferLatch = new CountDownLatch(1);
        final CountDownLatch onControllerUpdatedLatchForSelect = new CountDownLatch(1);
        final CountDownLatch onControllerUpdatedLatchForDeselect = new CountDownLatch(1);
        final List<RoutingController> controllers = new ArrayList<>();


        // Create session with ROUTE_ID1
        TransferCallback transferCallback = new TransferCallback() {
            @Override
            public void onTransfer(RoutingController oldController,
                    RoutingController newController) {
                assertEquals(mRouter2.getSystemController(), oldController);
                assertTrue(getOriginalRouteIds(newController.getSelectedRoutes()).contains(
                        ROUTE_ID1));
                controllers.add(newController);
                onTransferLatch.countDown();
            }
        };

        ControllerCallback controllerCallback = new ControllerCallback() {
            @Override
            public void onControllerUpdated(RoutingController controller) {
                if (onTransferLatch.getCount() != 0
                        || !TextUtils.equals(controllers.get(0).getId(), controller.getId())) {
                    return;
                }

                if (onControllerUpdatedLatchForSelect.getCount() != 0) {
                    assertEquals(2, controller.getSelectedRoutes().size());
                    assertTrue(getOriginalRouteIds(controller.getSelectedRoutes())
                            .contains(ROUTE_ID1));
                    assertTrue(getOriginalRouteIds(controller.getSelectedRoutes())
                            .contains(ROUTE_ID4_TO_SELECT_AND_DESELECT));
                    assertFalse(getOriginalRouteIds(controller.getSelectableRoutes())
                            .contains(ROUTE_ID4_TO_SELECT_AND_DESELECT));
                    assertTrue(getOriginalRouteIds(controller.getDeselectableRoutes())
                            .contains(ROUTE_ID4_TO_SELECT_AND_DESELECT));

                    onControllerUpdatedLatchForSelect.countDown();
                } else {
                    assertEquals(1, controller.getSelectedRoutes().size());
                    assertTrue(getOriginalRouteIds(controller.getSelectedRoutes())
                            .contains(ROUTE_ID1));
                    assertFalse(getOriginalRouteIds(controller.getSelectedRoutes())
                            .contains(ROUTE_ID4_TO_SELECT_AND_DESELECT));
                    assertTrue(getOriginalRouteIds(controller.getSelectableRoutes())
                            .contains(ROUTE_ID4_TO_SELECT_AND_DESELECT));
                    assertFalse(getOriginalRouteIds(controller.getDeselectableRoutes())
                            .contains(ROUTE_ID4_TO_SELECT_AND_DESELECT));

                    onControllerUpdatedLatchForDeselect.countDown();
                }
            }
        };


        // TODO: Remove this once the MediaRouter2 becomes always connected to the service.
        RouteCallback routeCallback = new RouteCallback() {};
        mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);

        try {
            mRouter2.registerTransferCallback(mExecutor, transferCallback);
            mRouter2.registerControllerCallback(mExecutor, controllerCallback);
            mRouter2.transferTo(routeToBegin);
            assertTrue(onTransferLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));

            assertEquals(1, controllers.size());
            RoutingController controller = controllers.get(0);
            assertTrue(getOriginalRouteIds(controller.getSelectableRoutes())
                    .contains(ROUTE_ID4_TO_SELECT_AND_DESELECT));

            // Select ROUTE_ID4_TO_SELECT_AND_DESELECT
            MediaRoute2Info routeToSelectAndDeselect = routes.get(
                    ROUTE_ID4_TO_SELECT_AND_DESELECT);
            assertNotNull(routeToSelectAndDeselect);

            controller.selectRoute(routeToSelectAndDeselect);
            assertTrue(onControllerUpdatedLatchForSelect.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));

            controller.deselectRoute(routeToSelectAndDeselect);
            assertTrue(onControllerUpdatedLatchForDeselect.await(
                    TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } finally {
            releaseControllers(controllers);
            mRouter2.unregisterRouteCallback(routeCallback);
            mRouter2.unregisterTransferCallback(transferCallback);
            mRouter2.unregisterControllerCallback(controllerCallback);
        }
    }

    @Test
    public void testRoutingControllerTransferToRoute() throws Exception {
        final List<String> sampleRouteType = new ArrayList<>();
        sampleRouteType.add(FEATURE_SAMPLE);

        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(sampleRouteType);
        MediaRoute2Info routeToBegin = routes.get(ROUTE_ID1);
        assertNotNull(routeToBegin);

        final CountDownLatch onTransferLatch = new CountDownLatch(1);
        final CountDownLatch onControllerUpdatedLatch = new CountDownLatch(1);
        final List<RoutingController> controllers = new ArrayList<>();

        // Create session with ROUTE_ID1
        TransferCallback transferCallback = new TransferCallback() {
            @Override
            public void onTransfer(RoutingController oldController,
                    RoutingController newController) {
                assertEquals(mRouter2.getSystemController(), oldController);
                assertTrue(getOriginalRouteIds(newController.getSelectedRoutes()).contains(
                        ROUTE_ID1));
                controllers.add(newController);
                onTransferLatch.countDown();
            }
        };

        ControllerCallback controllerCallback = new ControllerCallback() {
            @Override
            public void onControllerUpdated(RoutingController controller) {
                if (onTransferLatch.getCount() != 0
                        || !TextUtils.equals(controllers.get(0).getId(), controller.getId())) {
                    return;
                }
                assertEquals(1, controller.getSelectedRoutes().size());
                assertFalse(getOriginalRouteIds(controller.getSelectedRoutes()).contains(
                        ROUTE_ID1));
                assertTrue(getOriginalRouteIds(controller.getSelectedRoutes())
                        .contains(ROUTE_ID5_TO_TRANSFER_TO));
                onControllerUpdatedLatch.countDown();
            }
        };

        // TODO: Remove this once the MediaRouter2 becomes always connected to the service.
        RouteCallback routeCallback = new RouteCallback() {};
        mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);

        try {
            mRouter2.registerTransferCallback(mExecutor, transferCallback);
            mRouter2.registerControllerCallback(mExecutor, controllerCallback);
            mRouter2.transferTo(routeToBegin);
            assertTrue(onTransferLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));

            assertEquals(1, controllers.size());
            RoutingController controller = controllers.get(0);

            // Transfer to ROUTE_ID5_TO_TRANSFER_TO
            MediaRoute2Info routeToTransferTo = routes.get(ROUTE_ID5_TO_TRANSFER_TO);
            assertNotNull(routeToTransferTo);

            mRouter2.transferTo(routeToTransferTo);
            assertTrue(onControllerUpdatedLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } finally {
            releaseControllers(controllers);
            mRouter2.unregisterRouteCallback(routeCallback);
            mRouter2.unregisterControllerCallback(controllerCallback);
            mRouter2.unregisterTransferCallback(transferCallback);
        }
    }

    @Test
    public void testControllerCallbackUnregister() throws Exception {
        final List<String> sampleRouteType = new ArrayList<>();
        sampleRouteType.add(FEATURE_SAMPLE);

        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(sampleRouteType);
        MediaRoute2Info routeToBegin = routes.get(ROUTE_ID1);
        assertNotNull(routeToBegin);

        final CountDownLatch onTransferLatch = new CountDownLatch(1);
        final CountDownLatch onControllerUpdatedLatch = new CountDownLatch(1);
        final List<RoutingController> controllers = new ArrayList<>();

        // Create session with ROUTE_ID1
        TransferCallback transferCallback = new TransferCallback() {
            @Override
            public void onTransfer(RoutingController oldController,
                    RoutingController newController) {
                assertEquals(mRouter2.getSystemController(), oldController);
                assertTrue(getOriginalRouteIds(newController.getSelectedRoutes()).contains(
                        ROUTE_ID1));
                controllers.add(newController);
                onTransferLatch.countDown();
            }
        };
        ControllerCallback controllerCallback = new ControllerCallback() {
            @Override
            public void onControllerUpdated(RoutingController controller) {
                if (onTransferLatch.getCount() != 0
                        || !TextUtils.equals(controllers.get(0).getId(), controller.getId())) {
                    return;
                }
                assertEquals(1, controller.getSelectedRoutes().size());
                assertFalse(getOriginalRouteIds(controller.getSelectedRoutes()).contains(
                        ROUTE_ID1));
                assertTrue(getOriginalRouteIds(controller.getSelectedRoutes())
                        .contains(ROUTE_ID5_TO_TRANSFER_TO));
                onControllerUpdatedLatch.countDown();
            }
        };

        // TODO: Remove this once the MediaRouter2 becomes always connected to the service.
        RouteCallback routeCallback = new RouteCallback() {};
        mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);

        try {
            mRouter2.registerTransferCallback(mExecutor, transferCallback);
            mRouter2.registerControllerCallback(mExecutor, controllerCallback);
            mRouter2.transferTo(routeToBegin);
            assertTrue(onTransferLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));

            assertEquals(1, controllers.size());

            // Transfer to ROUTE_ID5_TO_TRANSFER_TO
            MediaRoute2Info routeToTransferTo = routes.get(ROUTE_ID5_TO_TRANSFER_TO);
            assertNotNull(routeToTransferTo);

            mRouter2.unregisterControllerCallback(controllerCallback);
            mRouter2.transferTo(routeToTransferTo);
            assertFalse(onControllerUpdatedLatch.await(WAIT_MS, TimeUnit.MILLISECONDS));
        } finally {
            releaseControllers(controllers);
            mRouter2.unregisterRouteCallback(routeCallback);
            mRouter2.unregisterControllerCallback(controllerCallback);
            mRouter2.unregisterTransferCallback(transferCallback);
        }
    }

    // TODO: Add tests for onStop() when provider releases the session.
    @Test
    public void testStop() throws Exception {
        final List<String> sampleRouteType = new ArrayList<>();
        sampleRouteType.add(FEATURE_SAMPLE);

        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(sampleRouteType);
        MediaRoute2Info routeTransferFrom = routes.get(ROUTE_ID1);
        assertNotNull(routeTransferFrom);

        final CountDownLatch onTransferLatch = new CountDownLatch(1);
        final CountDownLatch onControllerUpdatedLatch = new CountDownLatch(1);
        final CountDownLatch onStopLatch = new CountDownLatch(1);
        final List<RoutingController> controllers = new ArrayList<>();

        TransferCallback transferCallback = new TransferCallback() {
            @Override
            public void onTransfer(RoutingController oldController,
                    RoutingController newController) {
                assertEquals(mRouter2.getSystemController(), oldController);
                assertTrue(getOriginalRouteIds(newController.getSelectedRoutes()).contains(
                        ROUTE_ID1));
                controllers.add(newController);
                onTransferLatch.countDown();
            }
            @Override
            public void onStop(RoutingController controller) {
                if (onTransferLatch.getCount() != 0
                        || !TextUtils.equals(
                        controllers.get(0).getId(), controller.getId())) {
                    return;
                }
                onStopLatch.countDown();
            }
        };

        ControllerCallback controllerCallback = new ControllerCallback() {
            @Override
            public void onControllerUpdated(RoutingController controller) {
                if (onTransferLatch.getCount() != 0
                        || !TextUtils.equals(controllers.get(0).getId(), controller.getId())) {
                    return;
                }
                onControllerUpdatedLatch.countDown();
            }
        };

        // TODO: Remove this once the MediaRouter2 becomes always connected to the service.
        RouteCallback routeCallback = new RouteCallback() {};
        mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);

        try {
            mRouter2.registerTransferCallback(mExecutor, transferCallback);
            mRouter2.registerControllerCallback(mExecutor, controllerCallback);
            mRouter2.transferTo(routeTransferFrom);
            assertTrue(onTransferLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));

            assertEquals(1, controllers.size());
            RoutingController controller = controllers.get(0);

            mRouter2.stop();

            // Select ROUTE_ID5_TO_TRANSFER_TO
            MediaRoute2Info routeToSelect = routes.get(ROUTE_ID4_TO_SELECT_AND_DESELECT);
            assertNotNull(routeToSelect);

            // This call should be ignored.
            // The onSessionInfoChanged() shouldn't be called.
            controller.selectRoute(routeToSelect);
            assertFalse(onControllerUpdatedLatch.await(WAIT_MS, TimeUnit.MILLISECONDS));

            // onStop should be called.
            assertTrue(onStopLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } finally {
            releaseControllers(controllers);
            mRouter2.unregisterRouteCallback(routeCallback);
            mRouter2.unregisterControllerCallback(controllerCallback);
            mRouter2.unregisterTransferCallback(transferCallback);
        }
    }

    @Test
    public void testRoutingControllerRelease() throws Exception {
        final List<String> sampleRouteType = new ArrayList<>();
        sampleRouteType.add(FEATURE_SAMPLE);

        Map<String, MediaRoute2Info> routes = waitAndGetRoutes(sampleRouteType);
        MediaRoute2Info routeTransferFrom = routes.get(ROUTE_ID1);
        assertNotNull(routeTransferFrom);

        final CountDownLatch onTransferLatch = new CountDownLatch(1);
        final CountDownLatch onControllerUpdatedLatch = new CountDownLatch(1);
        final CountDownLatch onStopLatch = new CountDownLatch(1);
        final List<RoutingController> controllers = new ArrayList<>();

        TransferCallback transferCallback = new TransferCallback() {
            @Override
            public void onTransfer(RoutingController oldController,
                    RoutingController newController) {
                assertEquals(mRouter2.getSystemController(), oldController);
                assertTrue(getOriginalRouteIds(newController.getSelectedRoutes()).contains(
                        ROUTE_ID1));
                controllers.add(newController);
                onTransferLatch.countDown();
            }
            @Override
            public void onStop(RoutingController controller) {
                if (onTransferLatch.getCount() != 0
                        || !TextUtils.equals(
                                controllers.get(0).getId(), controller.getId())) {
                    return;
                }
                onStopLatch.countDown();
            }
        };

        ControllerCallback controllerCallback = new ControllerCallback() {
            @Override
            public void onControllerUpdated(RoutingController controller) {
                if (onTransferLatch.getCount() != 0
                        || !TextUtils.equals(controllers.get(0).getId(), controller.getId())) {
                    return;
                }
                onControllerUpdatedLatch.countDown();
            }
        };

       // TODO: Remove this once the MediaRouter2 becomes always connected to the service.
        RouteCallback routeCallback = new RouteCallback() {};
        mRouter2.registerRouteCallback(mExecutor, routeCallback, EMPTY_DISCOVERY_PREFERENCE);

        try {
            mRouter2.registerTransferCallback(mExecutor, transferCallback);
            mRouter2.registerControllerCallback(mExecutor, controllerCallback);
            mRouter2.transferTo(routeTransferFrom);
            assertTrue(onTransferLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));

            assertEquals(1, controllers.size());
            RoutingController controller = controllers.get(0);

            // Release controller. Future calls should be ignored.
            controller.release();

            // Select ROUTE_ID5_TO_TRANSFER_TO
            MediaRoute2Info routeToSelect = routes.get(ROUTE_ID4_TO_SELECT_AND_DESELECT);
            assertNotNull(routeToSelect);

            // This call should be ignored.
            // The onSessionInfoChanged() shouldn't be called.
            controller.selectRoute(routeToSelect);
            assertFalse(onControllerUpdatedLatch.await(WAIT_MS, TimeUnit.MILLISECONDS));

            // onStop should be called.
            assertTrue(onStopLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } finally {
            releaseControllers(controllers);
            mRouter2.unregisterRouteCallback(routeCallback);
            mRouter2.unregisterControllerCallback(controllerCallback);
            mRouter2.unregisterTransferCallback(transferCallback);
        }
    }

    // TODO: Consider adding tests with bluetooth connection/disconnection.
    @Test
    public void testGetSystemController() {
        final RoutingController systemController = mRouter2.getSystemController();
        assertNotNull(systemController);
        assertFalse(systemController.isReleased());

        for (MediaRoute2Info route : systemController.getSelectedRoutes()) {
            assertTrue(route.isSystemRoute());
        }
    }

    @Test
    public void testGetControllers() {
        List<RoutingController> controllers = mRouter2.getControllers();
        assertNotNull(controllers);
        assertFalse(controllers.isEmpty());
        assertSame(mRouter2.getSystemController(), controllers.get(0));
    }

    @Test
    public void testVolumeHandlingWhenVolumeFixed() {
        if (!mAudioManager.isVolumeFixed()) {
            return;
        }
        MediaRoute2Info selectedSystemRoute =
                mRouter2.getSystemController().getSelectedRoutes().get(0);
        assertEquals(MediaRoute2Info.PLAYBACK_VOLUME_FIXED,
                selectedSystemRoute.getVolumeHandling());
    }

    @Test
    public void testCallbacksAreCalledWhenVolumeChanged() throws Exception {
        if (mAudioManager.isVolumeFixed()) {
            return;
        }

        final int maxVolume = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        final int minVolume = mAudioManager.getStreamMinVolume(AudioManager.STREAM_MUSIC);
        final int originalVolume = mAudioManager.getStreamVolume(AudioManager.STREAM_MUSIC);

        MediaRoute2Info selectedSystemRoute =
                mRouter2.getSystemController().getSelectedRoutes().get(0);

        assertEquals(maxVolume, selectedSystemRoute.getVolumeMax());
        assertEquals(originalVolume, selectedSystemRoute.getVolume());
        assertEquals(PLAYBACK_VOLUME_VARIABLE,
                selectedSystemRoute.getVolumeHandling());

        final int targetVolume = originalVolume == minVolume
                ? originalVolume + 1 : originalVolume - 1;
        final CountDownLatch latch = new CountDownLatch(1);
        RouteCallback routeCallback = new RouteCallback() {
            @Override
            public void onRoutesChanged(List<MediaRoute2Info> routes) {
                for (MediaRoute2Info route : routes) {
                    if (route.getId().equals(selectedSystemRoute.getId())
                            && route.getVolume() == targetVolume) {
                        latch.countDown();
                        break;
                    }
                }
            }
        };

        mRouter2.registerRouteCallback(mExecutor, routeCallback, LIVE_AUDIO_DISCOVERY_PREFERENCE);

        try {
            mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, targetVolume, 0);
            assertTrue(latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } finally {
            mRouter2.unregisterRouteCallback(routeCallback);
            mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, originalVolume, 0);
        }
    }

    @Test
    public void markCallbacksAsTested() {
        // Due to CTS coverage tool's bug, it doesn't count the callback methods as tested even if
        // we have tests for them. This method just directly calls those methods so that the tool
        // can recognize the callback methods as tested.

        RouteCallback routeCallback = new RouteCallback() {};
        routeCallback.onRoutesAdded(null);
        routeCallback.onRoutesChanged(null);
        routeCallback.onRoutesRemoved(null);

        TransferCallback transferCallback = new TransferCallback() {};
        transferCallback.onTransfer(null, null);
        transferCallback.onTransferFailure(null);

        ControllerCallback controllerCallback = new ControllerCallback() {};
        controllerCallback.onControllerUpdated(null);

        OnGetControllerHintsListener listener = route -> null;
        listener.onGetControllerHints(null);
    }

    // Helper for getting routes easily. Uses original ID as a key
    private static Map<String, MediaRoute2Info> createRouteMap(List<MediaRoute2Info> routes) {
        Map<String, MediaRoute2Info> routeMap = new HashMap<>();
        for (MediaRoute2Info route : routes) {
            routeMap.put(route.getOriginalId(), route);
        }
        return routeMap;
    }

    private Map<String, MediaRoute2Info> waitAndGetRoutes(List<String> routeTypes)
            throws Exception {
        CountDownLatch latch = new CountDownLatch(1);

        RouteCallback routeCallback = new RouteCallback() {
            @Override
            public void onRoutesAdded(List<MediaRoute2Info> routes) {
                for (MediaRoute2Info route : routes) {
                    if (!route.isSystemRoute()) {
                        latch.countDown();
                    }
                }
            }
        };

        mRouter2.registerRouteCallback(mExecutor, routeCallback,
                new RouteDiscoveryPreference.Builder(routeTypes, true).build());
        try {
            latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS);
            return createRouteMap(mRouter2.getRoutes());
        } finally {
            mRouter2.unregisterRouteCallback(routeCallback);
        }
    }

    static void releaseControllers(@NonNull List<RoutingController> controllers) {
        for (RoutingController controller : controllers) {
            controller.release();
        }
    }

    /**
     * Returns a list of original route IDs of the given route list.
     */
    private List<String> getOriginalRouteIds(@NonNull List<MediaRoute2Info> routes) {
        List<String> result = new ArrayList<>();
        for (MediaRoute2Info route : routes) {
            result.add(route.getOriginalId());
        }
        return result;
    }
}
