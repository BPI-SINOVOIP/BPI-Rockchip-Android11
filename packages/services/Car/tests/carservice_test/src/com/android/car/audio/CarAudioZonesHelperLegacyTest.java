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

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;
import static org.testng.Assert.expectThrows;

import android.annotation.XmlRes;
import android.content.Context;
import android.util.SparseArray;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.R;
import com.android.car.audio.hal.AudioControlWrapperV1;

import com.google.common.collect.Lists;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

@RunWith(AndroidJUnit4.class)
public class CarAudioZonesHelperLegacyTest {
    @Rule
    public final MockitoRule rule = MockitoJUnit.rule();

    @Mock
    private AudioControlWrapperV1 mMockAudioControlWrapper;
    @Mock
    private CarAudioSettings mMockCarAudioSettings;

    private static final int INVALID_BUS = -1;
    private final Context mContext = ApplicationProvider.getApplicationContext();
    private final @XmlRes int mCarVolumeGroups = R.xml.test_car_volume_groups;

    @Test
    public void constructor_checksForNoDuplicateBusNumbers() {
        List<CarAudioDeviceInfo> carAudioDeviceInfos = getCarAudioDeviceInfoWithDuplicateBuses();

        RuntimeException exception = expectThrows(RuntimeException.class,
                () -> new CarAudioZonesHelperLegacy(mContext, mCarVolumeGroups,
                        carAudioDeviceInfos, mMockAudioControlWrapper, mMockCarAudioSettings));

        assertThat(exception.getMessage()).contains("Two addresses map to same bus number:");
    }

    @Test
    public void constructor_throwsIfLegacyContextNotAssignedToBus() throws Exception {
        List<CarAudioDeviceInfo> carAudioDeviceInfos = getValidCarAudioDeviceInfos();

        when(mMockAudioControlWrapper.getBusForContext(anyInt())).thenReturn(INVALID_BUS);

        RuntimeException exception = expectThrows(RuntimeException.class,
                () -> new CarAudioZonesHelperLegacy(mContext, mCarVolumeGroups,
                carAudioDeviceInfos, mMockAudioControlWrapper, mMockCarAudioSettings));

        assertThat(exception.getMessage()).contains("Invalid bus -1 was associated with context");
    }

    @Test
    public void loadAudioZones_succeeds() throws Exception {
        List<CarAudioDeviceInfo> carAudioDeviceInfos = getValidCarAudioDeviceInfos();
        when(mMockAudioControlWrapper.getBusForContext(anyInt())).thenReturn(1);

        CarAudioZonesHelperLegacy helper = new CarAudioZonesHelperLegacy(mContext, mCarVolumeGroups,
                carAudioDeviceInfos, mMockAudioControlWrapper, mMockCarAudioSettings);

        SparseArray<CarAudioZone> zones = helper.loadAudioZones();

        assertThat(zones.size()).isEqualTo(1);
    }

    @Test
    public void loadAudioZones_parsesAllVolumeGroups() throws Exception {
        List<CarAudioDeviceInfo> carAudioDeviceInfos = getValidCarAudioDeviceInfos();

        when(mMockAudioControlWrapper.getBusForContext(anyInt())).thenReturn(1);

        CarAudioZonesHelperLegacy helper = new CarAudioZonesHelperLegacy(mContext, mCarVolumeGroups,
                carAudioDeviceInfos, mMockAudioControlWrapper, mMockCarAudioSettings);

        SparseArray<CarAudioZone> zones = helper.loadAudioZones();
        CarVolumeGroup[] volumeGroups = zones.get(0).getVolumeGroups();
        assertThat(volumeGroups).hasLength(2);
    }

