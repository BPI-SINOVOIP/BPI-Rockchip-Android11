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

package com.android.car;

import static com.google.common.truth.Truth.assertThat;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;
import static junit.framework.Assert.fail;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.car.IPerUserCarService;
import android.car.drivingstate.CarDrivingStateEvent;
import android.car.drivingstate.ICarDrivingStateChangeListener;
import android.car.hardware.power.CarPowerManager.CarPowerStateListener;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.location.Location;
import android.location.LocationManager;
import android.os.SystemClock;
import android.os.UserManager;

import androidx.test.InstrumentationRegistry;

import com.android.car.systeminterface.SystemInterface;
import com.android.car.test.utils.TemporaryDirectory;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CountDownLatch;
import java.util.stream.Collectors;

/**
 * This class contains unit tests for the {@link CarLocationService}.
 * It tests that {@link LocationManagerProxy}'s last known location is stored in and loaded from a
 * JSON file upon appropriate system events.
 *
 * The following mocks are used:
 * 1. {@link Context} registers intent receivers.
 * 2. {@link SystemInterface} tells where to store system files.
 * 3. {@link CarDrivingStateService} tells about driving state changes.
 * 4. {@link PerUserCarServiceHelper} provides a mocked {@link IPerUserCarService}.
 * 5. {@link IPerUserCarService} provides a mocked {@link LocationManagerProxy}.
 * 6. {@link LocationManagerProxy} provides dummy {@link Location}s.
 */
@RunWith(MockitoJUnitRunner.class)
public class CarLocationServiceTest {
    private static final String TAG = "CarLocationServiceTest";
    private static final String TEST_FILENAME = "location_cache.json";
    private CarLocationService mCarLocationService;
    private Context mContext;
    private CountDownLatch mLatch;
    private File mTempDirectory;
    private PerUserCarServiceHelper.ServiceCallback mUserServiceCallback;
    @Mock
    private Context mMockContext;
    @Mock
    private LocationManagerProxy mMockLocationManagerProxy;
    @Mock
    private SystemInterface mMockSystemInterface;
    @Mock
    private CarDrivingStateService mMockCarDrivingStateService;
    @Mock
    private PerUserCarServiceHelper mMockPerUserCarServiceHelper;
    @Mock
    private IPerUserCarService mMockIPerUserCarService;

    /**
     * Initialize all of the objects with the @Mock annotation.
     */
    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mTempDirectory = new TemporaryDirectory(TAG).getDirectory();
        mLatch = new CountDownLatch(1);
        mCarLocationService = new CarLocationService(mMockContext) {
            @Override
            void asyncOperation(Runnable operation) {
                super.asyncOperation(() -> {
                    operation.run();
                    mLatch.countDown();
                });
            }
        };
        CarLocalServices.removeServiceForTest(SystemInterface.class);
        CarLocalServices.addService(SystemInterface.class, mMockSystemInterface);
        CarLocalServices.removeServiceForTest(CarDrivingStateService.class);
        CarLocalServices.addService(CarDrivingStateService.class, mMockCarDrivingStateService);
        CarLocalServices.removeServiceForTest(PerUserCarServiceHelper.class);
        CarLocalServices.addService(PerUserCarServiceHelper.class, mMockPerUserCarServiceHelper);
        when(mMockSystemInterface.getSystemCarDir()).thenReturn(mTempDirectory);
        when(mMockIPerUserCarService.getLocationManagerProxy())
                .thenReturn(mMockLocationManagerProxy);

        if (!UserManager.isHeadlessSystemUserMode()) {
            fail("We only support and test the headless system user case. Ensure the system has "
                    + "the system property 'ro.fw.mu.headless_system_user' set to true.");
        }

