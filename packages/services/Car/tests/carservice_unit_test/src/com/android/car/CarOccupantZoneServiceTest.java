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

package com.android.car;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.spyOn;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;
import static org.testng.Assert.expectThrows;

import android.annotation.UserIdInt;
import android.car.Car;
import android.car.CarOccupantZoneManager;
import android.car.CarOccupantZoneManager.OccupantZoneInfo;
import android.car.VehicleAreaSeat;
import android.car.media.CarAudioManager;
import android.car.user.CarUserManager;
import android.car.user.CarUserManager.UserLifecycleEvent;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.hardware.display.DisplayManager;
import android.os.Looper;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.SparseIntArray;
import android.view.Display;
import android.view.DisplayAddress;

import com.android.car.CarOccupantZoneService.DisplayConfig;
import com.android.car.CarOccupantZoneService.DisplayInfo;
import com.android.car.CarOccupantZoneService.OccupantConfig;
import com.android.car.user.CarUserService;
import com.android.internal.car.ICarServiceHelper;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

@RunWith(MockitoJUnitRunner.class)
public class CarOccupantZoneServiceTest {

    private static final String TAG = CarOccupantZoneServiceTest.class.getSimpleName();

    private CarOccupantZoneService mService;
    private CarOccupantZoneManager mManager;

    @Mock
    private CarPropertyService mCarPropertyService;

    @Mock
    private CarUserService mCarUserService;

    @Mock
    private Context mContext;

    @Mock
    private DisplayManager mDisplayManager;

    @Mock
    private UserManager mUserManager;

    @Mock
    private Resources mResources;

    @Mock
    private Display mDisplay0;

    @Mock
    private Display mDisplay1;

    @Mock
    private Display mDisplay2;

    @Mock
    private Display mDisplay3; // not listed by default

    @Mock
    private Display mDisplay4;

    @Mock
    private Display mDisplay5; // outside display config and become unknown display

    private static final int CURRENT_USER = 100;
    private static final int PROFILE_USER1 = 1001;
    private static final int PROFILE_USER2 = 1002;

    private static final String[] DEFAULT_OCCUPANT_ZONES = {
            "occupantZoneId=0,occupantType=DRIVER,seatRow=1,seatSide=driver",
            "occupantZoneId=1,occupantType=FRONT_PASSENGER,seatRow=1,seatSide=oppositeDriver",
            "occupantZoneId=2,occupantType=REAR_PASSENGER,seatRow=2,seatSide=left",
            "occupantZoneId=3,occupantType=REAR_PASSENGER,seatRow=2,seatSide=right"
    };

    private static final int PRIMARY_AUDIO_ZONE_ID = 0;
    private static final int PRIMARY_AUDIO_ZONE_ID_OCCUPANT = 0;
    private static final int SECONDARY_AUDIO_ZONE_ID = 1;
    private static final int SECONDARY_AUDIO_ZONE_ID_OCCUPANT = 3;
    private static final int UNMAPPED_AUDIO_ZONE_ID_OCCUPANT = 2;
    private static final int INVALID_AUDIO_ZONE_ID_OCCUPANT = 100;

    // LHD : Left Hand Drive
    private final OccupantZoneInfo mZoneDriverLHD = new OccupantZoneInfo(0,
            CarOccupantZoneManager.OCCUPANT_TYPE_DRIVER,
            VehicleAreaSeat.SEAT_ROW_1_LEFT);
    private final OccupantZoneInfo mZoneFrontPassengerLHD = new OccupantZoneInfo(1,
            CarOccupantZoneManager.OCCUPANT_TYPE_FRONT_PASSENGER,
            VehicleAreaSeat.SEAT_ROW_1_RIGHT);
    private final OccupantZoneInfo mZoneRearLeft = new OccupantZoneInfo(2,
            CarOccupantZoneManager.OCCUPANT_TYPE_REAR_PASSENGER,
            VehicleAreaSeat.SEAT_ROW_2_LEFT);
    private final OccupantZoneInfo mZoneRearRight = new OccupantZoneInfo(3,
            CarOccupantZoneManager.OCCUPANT_TYPE_REAR_PASSENGER,
            VehicleAreaSeat.SEAT_ROW_2_RIGHT);

    // port address set to mocked displayid + 10 so that any possible mix of port address and
    // display id can be detected.
    private static final String[] DEFAULT_OCCUPANT_DISPLAY_MAPPING = {
            "displayPort=10,displayType=MAIN,occupantZoneId=0",
            "displayPort=11,displayType=INSTRUMENT_CLUSTER,occupantZoneId=0",
            "displayPort=12,displayType=MAIN,occupantZoneId=1",
            "displayPort=13,displayType=MAIN,occupantZoneId=2",
            "displayPort=14,displayType=MAIN,occupantZoneId=3"
    };

    // Stores last changeFlags from onOccupantZoneConfigChanged call.
    private int mLastChangeFlags;
    private final Semaphore mChangeEventSignal = new Semaphore(0);

    private final ICarServiceHelperImpl mICarServiceHelper = new ICarServiceHelperImpl();