    @Test
    public void loadAudioZones_associatesLegacyContextsWithCorrectBuses() throws Exception {
        List<CarAudioDeviceInfo> carAudioDeviceInfos = getValidCarAudioDeviceInfos();

        when(mMockAudioControlWrapper.getBusForContext(anyInt())).thenReturn(2);
        when(mMockAudioControlWrapper.getBusForContext(CarAudioContext.MUSIC)).thenReturn(1);

        CarAudioZonesHelperLegacy helper = new CarAudioZonesHelperLegacy(mContext, mCarVolumeGroups,
                carAudioDeviceInfos, mMockAudioControlWrapper, mMockCarAudioSettings);

        SparseArray<CarAudioZone> zones = helper.loadAudioZones();

        CarVolumeGroup[] volumeGroups = zones.get(0).getVolumeGroups();
        CarVolumeGroup mediaVolumeGroup = volumeGroups[0];
        List<Integer> contexts = IntStream.of(mediaVolumeGroup.getContexts()).boxed().collect(
                Collectors.toList());
        assertThat(contexts).contains(CarAudioContext.MUSIC);

        CarVolumeGroup secondVolumeGroup = volumeGroups[1];
        List<Integer> secondContexts = IntStream.of(secondVolumeGroup.getContexts()).boxed()
                .collect(Collectors.toList());
        assertThat(secondContexts).containsAllOf(CarAudioContext.NAVIGATION,
                CarAudioContext.VOICE_COMMAND, CarAudioContext.CALL_RING, CarAudioContext.CALL,
                CarAudioContext.ALARM, CarAudioContext.NOTIFICATION, CarAudioContext.SYSTEM_SOUND);

    }

    @Test
    public void loadAudioZones_associatesNonLegacyContextsWithMediaBus() throws Exception {
        List<CarAudioDeviceInfo> carAudioDeviceInfos = getValidCarAudioDeviceInfos();
        when(mMockAudioControlWrapper.getBusForContext(anyInt())).thenReturn(2);
        when(mMockAudioControlWrapper.getBusForContext(CarAudioService.DEFAULT_AUDIO_CONTEXT))
                .thenReturn(1);

        CarAudioZonesHelperLegacy helper = new CarAudioZonesHelperLegacy(mContext, mCarVolumeGroups,
                carAudioDeviceInfos, mMockAudioControlWrapper, mMockCarAudioSettings);

        SparseArray<CarAudioZone> zones = helper.loadAudioZones();

        CarVolumeGroup[] volumeGroups = zones.get(0).getVolumeGroups();
        CarVolumeGroup mediaVolumeGroup = volumeGroups[0];
        List<Integer> contexts = IntStream.of(mediaVolumeGroup.getContexts()).boxed().collect(
                Collectors.toList());
        assertThat(contexts).containsAllOf(CarAudioService.DEFAULT_AUDIO_CONTEXT,
                CarAudioContext.EMERGENCY, CarAudioContext.VEHICLE_STATUS,
                CarAudioContext.ANNOUNCEMENT);
    }

    private List<CarAudioDeviceInfo> getCarAudioDeviceInfoWithDuplicateBuses() {
        CarAudioDeviceInfo deviceInfo1 = Mockito.mock(CarAudioDeviceInfo.class);
        when(deviceInfo1.getAddress()).thenReturn("bus001_media");
        CarAudioDeviceInfo deviceInfo2 = Mockito.mock(CarAudioDeviceInfo.class);
        when(deviceInfo2.getAddress()).thenReturn("bus001_notifications");
        return Lists.newArrayList(deviceInfo1, deviceInfo2);
    }

    private List<CarAudioDeviceInfo> getValidCarAudioDeviceInfos() {
        CarAudioDeviceInfo deviceInfo1 = Mockito.mock(CarAudioDeviceInfo.class);
        when(deviceInfo1.getAddress()).thenReturn("bus001_media");
        when(deviceInfo1.getStepValue()).thenReturn(10);
        CarAudioDeviceInfo deviceInfo2 = Mockito.mock(CarAudioDeviceInfo.class);
        when(deviceInfo2.getAddress()).thenReturn("bus002_notifications");
        when(deviceInfo2.getStepValue()).thenReturn(10);
        return Lists.newArrayList(deviceInfo1, deviceInfo2);
    }
}
