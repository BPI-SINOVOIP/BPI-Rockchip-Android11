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
package com.android.car.audio;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.testng.Assert.expectThrows;

import android.app.ActivityManager;
import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.os.UserHandle;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.google.common.primitives.Ints;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@RunWith(AndroidJUnit4.class)
public class CarVolumeGroupTest extends AbstractExtendedMockitoTestCase{
    private static final int STEP_VALUE = 2;
    private static final int MIN_GAIN = 0;
    private static final int MAX_GAIN = 5;
    private static final int DEFAULT_GAIN = 0;
    private static final int TEST_USER_10 = 10;
    private static final int TEST_USER_11 = 11;
    private static final String OTHER_ADDRESS = "other_address";
    private static final String MEDIA_DEVICE_ADDRESS = "music";
    private static final String NAVIGATION_DEVICE_ADDRESS = "navigation";


    private CarAudioDeviceInfo mMediaDevice;
    private CarAudioDeviceInfo mNavigationDevice;

    @Override
    protected void onSessionBuilder(CustomMockitoSessionBuilder session) {
        session.spyStatic(ActivityManager.class);
    }

    @Before
    public void setUp() {
        mMediaDevice = generateCarAudioDeviceInfo(MEDIA_DEVICE_ADDRESS);
        mNavigationDevice = generateCarAudioDeviceInfo(NAVIGATION_DEVICE_ADDRESS);
    }

    @Test
    public void bind_associatesDeviceAddresses() {
        CarAudioSettings settings =
                generateCarAudioSettings(0 , 0, 2);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);

        carVolumeGroup.bind(CarAudioContext.MUSIC, mMediaDevice);
        assertEquals(1, carVolumeGroup.getAddresses().size());

        carVolumeGroup.bind(CarAudioContext.NAVIGATION, mNavigationDevice);