    private final CarOccupantZoneManager.OccupantZoneConfigChangeListener mChangeListener =
            new CarOccupantZoneManager.OccupantZoneConfigChangeListener() {
                @Override
                public void onOccupantZoneConfigChanged(int changeFlags) {
                    // should be dispatched to main thread.
                    assertThat(Looper.getMainLooper()).isEqualTo(Looper.myLooper());
                    mLastChangeFlags = changeFlags;
                    mChangeEventSignal.release();
                }
            };

    private void resetConfigChangeEventWait() {
        mLastChangeFlags = 0;
        mChangeEventSignal.drainPermits();
    }

    private boolean waitForConfigChangeEventAndAssertFlag(long timeoutMs, int expectedFlag) {
        boolean acquired = false;
        try {
            acquired = mChangeEventSignal.tryAcquire(timeoutMs, TimeUnit.MILLISECONDS);
        } catch (Exception ignored) {

        }
        if (acquired) {
            assertThat(expectedFlag).isEqualTo(mLastChangeFlags);
        }
        return acquired;
    }

    private void mockDisplay(DisplayManager displayManager, Display display, int displayId,
            int portAddress) {
        when(displayManager.getDisplay(displayId)).thenReturn(display);
        when(display.getDisplayId()).thenReturn(displayId);
        when(display.getAddress()).thenReturn(DisplayAddress.fromPhysicalDisplayId(portAddress));
    }

    @Before
    public void setUp() {
        when(mContext.getResources()).thenReturn(mResources);
        when(mContext.getSystemService(DisplayManager.class)).thenReturn(mDisplayManager);
        when(mResources.getStringArray(R.array.config_occupant_zones))
                .thenReturn(DEFAULT_OCCUPANT_ZONES);
        when(mResources.getStringArray(R.array.config_occupant_display_mapping))
                .thenReturn(DEFAULT_OCCUPANT_DISPLAY_MAPPING);
        when(mContext.getApplicationInfo()).thenReturn(new ApplicationInfo());
        // Stored as static: Other tests can leave things behind and fail this test in add call.
        // So just remove as safety guard.
        CarLocalServices.removeServiceForTest(CarPropertyService.class);
        CarLocalServices.addService(CarPropertyService.class, mCarPropertyService);
        CarLocalServices.removeServiceForTest(CarUserService.class);
        CarLocalServices.addService(CarUserService.class, mCarUserService);
        mockDisplay(mDisplayManager, mDisplay0, 0, 10);
        mockDisplay(mDisplayManager, mDisplay1, 1, 11);
        mockDisplay(mDisplayManager, mDisplay2, 2, 12);
        mockDisplay(mDisplayManager, mDisplay4, 4, 14);
        mockDisplay(mDisplayManager, mDisplay5, 5, 15);
        when(mDisplayManager.getDisplays()).thenReturn(new Display[]{
                mDisplay0,
                mDisplay1,
                mDisplay2,
                mDisplay4,
                mDisplay5
        });

        mService = new CarOccupantZoneService(mContext, mDisplayManager, mUserManager,
                /* enableProfileUserAssignmentForMultiDisplay= */ false);
        spyOn(mService);
        doReturn(VehicleAreaSeat.SEAT_ROW_1_LEFT).when(mService).getDriverSeat();
        doReturn(CURRENT_USER).when(mService).getCurrentUser();

        Car car = new Car(mContext, /* service= */ null, /* handler= */ null);
        mManager = new CarOccupantZoneManager(car, mService);
    }

    @After
    public void tearDown() {
        CarLocalServices.removeServiceForTest(CarUserService.class);
        CarLocalServices.removeServiceForTest(CarPropertyService.class);
    }

    @Test
    public void testDefaultOccupantConfig() {
        mService.init();

        // key : zone id
        HashMap<Integer, OccupantZoneInfo> configs = mService.getOccupantsConfig();
        assertThat(configs).hasSize(DEFAULT_OCCUPANT_ZONES.length);
        assertThat(mZoneDriverLHD).isEqualTo(configs.get(0));
        assertThat(mZoneFrontPassengerLHD).isEqualTo(configs.get(1));
        assertThat(mZoneRearLeft).isEqualTo(configs.get(2));
        assertThat(mZoneRearRight).isEqualTo(configs.get(3));
    }

    @Test
    public void testDefaultAudioZoneConfig() {
        mService.init();
        SparseIntArray audioConfigs = mService.getAudioConfigs();
        assertThat(audioConfigs.size()).isEqualTo(0);
    }

    /** RHD: Right Hand Drive */
    @Test
    public void testDefaultOccupantConfigForRHD() {
        // driver is right side and opposite should be left.
        doReturn(VehicleAreaSeat.SEAT_ROW_1_RIGHT).when(mService).getDriverSeat();

        mService.init();

        // key : zone id
        HashMap<Integer, OccupantZoneInfo> configs = mService.getOccupantsConfig();
        assertThat(configs).hasSize(DEFAULT_OCCUPANT_ZONES.length);
        assertThat(new OccupantZoneInfo(0, CarOccupantZoneManager.OCCUPANT_TYPE_DRIVER,
                VehicleAreaSeat.SEAT_ROW_1_RIGHT)).isEqualTo(configs.get(0));
        assertThat(new OccupantZoneInfo(1, CarOccupantZoneManager.OCCUPANT_TYPE_FRONT_PASSENGER,
                VehicleAreaSeat.SEAT_ROW_1_LEFT)).isEqualTo(configs.get(1));
        assertThat(mZoneRearLeft).isEqualTo(configs.get(2));
        assertThat(mZoneRearRight).isEqualTo(configs.get(3));
    }