        // Store CarLocationService's user switch callback so we can invoke it in the tests.
        doAnswer((invocation) -> {
            Object[] arguments = invocation.getArguments();
            assertThat(arguments).isNotNull();
            assertThat(arguments).hasLength(1);
            assertThat(arguments[0]).isNotNull();
            mUserServiceCallback = (PerUserCarServiceHelper.ServiceCallback) arguments[0];
            return null;
        }).when(mMockPerUserCarServiceHelper).registerServiceCallback(any(
                PerUserCarServiceHelper.ServiceCallback.class));
    }

    @After
    public void tearDown() throws Exception {
        if (mCarLocationService != null) {
            mCarLocationService.release();
        }
    }

    /**
     * Test that the {@link CarLocationService} has the permissions necessary to call the
     * {@link LocationManager} injectLocation API.
     *
     * Note that this test will never fail even if the relevant permissions are removed from the
     * manifest since {@link CarService} runs in a system process.
     */
    @Test
    public void testCarLocationServiceShouldHavePermissions() {
        int fineLocationCheck =
                mContext.checkSelfPermission(android.Manifest.permission.ACCESS_FINE_LOCATION);
        int locationHardwareCheck =
                mContext.checkSelfPermission(android.Manifest.permission.LOCATION_HARDWARE);
        assertEquals(PackageManager.PERMISSION_GRANTED, fineLocationCheck);
        assertEquals(PackageManager.PERMISSION_GRANTED, locationHardwareCheck);
    }

    /**
     * Test that the {@link CarLocationService} registers to receive location intents.
     */
    @Test
    public void testRegistersToReceiveEvents() {
        mCarLocationService.init();
        ArgumentCaptor<IntentFilter> intentFilterArgument = ArgumentCaptor.forClass(
                IntentFilter.class);
        verify(mMockContext).registerReceiver(eq(mCarLocationService),
                intentFilterArgument.capture());
        verify(mMockCarDrivingStateService).registerDrivingStateChangeListener(any());
        verify(mMockPerUserCarServiceHelper).registerServiceCallback(any());
        IntentFilter intentFilter = intentFilterArgument.getValue();
        assertThat(intentFilter.countActions()).isEqualTo(1);
        assertThat(intentFilter.getAction(0)).isEqualTo(LocationManager.MODE_CHANGED_ACTION);
    }

    /**
     * Test that the {@link CarLocationService} unregisters its event receivers.
     */
    @Test
    public void testUnregistersEventReceivers() {
        mCarLocationService.init();
        mCarLocationService.release();
        verify(mMockContext).unregisterReceiver(mCarLocationService);
        verify(mMockCarDrivingStateService).unregisterDrivingStateChangeListener(any());
        verify(mMockPerUserCarServiceHelper).unregisterServiceCallback(any());
    }

    /**
     * Test that the {@link CarLocationService} parses a location from a JSON serialization and then
     * injects it into the {@link LocationManagerProxy} upon onServiceConnected.
     */
    @Test
    public void testLoadsLocationWithHeadlessSystemUser() throws Exception {
        mCarLocationService.init();
        assertThat(mUserServiceCallback).isNotNull();
        long currentTime = System.currentTimeMillis();
        long elapsedTime = SystemClock.elapsedRealtimeNanos();
        long pastTime = currentTime - 60000;
        writeCacheFile("{\"provider\": \"gps\", \"latitude\": 16.7666, \"longitude\": 3.0026,"
                + "\"accuracy\":12.3, \"captureTime\": " + pastTime + "}");
        ArgumentCaptor<Location> argument = ArgumentCaptor.forClass(Location.class);
        when(mMockLocationManagerProxy.injectLocation(argument.capture())).thenReturn(true);

        mUserServiceCallback.onServiceConnected(mMockIPerUserCarService);
        mLatch.await();

        Location location = argument.getValue();
        assertEquals("gps", location.getProvider());
        assertEquals(16.7666, location.getLatitude());
        assertEquals(3.0026, location.getLongitude());
        assertEquals(12.3f, location.getAccuracy());
        assertTrue(location.getTime() >= currentTime);
        assertTrue(location.getElapsedRealtimeNanos() >= elapsedTime);
    }

    /**
     * Test that the {@link CarLocationService} does not inject a location if there is no location
     * cache file.
     */
    @Test
    public void testDoesNotLoadLocationWhenNoFileExists() throws Exception {
        mCarLocationService.init();
        assertThat(mUserServiceCallback).isNotNull();
        assertThat(getLocationCacheFile().exists()).isFalse();

        mUserServiceCallback.onServiceConnected(mMockIPerUserCarService);
        mLatch.await();

        verify(mMockLocationManagerProxy, never()).injectLocation(any());
    }

    /**
     * Test that the {@link CarLocationService} handles an incomplete JSON file gracefully.
     */
    @Test
    public void testDoesNotLoadLocationFromIncompleteFile() throws Exception {
        mCarLocationService.init();
        assertThat(mUserServiceCallback).isNotNull();
        writeCacheFile("{\"provider\": \"gps\", \"latitude\": 16.7666, \"longitude\": 3.0026,");

        mUserServiceCallback.onServiceConnected(mMockIPerUserCarService);
        mLatch.await();

        verify(mMockLocationManagerProxy, never()).injectLocation(any());
    }

    /**
     * Test that the {@link CarLocationService} handles a corrupt JSON file gracefully.
     */
    @Test
    public void testDoesNotLoadLocationFromCorruptFile() throws Exception {
        mCarLocationService.init();
        assertThat(mUserServiceCallback).isNotNull();
        writeCacheFile("{\"provider\":\"latitude\":16.7666,\"longitude\": \"accuracy\":1.0}");

        mUserServiceCallback.onServiceConnected(mMockIPerUserCarService);
        mLatch.await();

        verify(mMockLocationManagerProxy, never()).injectLocation(any());
    }

    /**
     * Test that the {@link CarLocationService} does not inject a location that is missing
     * accuracy.
     */
    @Test
    public void testDoesNotLoadIncompleteLocation() throws Exception {
        mCarLocationService.init();
        assertThat(mUserServiceCallback).isNotNull();
        writeCacheFile("{\"provider\": \"gps\", \"latitude\": 16.7666, \"longitude\": 3.0026}");

        mUserServiceCallback.onServiceConnected(mMockIPerUserCarService);
        mLatch.await();

        verify(mMockLocationManagerProxy, never()).injectLocation(any());
    }

    /**
     * Test that the {@link CarLocationService} does not inject a location that is older than
     * thirty days.
     */
    @Test
    public void testDoesNotLoadOldLocation() throws Exception {
        mCarLocationService.init();
        assertThat(mUserServiceCallback).isNotNull();
        long thirtyThreeDaysMs = 33 * 24 * 60 * 60 * 1000L;
        long oldTime = System.currentTimeMillis() - thirtyThreeDaysMs;
        writeCacheFile("{\"provider\": \"gps\", \"latitude\": 16.7666, \"longitude\": 3.0026,"
                + "\"accuracy\":12.3, \"captureTime\": " + oldTime + "}");

        mUserServiceCallback.onServiceConnected(mMockIPerUserCarService);
        mLatch.await();

        verify(mMockLocationManagerProxy, never()).injectLocation(any());
    }

    /**
     * Test that the {@link CarLocationService} stores the {@link LocationManager}'s last known
     * location upon power state-changed SHUTDOWN_PREPARE events.
     */
    @Test
    public void testStoresLocationUponShutdownPrepare() throws Exception {
        // We must have a LocationManagerProxy for the current user in order to get a location
        // during shutdown-prepare.
        mCarLocationService.init();
        mUserServiceCallback.onServiceConnected(mMockIPerUserCarService);
        mLatch.await();
        mLatch = new CountDownLatch(1);

        long currentTime = System.currentTimeMillis();
        long elapsedTime = SystemClock.elapsedRealtimeNanos();
        Location timbuktu = new Location(LocationManager.GPS_PROVIDER);
        timbuktu.setLatitude(16.7666);
        timbuktu.setLongitude(3.0026);
        timbuktu.setAccuracy(13.75f);
        timbuktu.setTime(currentTime);
        timbuktu.setElapsedRealtimeNanos(elapsedTime);
        when(mMockLocationManagerProxy.getLastKnownLocation(LocationManager.GPS_PROVIDER))
                .thenReturn(timbuktu);
        CompletableFuture<Void> future = new CompletableFuture<>();

        mCarLocationService.onStateChanged(CarPowerStateListener.SHUTDOWN_PREPARE, future);
        mLatch.await();

        verify(mMockLocationManagerProxy).getLastKnownLocation(LocationManager.GPS_PROVIDER);
        assertTrue(future.isDone());
        String actualContents = readCacheFile();
        long oneDayMs = 24 * 60 * 60 * 1000;
        long granularCurrentTime = (currentTime / oneDayMs) * oneDayMs;
        String expectedContents = "{\"provider\":\"gps\",\"latitude\":16.7666,\"longitude\":"
                + "3.0026,\"accuracy\":13.75,\"captureTime\":" + granularCurrentTime + "}";
        assertEquals(expectedContents, actualContents);
    }

    /**
     * Test that the {@link CarLocationService} does not throw an exception on SUSPEND_EXIT events.
     */
    @Test
    public void testDoesNotThrowExceptionUponPowerStateChanged() {
        try {
            mCarLocationService.onStateChanged(CarPowerStateListener.SUSPEND_ENTER, null);
            mCarLocationService.onStateChanged(CarPowerStateListener.SUSPEND_EXIT, null);
            mCarLocationService.onStateChanged(CarPowerStateListener.SHUTDOWN_ENTER, null);
            mCarLocationService.onStateChanged(CarPowerStateListener.ON, null);
            mCarLocationService.onStateChanged(CarPowerStateListener.WAIT_FOR_VHAL, null);
            mCarLocationService.onStateChanged(CarPowerStateListener.SHUTDOWN_CANCELLED, null);
        } catch (Exception e) {
            fail("onStateChanged should not throw an exception: " + e);
        }
    }

    /**
     * Test that the {@link CarLocationService} does not store null locations.
     */
    @Test
    public void testDoesNotStoreNullLocation() throws Exception {
        // We must have a LocationManagerProxy for the current user in order to get a location
        // during shutdown-prepare.
        mCarLocationService.init();
        mUserServiceCallback.onServiceConnected(mMockIPerUserCarService);
        mLatch.await();
        mLatch = new CountDownLatch(1);

        when(mMockLocationManagerProxy.getLastKnownLocation(LocationManager.GPS_PROVIDER))
                .thenReturn(null);
        CompletableFuture<Void> future = new CompletableFuture<>();

        mCarLocationService.onStateChanged(CarPowerStateListener.SHUTDOWN_PREPARE, future);
        mLatch.await();

        assertTrue(future.isDone());
        verify(mMockLocationManagerProxy).getLastKnownLocation(LocationManager.GPS_PROVIDER);
        assertFalse(getLocationCacheFile().exists());
    }

    /**
     * Test that the {@link CarLocationService} deletes location_cache.json when location is
     * disabled.
     */
    @Test
    public void testDeletesCacheFileWhenLocationIsDisabled() throws Exception {
        // We must have a LocationManagerProxy for the current user in order to check whether or
        // not location is enabled.
        mCarLocationService.init();
        mUserServiceCallback.onServiceConnected(mMockIPerUserCarService);
        mLatch.await();
        mLatch = new CountDownLatch(1);

        writeCacheFile("{\"provider\":\"latitude\":16.7666,\"longitude\": \"accuracy\":1.0}");
        when(mMockLocationManagerProxy.isLocationEnabled()).thenReturn(false);
        assertTrue(getLocationCacheFile().exists());

        mCarLocationService.onReceive(mMockContext,
                new Intent(LocationManager.MODE_CHANGED_ACTION));

        verify(mMockLocationManagerProxy, times(1)).isLocationEnabled();
        assertFalse(getLocationCacheFile().exists());
    }

    /**
     * Test that the {@link CarLocationService} deletes location_cache.json when the system resumes
     * from suspend-to-ram if moving.
     */
    @Test
    public void testDeletesCacheFileUponSuspendExitWhileMoving() throws Exception {
        mCarLocationService.init();
        when(mMockLocationManagerProxy.isLocationEnabled()).thenReturn(false);
        when(mMockCarDrivingStateService.getCurrentDrivingState()).thenReturn(
                new CarDrivingStateEvent(CarDrivingStateEvent.DRIVING_STATE_MOVING, 0));
        writeCacheFile("{\"provider\":\"latitude\":16.7666,\"longitude\": \"accuracy\":1.0}");
        CompletableFuture<Void> future = new CompletableFuture<>();
        assertTrue(getLocationCacheFile().exists());

        mCarLocationService.onStateChanged(CarPowerStateListener.SUSPEND_EXIT, future);

        assertTrue(future.isDone());
        verify(mMockLocationManagerProxy, times(0)).isLocationEnabled();
        assertFalse(getLocationCacheFile().exists());
    }

    /**
     * Test that the {@link CarLocationService} registers for driving state changes when the system
     * resumes from suspend-to-ram.
     */
    @Test
    public void testRegistersForDrivingStateChangesUponSuspendExit() throws Exception {
        mCarLocationService.init();
        when(mMockLocationManagerProxy.isLocationEnabled()).thenReturn(false);
        when(mMockCarDrivingStateService.getCurrentDrivingState()).thenReturn(
                new CarDrivingStateEvent(CarDrivingStateEvent.DRIVING_STATE_PARKED, 0));
        CompletableFuture<Void> future = new CompletableFuture<>();

        mCarLocationService.onStateChanged(CarPowerStateListener.SUSPEND_EXIT, future);

        assertTrue(future.isDone());
        verify(mMockLocationManagerProxy, times(0)).isLocationEnabled();
        // One of the registrations should happen during init and another during onStateChanged.
        verify(mMockCarDrivingStateService, times(2)).registerDrivingStateChangeListener(any());
    }

    /**
     * Test that the {@link CarLocationService} deletes location_cache.json when the car enters a
     * moving driving state.
     */
    @Test
    public void testDeletesCacheFileWhenDrivingStateBecomesMoving() throws Exception {
        mCarLocationService.init();
        writeCacheFile("{\"provider\":\"latitude\":16.7666,\"longitude\": \"accuracy\":1.0}");
        when(mMockLocationManagerProxy.isLocationEnabled()).thenReturn(false);
        ArgumentCaptor<ICarDrivingStateChangeListener> changeListenerArgument =
                ArgumentCaptor.forClass(ICarDrivingStateChangeListener.class);
        verify(mMockCarDrivingStateService).registerDrivingStateChangeListener(
                changeListenerArgument.capture());
        ICarDrivingStateChangeListener changeListener = changeListenerArgument.getValue();
        assertTrue(getLocationCacheFile().exists());

        changeListener.onDrivingStateChanged(
                new CarDrivingStateEvent(CarDrivingStateEvent.DRIVING_STATE_MOVING,
                        SystemClock.elapsedRealtimeNanos()));

        verify(mMockLocationManagerProxy, times(0)).isLocationEnabled();
        verify(mMockCarDrivingStateService, times(1)).unregisterDrivingStateChangeListener(any());
        assertFalse(getLocationCacheFile().exists());
    }

    private void writeCacheFile(String json) throws IOException {
        FileOutputStream fos = new FileOutputStream(getLocationCacheFile());
        fos.write(json.getBytes());
        fos.close();
    }

    private String readCacheFile() throws IOException {
        FileInputStream fis = new FileInputStream(getLocationCacheFile());
        String json = new BufferedReader(new InputStreamReader(fis)).lines()
                .parallel().collect(Collectors.joining("\n"));
        fis.close();
        return json;
    }

    private File getLocationCacheFile() {
        return new File(mTempDirectory, TEST_FILENAME);
    }
}