        List<String> addresses = carVolumeGroup.getAddresses();
        assertEquals(2, addresses.size());
        assertTrue(addresses.contains(MEDIA_DEVICE_ADDRESS));
        assertTrue(addresses.contains(NAVIGATION_DEVICE_ADDRESS));
    }

    @Test
    public void bind_checksForSameStepSize() {
        CarAudioSettings settings =
                generateCarAudioSettings(0 , 0, 2);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);

        carVolumeGroup.bind(CarAudioContext.MUSIC, mMediaDevice);
        CarAudioDeviceInfo differentStepValueDevice = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, STEP_VALUE + 1,
                MIN_GAIN, MAX_GAIN);

        IllegalArgumentException thrown = expectThrows(IllegalArgumentException.class,
                () -> carVolumeGroup.bind(CarAudioContext.NAVIGATION, differentStepValueDevice));
        assertThat(thrown).hasMessageThat()
                .contains("Gain controls within one group must have same step value");
    }

    @Test
    public void bind_updatesMinGainToSmallestValue() {
        CarAudioSettings settings =
                generateCarAudioSettings(0 , 0, 2);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);

        CarAudioDeviceInfo largestMinGain = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, 1, 10, 10);
        carVolumeGroup.bind(CarAudioContext.NAVIGATION, largestMinGain);

        assertEquals(0, carVolumeGroup.getMaxGainIndex());

        CarAudioDeviceInfo smallestMinGain = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, 1, 2, 10);
        carVolumeGroup.bind(CarAudioContext.NOTIFICATION, smallestMinGain);

        assertEquals(8, carVolumeGroup.getMaxGainIndex());

        CarAudioDeviceInfo middleMinGain = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, 1, 7, 10);
        carVolumeGroup.bind(CarAudioContext.VOICE_COMMAND, middleMinGain);

        assertEquals(8, carVolumeGroup.getMaxGainIndex());
    }

    @Test
    public void bind_updatesMaxGainToLargestValue() {
        CarAudioSettings settings =
                generateCarAudioSettings(0 , 0, 2);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);

        CarAudioDeviceInfo smallestMaxGain = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, 1, 1, 5);
        carVolumeGroup.bind(CarAudioContext.NAVIGATION, smallestMaxGain);

        assertEquals(4, carVolumeGroup.getMaxGainIndex());

        CarAudioDeviceInfo largestMaxGain = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, 1, 1, 10);
        carVolumeGroup.bind(CarAudioContext.NOTIFICATION, largestMaxGain);

        assertEquals(9, carVolumeGroup.getMaxGainIndex());

        CarAudioDeviceInfo middleMaxGain = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, 1, 1, 7);
        carVolumeGroup.bind(CarAudioContext.VOICE_COMMAND, middleMaxGain);

        assertEquals(9, carVolumeGroup.getMaxGainIndex());
    }

    @Test
    public void bind_checksThatTheSameContextIsNotBoundTwice() {
        CarAudioSettings settings =
                generateCarAudioSettings(0 , 0, 2);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);

        carVolumeGroup.bind(CarAudioContext.NAVIGATION, mMediaDevice);

        IllegalArgumentException thrown = expectThrows(IllegalArgumentException.class,
                () -> carVolumeGroup.bind(CarAudioContext.NAVIGATION, mMediaDevice));
        assertThat(thrown).hasMessageThat()
                .contains("Context NAVIGATION has already been bound to " + MEDIA_DEVICE_ADDRESS);
    }

    @Test
    public void getContexts_returnsAllContextsBoundToVolumeGroup() {
        CarVolumeGroup carVolumeGroup = testVolumeGroupSetup();

        int[] contexts = carVolumeGroup.getContexts();

        assertEquals(6, contexts.length);

        List<Integer> contextsList = Ints.asList(contexts);
        assertTrue(contextsList.contains(CarAudioContext.MUSIC));
        assertTrue(contextsList.contains(CarAudioContext.CALL));
        assertTrue(contextsList.contains(CarAudioContext.CALL_RING));
        assertTrue(contextsList.contains(CarAudioContext.NAVIGATION));
        assertTrue(contextsList.contains(CarAudioContext.ALARM));
        assertTrue(contextsList.contains(CarAudioContext.NOTIFICATION));
    }

    @Test
    public void getContextsForAddress_returnsContextsBoundToThatAddress() {
        CarVolumeGroup carVolumeGroup = testVolumeGroupSetup();

        int[] contexts = carVolumeGroup.getContextsForAddress(MEDIA_DEVICE_ADDRESS);

        assertEquals(3, contexts.length);
        List<Integer> contextsList = Ints.asList(contexts);
        assertTrue(contextsList.contains(CarAudioContext.MUSIC));
        assertTrue(contextsList.contains(CarAudioContext.CALL));
        assertTrue(contextsList.contains(CarAudioContext.CALL_RING));
    }

    @Test
    public void getContextsForAddress_returnsEmptyArrayIfAddressNotBound() {
        CarVolumeGroup carVolumeGroup = testVolumeGroupSetup();

        int[] contexts = carVolumeGroup.getContextsForAddress(OTHER_ADDRESS);

        assertEquals(0, contexts.length);
    }

    @Test
    public void getCarAudioDeviceInfoForAddress_returnsExpectedDevice() {
        CarVolumeGroup carVolumeGroup = testVolumeGroupSetup();

        CarAudioDeviceInfo actualDevice = carVolumeGroup.getCarAudioDeviceInfoForAddress(
                MEDIA_DEVICE_ADDRESS);

        assertEquals(mMediaDevice, actualDevice);
    }

    @Test
    public void getCarAudioDeviceInfoForAddress_returnsNullIfAddressNotBound() {
        CarVolumeGroup carVolumeGroup = testVolumeGroupSetup();

        CarAudioDeviceInfo actualDevice = carVolumeGroup.getCarAudioDeviceInfoForAddress(
                OTHER_ADDRESS);

        assertNull(actualDevice);
    }

    @Test
    public void setCurrentGainIndex_setsGainOnAllBoundDevices() {
        CarVolumeGroup carVolumeGroup = testVolumeGroupSetup();

        carVolumeGroup.setCurrentGainIndex(2);
        verify(mMediaDevice).setCurrentGain(4);
        verify(mNavigationDevice).setCurrentGain(4);
    }

    @Test
    public void setCurrentGainIndex_updatesCurrentGainIndex() {
        CarVolumeGroup carVolumeGroup = testVolumeGroupSetup();

        carVolumeGroup.setCurrentGainIndex(2);

        assertEquals(2, carVolumeGroup.getCurrentGainIndex());
    }

    @Test
    public void setCurrentGainIndex_checksNewGainIsAboveMin() {
        CarVolumeGroup carVolumeGroup = testVolumeGroupSetup();

        IllegalArgumentException thrown = expectThrows(IllegalArgumentException.class,
                () -> carVolumeGroup.setCurrentGainIndex(-1));
        assertThat(thrown).hasMessageThat().contains("Gain out of range (0:5) -2index -1");
    }

    @Test
    public void setCurrentGainIndex_checksNewGainIsBelowMax() {
        CarVolumeGroup carVolumeGroup = testVolumeGroupSetup();

        IllegalArgumentException thrown = expectThrows(IllegalArgumentException.class,
                () -> carVolumeGroup.setCurrentGainIndex(3));
        assertThat(thrown).hasMessageThat().contains("Gain out of range (0:5) 6index 3");
    }

    @Test
    public void getMinGainIndex_alwaysReturnsZero() {

        CarAudioSettings settings =
                generateCarAudioSettings(0 , 0, 2);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);
        CarAudioDeviceInfo minGainPlusOneDevice = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, STEP_VALUE, 10, MAX_GAIN);
        carVolumeGroup.bind(CarAudioContext.NAVIGATION, minGainPlusOneDevice);

        assertEquals(0, carVolumeGroup.getMinGainIndex());

        CarAudioDeviceInfo minGainDevice = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, STEP_VALUE, 1, MAX_GAIN);
        carVolumeGroup.bind(CarAudioContext.NOTIFICATION, minGainDevice);

        assertEquals(0, carVolumeGroup.getMinGainIndex());
    }

    @Test
    public void loadVolumesForUser_setsCurrentGainIndexForUser() {

        List<Integer> users = new ArrayList<>();
        users.add(TEST_USER_10);
        users.add(TEST_USER_11);

        Map<Integer, Integer> storedGainIndex = new HashMap<>();
        storedGainIndex.put(TEST_USER_10, 2);
        storedGainIndex.put(TEST_USER_11, 0);

        CarAudioSettings settings =
                generateCarAudioSettings(users, 0 , 0, storedGainIndex);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);

        CarAudioDeviceInfo deviceInfo = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, STEP_VALUE, MIN_GAIN, MAX_GAIN);
        carVolumeGroup.bind(CarAudioContext.NAVIGATION, deviceInfo);
        carVolumeGroup.loadVolumesForUser(TEST_USER_10);

        assertEquals(2, carVolumeGroup.getCurrentGainIndex());

        carVolumeGroup.loadVolumesForUser(TEST_USER_11);

        assertEquals(0, carVolumeGroup.getCurrentGainIndex());
    }

    @Test
    public void loadUserStoredGainIndex_setsCurrentGainIndexToDefault() {
        CarAudioSettings settings =
                generateCarAudioSettings(TEST_USER_10, 0, 0, 10);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);

        CarAudioDeviceInfo deviceInfo = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, STEP_VALUE, MIN_GAIN, MAX_GAIN);
        carVolumeGroup.bind(CarAudioContext.NAVIGATION, deviceInfo);

        carVolumeGroup.setCurrentGainIndex(2);

        assertEquals(2, carVolumeGroup.getCurrentGainIndex());

        carVolumeGroup.loadVolumesForUser(0);

        assertEquals(0, carVolumeGroup.getCurrentGainIndex());
    }

    @Test
    public void setCurrentGainIndex_setsCurrentGainIndexForUser() {
        List<Integer> users = new ArrayList<>();
        users.add(TEST_USER_11);

        Map<Integer, Integer> storedGainIndex = new HashMap<>();
        storedGainIndex.put(TEST_USER_11, 2);

        CarAudioSettings settings =
                generateCarAudioSettings(users, 0 , 0, storedGainIndex);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);

        CarAudioDeviceInfo deviceInfo = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, STEP_VALUE, MIN_GAIN, MAX_GAIN);
        carVolumeGroup.bind(CarAudioContext.NAVIGATION, deviceInfo);
        carVolumeGroup.loadVolumesForUser(TEST_USER_11);

        carVolumeGroup.setCurrentGainIndex(MIN_GAIN);

        verify(settings).storeVolumeGainIndexForUser(TEST_USER_11, 0, 0, MIN_GAIN);
    }

    @Test
    public void setCurrentGainIndex_setsCurrentGainIndexForDefaultUser() {
        List<Integer> users = new ArrayList<>();
        users.add(UserHandle.USER_CURRENT);

        Map<Integer, Integer> storedGainIndex = new HashMap<>();
        storedGainIndex.put(UserHandle.USER_CURRENT, 2);

        CarAudioSettings settings =
                generateCarAudioSettings(users, 0 , 0, storedGainIndex);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);

        CarAudioDeviceInfo deviceInfo = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, STEP_VALUE, MIN_GAIN, MAX_GAIN);
        carVolumeGroup.bind(CarAudioContext.NAVIGATION, deviceInfo);

        carVolumeGroup.setCurrentGainIndex(MIN_GAIN);

        verify(settings)
                .storeVolumeGainIndexForUser(UserHandle.USER_CURRENT, 0, 0, MIN_GAIN);
    }

    @Test
    public void bind_setsCurrentGainIndexToStoredGainIndex() {
        CarAudioSettings settings =
                generateCarAudioSettings(0 , 0, 2);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);

        CarAudioDeviceInfo deviceInfo = generateCarAudioDeviceInfo(
                NAVIGATION_DEVICE_ADDRESS, STEP_VALUE, MIN_GAIN, MAX_GAIN);
        carVolumeGroup.bind(CarAudioContext.NAVIGATION, deviceInfo);


        assertEquals(2, carVolumeGroup.getCurrentGainIndex());
    }

    @Test
    public void getAddressForContext_returnsExpectedDeviceAddress() {
        CarAudioSettings settings =
                generateCarAudioSettings(0 , 0, 2);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);


        carVolumeGroup.bind(CarAudioContext.MUSIC, mMediaDevice);

        String mediaAddress = carVolumeGroup.getAddressForContext(CarAudioContext.MUSIC);

        assertEquals(mMediaDevice.getAddress(), mediaAddress);
    }

    @Test
    public void getAddressForContext_returnsNull() {
        CarAudioSettings settings =
                generateCarAudioSettings(0 , 0, 2);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);

        String nullAddress = carVolumeGroup.getAddressForContext(CarAudioContext.MUSIC);

        assertNull(nullAddress);
    }

    private CarVolumeGroup testVolumeGroupSetup() {
        CarAudioSettings settings =
                generateCarAudioSettings(0 , 0, 2);
        CarVolumeGroup carVolumeGroup = new CarVolumeGroup(settings, 0, 0);


        carVolumeGroup.bind(CarAudioContext.MUSIC, mMediaDevice);
        carVolumeGroup.bind(CarAudioContext.CALL, mMediaDevice);
        carVolumeGroup.bind(CarAudioContext.CALL_RING, mMediaDevice);

        carVolumeGroup.bind(CarAudioContext.NAVIGATION, mNavigationDevice);
        carVolumeGroup.bind(CarAudioContext.ALARM, mNavigationDevice);
        carVolumeGroup.bind(CarAudioContext.NOTIFICATION, mNavigationDevice);

        return carVolumeGroup;
    }

    private CarAudioDeviceInfo generateCarAudioDeviceInfo(String address) {
        return generateCarAudioDeviceInfo(address, STEP_VALUE, MIN_GAIN, MAX_GAIN);
    }

    private CarAudioDeviceInfo generateCarAudioDeviceInfo(String address, int stepValue,
            int minGain, int maxGain) {
        CarAudioDeviceInfo cadiMock = Mockito.mock(CarAudioDeviceInfo.class);
        when(cadiMock.getStepValue()).thenReturn(stepValue);
        when(cadiMock.getDefaultGain()).thenReturn(DEFAULT_GAIN);
        when(cadiMock.getMaxGain()).thenReturn(maxGain);
        when(cadiMock.getMinGain()).thenReturn(minGain);
        when(cadiMock.getAddress()).thenReturn(address);
        return cadiMock;
    }

    private CarAudioSettings generateCarAudioSettings(int userId,
            int zoneId, int id, int storedGainIndex) {
        CarAudioSettings settingsMock = Mockito.mock(CarAudioSettings.class);
        when(settingsMock.getStoredVolumeGainIndexForUser(userId, zoneId, id))
                .thenReturn(storedGainIndex);

        return settingsMock;
    }

    private CarAudioSettings generateCarAudioSettings(
            int zoneId, int id, int storedGainIndex) {
        CarAudioSettings settingsMock = Mockito.mock(CarAudioSettings.class);

        when(settingsMock.getStoredVolumeGainIndexForUser(anyInt(), eq(zoneId),
                eq(id))).thenReturn(storedGainIndex);

        return settingsMock;
    }

    private CarAudioSettings generateCarAudioSettings(List<Integer> users,
            int zoneId, int id, Map<Integer, Integer> storedGainIndex) {
        CarAudioSettings settingsMock = Mockito.mock(CarAudioSettings.class);
        for (Integer user : users) {
            when(settingsMock.getStoredVolumeGainIndexForUser(user, zoneId,
                    id)).thenReturn(storedGainIndex.get(user));
        }
        return settingsMock;
    }

}