    private void assertDisplayConfig(DisplayConfig c, int displayType, int occupantZoneId) {
        assertThat(displayType).isEqualTo(c.displayType);
        assertThat(occupantZoneId).isEqualTo(c.occupantZoneId);
    }

    @Test
    public void testDefaultOccupantDisplayMapping() {
        mService.init();

        // key: display port address
        HashMap<Integer, DisplayConfig> configs = mService.getDisplayConfigs();
        assertThat(configs).hasSize(DEFAULT_OCCUPANT_DISPLAY_MAPPING.length);
        assertDisplayConfig(configs.get(10), CarOccupantZoneManager.DISPLAY_TYPE_MAIN, 0);
        assertDisplayConfig(configs.get(11), CarOccupantZoneManager.DISPLAY_TYPE_INSTRUMENT_CLUSTER,
                0);
        assertDisplayConfig(configs.get(12), CarOccupantZoneManager.DISPLAY_TYPE_MAIN, 1);
        assertDisplayConfig(configs.get(13), CarOccupantZoneManager.DISPLAY_TYPE_MAIN, 2);
        assertDisplayConfig(configs.get(14), CarOccupantZoneManager.DISPLAY_TYPE_MAIN, 3);
    }

    private void setUpServiceWithProfileSupportEnabled() {
        mService = new CarOccupantZoneService(mContext, mDisplayManager, mUserManager,
                /* enableProfileUserAssignmentForMultiDisplay= */ true);
        spyOn(mService);
        doReturn(VehicleAreaSeat.SEAT_ROW_1_LEFT).when(mService).getDriverSeat();
        doReturn(CURRENT_USER).when(mService).getCurrentUser();
        LinkedList<UserInfo> profileUsers = new LinkedList<>();
        profileUsers.add(new UserInfo(PROFILE_USER1, "1", 0));
        profileUsers.add(new UserInfo(PROFILE_USER2, "1", 0));
        doReturn(profileUsers).when(mUserManager).getEnabledProfiles(CURRENT_USER);
        doReturn(true).when(mUserManager).isUserRunning(anyInt());

        Car car = new Car(mContext, /* service= */ null, /* handler= */ null);
        mManager = new CarOccupantZoneManager(car, mService);
    }

    @Test
    public void testAssignProfileUserFailForNonProfileUser() throws Exception {
        setUpServiceWithProfileSupportEnabled();
        mService.init();

        int invalidProfileUser = 2000;
        assertThat(mManager.assignProfileUserToOccupantZone(mZoneFrontPassengerLHD,
                invalidProfileUser)).isFalse();
    }

    private void assertDisplayWhitelist(int userId, int[] displays) {
        assertThat(mICarServiceHelper.mWhitelists).containsKey(userId);
        assertThat(mICarServiceHelper.mWhitelists.get(userId)).hasSize(displays.length);
        for (int display : displays) {
            assertThat(mICarServiceHelper.mWhitelists.get(userId)).contains(display);
        }
    }

    private void assertPassengerDisplaysFromDefaultConfig() throws Exception {
        assertThat(mICarServiceHelper.mPassengerDisplayIds).hasSize(2);
        assertThat(mICarServiceHelper.mPassengerDisplayIds).contains(
                mDisplay2.getDisplayId());
        assertThat(mICarServiceHelper.mPassengerDisplayIds).contains(
                mDisplay4.getDisplayId());
    }

    @Test
    public void testAssignProfileUserOnce() throws Exception {
        setUpServiceWithProfileSupportEnabled();
        mService.init();
        mService.setCarServiceHelper(mICarServiceHelper);

        assertPassengerDisplaysFromDefaultConfig();

        mICarServiceHelper.mWhitelists.clear();
        assertThat(mManager.assignProfileUserToOccupantZone(mZoneFrontPassengerLHD,
                PROFILE_USER1)).isTrue();
        assertPassengerDisplaysFromDefaultConfig();
        assertDisplayWhitelist(CURRENT_USER, new int[] {mDisplay4.getDisplayId()});
        assertDisplayWhitelist(PROFILE_USER1, new int[] {mDisplay2.getDisplayId()});
    }

    @Test
    public void testAssignProfileUserFailForStoppedUser() throws Exception {
        setUpServiceWithProfileSupportEnabled();
        mService.init();
        mService.setCarServiceHelper(mICarServiceHelper);

        assertPassengerDisplaysFromDefaultConfig();

        mICarServiceHelper.mWhitelists.clear();
        doReturn(false).when(mUserManager).isUserRunning(PROFILE_USER1);
        assertThat(mManager.assignProfileUserToOccupantZone(mZoneFrontPassengerLHD,
                PROFILE_USER1)).isFalse();
    }

    @Test
    public void testAssignProfileUserSwitch() throws Exception {
        setUpServiceWithProfileSupportEnabled();
        mService.init();
        mService.setCarServiceHelper(mICarServiceHelper);

        assertThat(mManager.assignProfileUserToOccupantZone(mZoneFrontPassengerLHD,
                PROFILE_USER1)).isTrue();

        assertPassengerDisplaysFromDefaultConfig();
        assertDisplayWhitelist(CURRENT_USER, new int[] {mDisplay4.getDisplayId()});
        assertDisplayWhitelist(PROFILE_USER1, new int[] {mDisplay2.getDisplayId()});

        mICarServiceHelper.mWhitelists.clear();
        assertThat(mManager.assignProfileUserToOccupantZone(mZoneFrontPassengerLHD,
                PROFILE_USER2)).isTrue();
        assertPassengerDisplaysFromDefaultConfig();
        assertDisplayWhitelist(CURRENT_USER, new int[] {mDisplay4.getDisplayId()});
        assertDisplayWhitelist(PROFILE_USER2, new int[] {mDisplay2.getDisplayId()});
    }

    @Test
    public void testAssignProfileFollowedByUserSwitch() throws Exception {
        setUpServiceWithProfileSupportEnabled();
        mService.init();
        mService.setCarServiceHelper(mICarServiceHelper);

        assertThat(mManager.assignProfileUserToOccupantZone(mZoneFrontPassengerLHD,
                PROFILE_USER1)).isTrue();

        assertPassengerDisplaysFromDefaultConfig();
        assertDisplayWhitelist(CURRENT_USER, new int[] {mDisplay4.getDisplayId()});
        assertDisplayWhitelist(PROFILE_USER1, new int[] {mDisplay2.getDisplayId()});

        mICarServiceHelper.mWhitelists.clear();
        int newUserId = 200;
        doReturn(newUserId).when(mService).getCurrentUser();
        mService.mUserLifecycleListener.onEvent(new UserLifecycleEvent(
                CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING, newUserId));

        assertPassengerDisplaysFromDefaultConfig();
        assertDisplayWhitelist(newUserId, new int[] {mDisplay2.getDisplayId(),
                mDisplay4.getDisplayId()});
        assertThat(mICarServiceHelper.mWhitelists).hasSize(1);
    }

    @Test
    public void testAssignProfileFollowedByNullUserAssignment() throws Exception {
        setUpServiceWithProfileSupportEnabled();
        mService.init();
        mService.setCarServiceHelper(mICarServiceHelper);

        assertThat(mManager.assignProfileUserToOccupantZone(mZoneFrontPassengerLHD,
                PROFILE_USER1)).isTrue();

        assertPassengerDisplaysFromDefaultConfig();
        assertDisplayWhitelist(CURRENT_USER, new int[] {mDisplay4.getDisplayId()});
        assertDisplayWhitelist(PROFILE_USER1, new int[] {mDisplay2.getDisplayId()});

        mICarServiceHelper.mWhitelists.clear();
        assertThat(mManager.assignProfileUserToOccupantZone(mZoneFrontPassengerLHD,
                UserHandle.USER_NULL)).isTrue();
        assertPassengerDisplaysFromDefaultConfig();
        assertDisplayWhitelist(CURRENT_USER, new int[] {mDisplay2.getDisplayId(),
                mDisplay4.getDisplayId()});
        assertThat(mICarServiceHelper.mWhitelists).hasSize(1);
    }

    @Test
    public void testCarServiceHelperInitialUpdate() throws Exception {
        setUpServiceWithProfileSupportEnabled();
        mService.init();
        mService.setCarServiceHelper(mICarServiceHelper);

        assertPassengerDisplaysFromDefaultConfig();
        assertDisplayWhitelist(CURRENT_USER, new int[] {mDisplay2.getDisplayId(),
                mDisplay4.getDisplayId()});
        assertThat(mICarServiceHelper.mWhitelists).hasSize(1);
    }

    private void assertDisplayInfoIncluded(
            ArrayList<DisplayInfo> displayInfos, Display display, int displayType) {
        for (DisplayInfo info : displayInfos) {
            if (info.display == display && info.displayType == displayType) {
                return;
            }
        }
        fail("Cannot find display:" + display + " type:" + displayType);
    }

    private void assertOccupantConfig(OccupantConfig c, int userId, Display[] displays,
            int[] displayTypes) {
        assertThat(userId).isEqualTo(c.userId);
        assertThat(c.displayInfos).hasSize(displays.length);
        assertThat(c.displayInfos).hasSize(displayTypes.length);
        for (int i = 0; i < displays.length; i++) {
            assertDisplayInfoIncluded(c.displayInfos, displays[i], displayTypes[i]);
        }
    }

    @Test
    public void testSetAudioConfigMapping() {
        mService.init();

        SparseIntArray audioZoneIdToOccupantZoneMapping =
                getDefaultAudioZoneToOccupantZoneMapping();

        mService.setAudioZoneIdsForOccupantZoneIds(audioZoneIdToOccupantZoneMapping);

        assertThat(mService.getAudioZoneIdForOccupant(PRIMARY_AUDIO_ZONE_ID_OCCUPANT))
                .isEqualTo(PRIMARY_AUDIO_ZONE_ID);

        assertThat(mService.getAudioZoneIdForOccupant(SECONDARY_AUDIO_ZONE_ID_OCCUPANT))
                .isEqualTo(SECONDARY_AUDIO_ZONE_ID);
    }

    private SparseIntArray getDefaultAudioZoneToOccupantZoneMapping() {
        SparseIntArray audioZoneIdToOccupantZoneMapping = new SparseIntArray(2);
        audioZoneIdToOccupantZoneMapping.put(PRIMARY_AUDIO_ZONE_ID,
                PRIMARY_AUDIO_ZONE_ID_OCCUPANT);
        audioZoneIdToOccupantZoneMapping.put(SECONDARY_AUDIO_ZONE_ID,
                SECONDARY_AUDIO_ZONE_ID_OCCUPANT);
        return audioZoneIdToOccupantZoneMapping;
    }

    @Test
    public void testOccupantZoneConfigInfoForAudio() {
        mService.init();
        SparseIntArray audioZoneIdToOccupantZoneMapping =
                getDefaultAudioZoneToOccupantZoneMapping();

        HashMap<Integer, CarOccupantZoneManager.OccupantZoneInfo> occupantZoneConfigs =
                mService.getOccupantsConfig();

        mService.setAudioZoneIdsForOccupantZoneIds(audioZoneIdToOccupantZoneMapping);

        CarOccupantZoneManager.OccupantZoneInfo primaryOccupantInfo =
                mService.getOccupantForAudioZoneId(PRIMARY_AUDIO_ZONE_ID);
        assertThat(primaryOccupantInfo).isEqualTo(
                occupantZoneConfigs.get(PRIMARY_AUDIO_ZONE_ID_OCCUPANT));

        CarOccupantZoneManager.OccupantZoneInfo secondaryOccupantInfo =
                mService.getOccupantForAudioZoneId(SECONDARY_AUDIO_ZONE_ID);
        assertThat(secondaryOccupantInfo).isEqualTo(
                occupantZoneConfigs.get(SECONDARY_AUDIO_ZONE_ID_OCCUPANT));

        CarOccupantZoneManager.OccupantZoneInfo nullOccupantInfo =
                mService.getOccupantForAudioZoneId(UNMAPPED_AUDIO_ZONE_ID_OCCUPANT);
        assertThat(nullOccupantInfo).isNull();
    }

    @Test
    public void testMissingAudioConfigMapping() {
        mService.init();
        SparseIntArray audioZoneIdToOccupantZoneMapping =
                getDefaultAudioZoneToOccupantZoneMapping();

        mService.setAudioZoneIdsForOccupantZoneIds(audioZoneIdToOccupantZoneMapping);

        assertThat(mService.getAudioZoneIdForOccupant(UNMAPPED_AUDIO_ZONE_ID_OCCUPANT))
                .isEqualTo(CarAudioManager.INVALID_AUDIO_ZONE);
    }

    @Test
    public void testSetInvalidAudioConfigMapping() {
        mService.init();
        SparseIntArray audioZoneIdToOccupantZoneMapping = new SparseIntArray(2);
        audioZoneIdToOccupantZoneMapping.put(PRIMARY_AUDIO_ZONE_ID,
                PRIMARY_AUDIO_ZONE_ID_OCCUPANT);
        audioZoneIdToOccupantZoneMapping.put(SECONDARY_AUDIO_ZONE_ID,
                INVALID_AUDIO_ZONE_ID_OCCUPANT);
        IllegalArgumentException thrown =
                expectThrows(IllegalArgumentException.class,
                        () -> mService.setAudioZoneIdsForOccupantZoneIds(
                                audioZoneIdToOccupantZoneMapping));
        thrown.getMessage().contains("does not exist");
    }

    @Test
    public void testActiveOccupantConfigs() {
        mService.init();

        // key : zone id
        HashMap<Integer, OccupantConfig> configs = mService.getActiveOccupantConfigs();
        assertThat(configs).hasSize(3); // driver, front passenger, one rear
        assertOccupantConfig(configs.get(0), CURRENT_USER, new Display[]{mDisplay0, mDisplay1},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN,
                        CarOccupantZoneManager.DISPLAY_TYPE_INSTRUMENT_CLUSTER});
        assertOccupantConfig(configs.get(1), CURRENT_USER, new Display[]{mDisplay2},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN});
        assertOccupantConfig(configs.get(3), CURRENT_USER, new Display[]{mDisplay4},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN});
    }

    @Test
    public void testActiveOccupantConfigsAfterDisplayAdd() {
        mService.init();

        mockDisplay(mDisplayManager, mDisplay3, 3, 13);
        when(mDisplayManager.getDisplays()).thenReturn(new Display[]{
                mDisplay0,
                mDisplay1,
                mDisplay2,
                mDisplay3,
                mDisplay4,
                mDisplay5
        });
        mService.mDisplayListener.onDisplayAdded(3);

        // key : zone id
        HashMap<Integer, OccupantConfig> configs = mService.getActiveOccupantConfigs();
        assertThat(configs).hasSize(4); // driver, front passenger, two rear
        assertOccupantConfig(configs.get(0), CURRENT_USER, new Display[]{mDisplay0, mDisplay1},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN,
                        CarOccupantZoneManager.DISPLAY_TYPE_INSTRUMENT_CLUSTER});
        assertOccupantConfig(configs.get(1), CURRENT_USER, new Display[]{mDisplay2},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN});
        assertOccupantConfig(configs.get(2), CURRENT_USER, new Display[]{mDisplay3},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN});
        assertOccupantConfig(configs.get(3), CURRENT_USER, new Display[]{mDisplay4},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN});
    }

    @Test
    public void testActiveOccupantConfigsAfterDisplayRemoval() {
        mService.init();

        when(mDisplayManager.getDisplays()).thenReturn(new Display[]{
                mDisplay0,
                mDisplay1,
                mDisplay2,
        });
        mService.mDisplayListener.onDisplayRemoved(4);

        // key : zone id
        HashMap<Integer, OccupantConfig> configs = mService.getActiveOccupantConfigs();
        assertThat(configs).hasSize(2); // driver, front passenger
        assertOccupantConfig(configs.get(0), CURRENT_USER, new Display[]{mDisplay0, mDisplay1},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN,
                        CarOccupantZoneManager.DISPLAY_TYPE_INSTRUMENT_CLUSTER});
        assertOccupantConfig(configs.get(1), CURRENT_USER, new Display[]{mDisplay2},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN});
    }

    @Test
    public void testActiveUserAfterUserSwitching() {
        mService.init();

        final int newUserId = 200;
        doReturn(newUserId).when(mService).getCurrentUser();
        mService.mUserLifecycleListener.onEvent(new UserLifecycleEvent(
                CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING, newUserId));

        // key : zone id
        HashMap<Integer, OccupantConfig> configs = mService.getActiveOccupantConfigs();
        assertThat(configs).hasSize(3); // driver, front passenger, one rear
        assertOccupantConfig(configs.get(0), newUserId, new Display[]{mDisplay0, mDisplay1},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN,
                        CarOccupantZoneManager.DISPLAY_TYPE_INSTRUMENT_CLUSTER});
        assertOccupantConfig(configs.get(1), newUserId, new Display[]{mDisplay2},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN});
        assertOccupantConfig(configs.get(3), newUserId, new Display[]{mDisplay4},
                new int[]{CarOccupantZoneManager.DISPLAY_TYPE_MAIN});
    }

    private void assertParsingFailure() {
        assertThrows(Exception.class, () -> mService.init());
        // call release to return it to clean state.
        mService.release();
    }

    @Test
    public void testWrongZoneConfigs() {
        final String[] wrongZoneConfigs = {
                "unknownKeyword",
                "unknownKey=0",
                "occupantZoneId=0,occupantType=Unknown,seatRow=1,seatSide=driver",
                "occupantZoneId=0,occupantType=DRIVER,seatRow=0,seatSide=driver", // wrong row
                "occupantZoneId=0,occupantType=DRIVER,seatRow=1,seatSide=wrongSide"
        };

        String[] zoneConfig = new String[1];
        when(mResources.getStringArray(R.array.config_occupant_zones))
                .thenReturn(zoneConfig);
        for (int i = 0; i < wrongZoneConfigs.length; i++) {
            zoneConfig[0] = wrongZoneConfigs[i];
            assertParsingFailure();
        }
    }

    @Test
    public void testWrongDisplayConfigs() {
        final String[] wrongDisplayConfigs = {
                "unknownKeyword",
                "unknownKey=0",
                "displayPort=10,displayType=Unknown,occupantZoneId=0",
                "displayPort=10,displayType=MAIN,occupantZoneId=100" // wrong zone id
        };

        String[] displayConfig = new String[1];
        when(mResources.getStringArray(R.array.config_occupant_display_mapping))
                .thenReturn(displayConfig);
        for (int i = 0; i < wrongDisplayConfigs.length; i++) {
            displayConfig[0] = wrongDisplayConfigs[i];
            assertParsingFailure();
        }
    }

    @Test
    public void testManagerGetAllOccupantZones() {
        mService.init();

        List<OccupantZoneInfo> infos = mManager.getAllOccupantZones();
        assertThat(infos).hasSize(3);
        assertThat(infos).contains(mZoneDriverLHD);
        assertThat(infos).contains(mZoneFrontPassengerLHD);
        assertThat(infos).contains(mZoneRearRight);
    }

    @Test
    public void testManagerGetAllDisplaysForOccupant() {
        mService.init();

        List<Display> displaysForDriver = mManager.getAllDisplaysForOccupant(mZoneDriverLHD);
        assertThat(displaysForDriver).hasSize(2);
        assertThat(displaysForDriver).contains(mDisplay0);
        assertThat(displaysForDriver).contains(mDisplay1);

        List<Display> displaysForFrontPassenger = mManager.getAllDisplaysForOccupant(
                mZoneFrontPassengerLHD);
        assertThat(displaysForFrontPassenger).hasSize(1);
        assertThat(displaysForFrontPassenger).contains(mDisplay2);

        List<Display> displaysForRearLeft = mManager.getAllDisplaysForOccupant(
                mZoneRearLeft);
        assertThat(displaysForRearLeft).hasSize(0);

        List<Display> displaysForRearRight = mManager.getAllDisplaysForOccupant(
                mZoneRearRight);
        assertThat(displaysForRearRight).hasSize(1);
        assertThat(displaysForRearRight).contains(mDisplay4);
    }

    @Test
    public void testManagerGetAllDisplaysForOccupantAfterDisplayAdd() {
        mService.init();

        mockDisplay(mDisplayManager, mDisplay3, 3, 13);
        when(mDisplayManager.getDisplays()).thenReturn(new Display[]{
                mDisplay0,
                mDisplay1,
                mDisplay2,
                mDisplay3,
                mDisplay4,
                mDisplay5
        });
        mService.mDisplayListener.onDisplayAdded(3);

        List<Display> displaysForDriver = mManager.getAllDisplaysForOccupant(mZoneDriverLHD);
        assertThat(displaysForDriver).hasSize(2);
        assertThat(displaysForDriver).contains(mDisplay0);
        assertThat(displaysForDriver).contains(mDisplay1);

        List<Display> displaysForFrontPassenger = mManager.getAllDisplaysForOccupant(
                mZoneFrontPassengerLHD);
        assertThat(displaysForFrontPassenger).hasSize(1);
        assertThat(displaysForFrontPassenger).contains(mDisplay2);

        List<Display> displaysForRearLeft = mManager.getAllDisplaysForOccupant(
                mZoneRearLeft);
        assertThat(displaysForRearLeft).hasSize(1);
        assertThat(displaysForRearLeft).contains(mDisplay3);

        List<Display> displaysForRearRight = mManager.getAllDisplaysForOccupant(
                mZoneRearRight);
        assertThat(displaysForRearRight).hasSize(1);
        assertThat(displaysForRearRight).contains(mDisplay4);
    }

    @Test
    public void testManagerGetAllDisplaysForOccupantAfterDisplayRemoval() {
        mService.init();

        when(mDisplayManager.getDisplays()).thenReturn(new Display[]{
                mDisplay0,
                mDisplay1,
                mDisplay2,
        });
        mService.mDisplayListener.onDisplayRemoved(4);

        List<Display> displaysForDriver = mManager.getAllDisplaysForOccupant(mZoneDriverLHD);
        assertThat(displaysForDriver).hasSize(2);
        assertThat(displaysForDriver).contains(mDisplay0);
        assertThat(displaysForDriver).contains(mDisplay1);

        List<Display> displaysForFrontPassenger = mManager.getAllDisplaysForOccupant(
                mZoneFrontPassengerLHD);
        assertThat(displaysForFrontPassenger).hasSize(1);
        assertThat(displaysForFrontPassenger).contains(mDisplay2);

        List<Display> displaysForRearLeft = mManager.getAllDisplaysForOccupant(
                mZoneRearLeft);
        assertThat(displaysForRearLeft).hasSize(0);

        List<Display> displaysForRearRight = mManager.getAllDisplaysForOccupant(
                mZoneRearRight);
        assertThat(displaysForRearRight).hasSize(0);
    }

    @Test
    public void testManagerGetDisplayForOccupant() {
        mService.init();

        assertThat(mDisplay0).isEqualTo(mManager.getDisplayForOccupant(mZoneDriverLHD,
                CarOccupantZoneManager.DISPLAY_TYPE_MAIN));
        assertThat(mDisplay1).isEqualTo(mManager.getDisplayForOccupant(mZoneDriverLHD,
                CarOccupantZoneManager.DISPLAY_TYPE_INSTRUMENT_CLUSTER));
        assertThat(mManager.getDisplayForOccupant(mZoneDriverLHD,
                CarOccupantZoneManager.DISPLAY_TYPE_HUD)).isNull();

        assertThat(mDisplay2).isEqualTo(mManager.getDisplayForOccupant(mZoneFrontPassengerLHD,
                CarOccupantZoneManager.DISPLAY_TYPE_MAIN));

        assertThat(mManager.getDisplayForOccupant(mZoneRearLeft,
                CarOccupantZoneManager.DISPLAY_TYPE_MAIN)).isNull();

        assertThat(mDisplay4).isEqualTo(mManager.getDisplayForOccupant(mZoneRearRight,
                CarOccupantZoneManager.DISPLAY_TYPE_MAIN));
    }

    @Test
    public void testManagerGetDisplayType() {
        mService.init();

        assertThat(mManager.getDisplayType(mDisplay0)).isEqualTo(
                CarOccupantZoneManager.DISPLAY_TYPE_MAIN);
        assertThat(mManager.getDisplayType(mDisplay1)).isEqualTo(
                CarOccupantZoneManager.DISPLAY_TYPE_INSTRUMENT_CLUSTER);
        assertThat(mManager.getDisplayType(mDisplay2)).isEqualTo(
                CarOccupantZoneManager.DISPLAY_TYPE_MAIN);
        assertThat(mManager.getDisplayType(mDisplay4)).isEqualTo(
                CarOccupantZoneManager.DISPLAY_TYPE_MAIN);
        assertThat(mManager.getDisplayType(mDisplay5)).isEqualTo(
                CarOccupantZoneManager.DISPLAY_TYPE_UNKNOWN);
    }

    @Test
    public void testManagerGetUserForOccupant() {
        mService.init();

        int driverUser = mManager.getUserForOccupant(mZoneDriverLHD);
        assertThat(CURRENT_USER).isEqualTo(driverUser);

        //TODO update this after secondary user handling
        assertThat(mManager.getUserForOccupant(mZoneFrontPassengerLHD)).isEqualTo(driverUser);
        assertThat(mManager.getUserForOccupant(mZoneRearLeft)).isEqualTo(UserHandle.USER_NULL);
        assertThat(mManager.getUserForOccupant(mZoneRearRight)).isEqualTo(driverUser);
    }

    @Test
    public void testManagerGetUserForOccupantAfterUserSwitch() {
        mService.init();

        final int newUserId = 200;
        doReturn(newUserId).when(mService).getCurrentUser();
        mService.mUserLifecycleListener.onEvent(new UserLifecycleEvent(
                CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING, newUserId));

        assertThat(newUserId).isEqualTo(mManager.getUserForOccupant(mZoneDriverLHD));
        //TODO update this after secondary user handling
        assertThat(mManager.getUserForOccupant(mZoneFrontPassengerLHD)).isEqualTo(newUserId);
        assertThat(mManager.getUserForOccupant(mZoneRearLeft)).isEqualTo(UserHandle.USER_NULL);
        assertThat(mManager.getUserForOccupant(mZoneRearRight)).isEqualTo(newUserId);
    }

    @Test
    public void testManagerRegisterUnregister() {
        mService.init();

        final long eventWaitTimeMs = 300;

        mManager.registerOccupantZoneConfigChangeListener(mChangeListener);

        resetConfigChangeEventWait();
        mService.mUserLifecycleListener.onEvent(new UserLifecycleEvent(
                CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING, 0)); // user id does not matter.

        assertThat(waitForConfigChangeEventAndAssertFlag(eventWaitTimeMs,
                CarOccupantZoneManager.ZONE_CONFIG_CHANGE_FLAG_USER)).isTrue();

        resetConfigChangeEventWait();
        mService.mDisplayListener.onDisplayAdded(0); // displayid ignored
        assertThat(waitForConfigChangeEventAndAssertFlag(eventWaitTimeMs,
                CarOccupantZoneManager.ZONE_CONFIG_CHANGE_FLAG_DISPLAY)).isTrue();

        resetConfigChangeEventWait();
        mManager.unregisterOccupantZoneConfigChangeListener(mChangeListener);
        mService.mUserLifecycleListener.onEvent(new UserLifecycleEvent(
                CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING, 0));
        assertThat(waitForConfigChangeEventAndAssertFlag(eventWaitTimeMs, 0)).isFalse();
    }

    @Test
    public void testManagerRegisterUnregisterForAudioConfigs() {
        mService.init();

        long eventWaitTimeMs = 300;

        mManager.registerOccupantZoneConfigChangeListener(mChangeListener);

        resetConfigChangeEventWait();

        SparseIntArray audioZoneIdToOccupantZoneMapping =
                getDefaultAudioZoneToOccupantZoneMapping();

        mService.setAudioZoneIdsForOccupantZoneIds(audioZoneIdToOccupantZoneMapping);

        assertThat(waitForConfigChangeEventAndAssertFlag(eventWaitTimeMs,
                CarOccupantZoneManager.ZONE_CONFIG_CHANGE_FLAG_AUDIO)).isTrue();

        resetConfigChangeEventWait();
        mManager.unregisterOccupantZoneConfigChangeListener(mChangeListener);
        mService.setAudioZoneIdsForOccupantZoneIds(audioZoneIdToOccupantZoneMapping);
        assertThat(waitForConfigChangeEventAndAssertFlag(eventWaitTimeMs,
                CarOccupantZoneManager.ZONE_CONFIG_CHANGE_FLAG_AUDIO)).isFalse();
    }

    private static class ICarServiceHelperImpl extends ICarServiceHelper.Stub {
        private List<Integer> mPassengerDisplayIds;

        /** key: user id, value: display whitelistis */
        private HashMap<Integer, List<Integer>> mWhitelists = new HashMap<>();

        @Override
        public int forceSuspend(int timeoutMs) {
            return 0;
        }

        @Override
        public void setDisplayWhitelistForUser(@UserIdInt int userId, int[] displayIds) {
            mWhitelists.put(userId, Arrays.stream(displayIds).boxed().collect(Collectors.toList()));
        }

        @Override
        public void setPassengerDisplays(int[] displayIdsForPassenger) {
            mPassengerDisplayIds = Arrays.stream(displayIdsForPassenger).boxed().collect(
                    Collectors.toList());
        }

        @Override
        public void setSourcePreferredComponents(boolean enableSourcePreferred,
                List<ComponentName> sourcePreferredComponents) throws RemoteException {
        }
    }
}
