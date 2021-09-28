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

import static com.android.car.audio.CarAudioService.DEFAULT_AUDIO_CONTEXT;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.testng.Assert.expectThrows;

import android.car.media.CarAudioManager;
import android.content.Context;
import android.media.AudioDeviceAttributes;
import android.media.AudioDeviceInfo;
import android.util.SparseArray;
import android.util.SparseIntArray;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.R;

import com.google.common.collect.ImmutableList;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

@RunWith(AndroidJUnit4.class)
public class CarAudioZonesHelperTest {
    private List<CarAudioDeviceInfo> mCarAudioOutputDeviceInfos;
    private AudioDeviceInfo[] mInputAudioDeviceInfos;
    private InputStream mInputStream;
    private Context mContext;
    private CarAudioSettings mCarAudioSettings;

    private static final String PRIMARY_ZONE_NAME = "primary zone";

    private static final String BUS_0_ADDRESS = "bus0_media_out";
    private static final String BUS_1_ADDRESS = "bus1_navigation_out";
    private static final String BUS_3_ADDRESS = "bus3_call_ring_out";
    private static final String BUS_100_ADDRESS = "bus100_rear_seat";
    private static final String BUS_1000_ADDRESS_DOES_NOT_EXIST = "bus1000_does_not_exist";

    private static final String PRIMARY_ZONE_MICROPHONE_ADDRESS = "Built-In Mic";
    private static final String PRIMARY_ZONE_FM_TUNER_ADDRESS = "fm_tuner";
    private static final String SECONDARY_ZONE_BACK_MICROPHONE_ADDRESS = "Built-In Back Mic";
    private static final String SECONDARY_ZONE_BUS_1000_INPUT_ADDRESS = "bus_1000_input";

    private static final int PRIMARY_OCCUPANT_ID = 1;
    private static final int SECONDARY_ZONE_ID = 2;

    @Before
    public void setUp() {
        mCarAudioOutputDeviceInfos = generateCarDeviceInfos();
        mInputAudioDeviceInfos = generateInputDeviceInfos();
        mContext = ApplicationProvider.getApplicationContext();
        mInputStream = mContext.getResources().openRawResource(R.raw.car_audio_configuration);
        mCarAudioSettings = mock(CarAudioSettings.class);
    }

    @After
    public void tearDown() throws IOException {
        if (mInputStream != null) {
            mInputStream.close();
        }
    }

    private List<CarAudioDeviceInfo> generateCarDeviceInfos() {
        return ImmutableList.of(
                generateCarAudioDeviceInfo(BUS_0_ADDRESS),
                generateCarAudioDeviceInfo(BUS_1_ADDRESS),
                generateCarAudioDeviceInfo(BUS_3_ADDRESS),
                generateCarAudioDeviceInfo(BUS_100_ADDRESS),
                generateCarAudioDeviceInfo(""),
                generateCarAudioDeviceInfo(""),
                generateCarAudioDeviceInfo(null),
                generateCarAudioDeviceInfo(null)
        );
    }

    private AudioDeviceInfo[] generateInputDeviceInfos() {
        return new AudioDeviceInfo[]{
                generateInputAudioDeviceInfo(PRIMARY_ZONE_MICROPHONE_ADDRESS,
                        AudioDeviceInfo.TYPE_BUILTIN_MIC),
                generateInputAudioDeviceInfo(PRIMARY_ZONE_FM_TUNER_ADDRESS,
                        AudioDeviceInfo.TYPE_FM_TUNER),
                generateInputAudioDeviceInfo(SECONDARY_ZONE_BACK_MICROPHONE_ADDRESS,
                        AudioDeviceInfo.TYPE_BUS),
                generateInputAudioDeviceInfo(SECONDARY_ZONE_BUS_1000_INPUT_ADDRESS,
                        AudioDeviceInfo.TYPE_BUILTIN_MIC)
        };
    }

    private CarAudioDeviceInfo generateCarAudioDeviceInfo(String address) {
        CarAudioDeviceInfo cadiMock = mock(CarAudioDeviceInfo.class);
        when(cadiMock.getStepValue()).thenReturn(1);
        when(cadiMock.getDefaultGain()).thenReturn(2);
        when(cadiMock.getMaxGain()).thenReturn(5);
        when(cadiMock.getMinGain()).thenReturn(0);
        when(cadiMock.getAddress()).thenReturn(address);
        return cadiMock;
    }

    private AudioDeviceInfo generateInputAudioDeviceInfo(String address, int type) {
        AudioDeviceInfo inputMock = mock(AudioDeviceInfo.class);
        when(inputMock.getAddress()).thenReturn(address);
        when(inputMock.getType()).thenReturn(type);
        when(inputMock.isSource()).thenReturn(true);
        when(inputMock.isSink()).thenReturn(false);
        return inputMock;
    }

    @Test
    public void loadAudioZones_parsesAllZones() throws Exception {
        CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings, mInputStream,
                mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

        SparseArray<CarAudioZone> zones = cazh.loadAudioZones();

        assertThat(zones.size()).isEqualTo(2);
    }

    @Test
    public void loadAudioZones_versionOneParsesAllZones() throws Exception {
        try (InputStream versionOneStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_V1)) {
            CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings, versionOneStream,
                    mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

            SparseArray<CarAudioZone> zones = cazh.loadAudioZones();

            assertThat(zones.size()).isEqualTo(2);
        }
    }

    @Test
    public void loadAudioZones_parsesAudioZoneId() throws Exception {
        CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings, mInputStream,
                mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

        SparseArray<CarAudioZone> zones = cazh.loadAudioZones();


        List<Integer> zoneIds = getListOfZoneIds(zones);
        assertThat(zones.size()).isEqualTo(2);
        assertThat(zones.contains(CarAudioManager.PRIMARY_AUDIO_ZONE)).isTrue();
        assertThat(zones.contains(SECONDARY_ZONE_ID)).isTrue();
    }

    @Test
    public void loadAudioZones_parsesOccupantZoneId() throws Exception {
        CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings, mInputStream,
                mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

        SparseArray<CarAudioZone> zones = cazh.loadAudioZones();

        assertThat(zones.size()).isEqualTo(2);

        SparseIntArray audioZoneIdToOccupantZoneIdMapping =
                cazh.getCarAudioZoneIdToOccupantZoneIdMapping();
        assertThat(audioZoneIdToOccupantZoneIdMapping.get(CarAudioManager.PRIMARY_AUDIO_ZONE))
                .isEqualTo(PRIMARY_OCCUPANT_ID);
        assertThat(audioZoneIdToOccupantZoneIdMapping.get(SECONDARY_ZONE_ID, -1))
                .isEqualTo(-1);
    }

    @Test
    public void loadAudioZones_parsesZoneName() throws Exception {
        CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings, mInputStream,
                mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

        SparseArray<CarAudioZone> zones = cazh.loadAudioZones();

        CarAudioZone primaryZone = zones.get(0);
        assertThat(primaryZone.getName()).isEqualTo(PRIMARY_ZONE_NAME);
    }

    @Test
    public void loadAudioZones_parsesIsPrimary() throws Exception {
        CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings, mInputStream,
                mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

        SparseArray<CarAudioZone> zones = cazh.loadAudioZones();

        CarAudioZone primaryZone = zones.get(0);
        assertThat(primaryZone.isPrimaryZone()).isTrue();

        CarAudioZone rseZone = zones.get(2);
        assertThat(rseZone.isPrimaryZone()).isFalse();
    }

    @Test
    public void loadAudioZones_parsesVolumeGroups() throws Exception {
        CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings, mInputStream,
                mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

        SparseArray<CarAudioZone> zones = cazh.loadAudioZones();

        CarAudioZone primaryZone = zones.get(0);
        assertThat(primaryZone.getVolumeGroupCount()).isEqualTo(2);
    }

    @Test
    public void loadAudioZones_parsesAddresses() throws Exception {
        CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings, mInputStream,
                mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

        SparseArray<CarAudioZone> zones = cazh.loadAudioZones();

        CarAudioZone primaryZone = zones.get(0);
        CarVolumeGroup volumeGroup = primaryZone.getVolumeGroups()[0];
        List<String> addresses = volumeGroup.getAddresses();
        assertThat(addresses).containsExactly(BUS_0_ADDRESS, BUS_3_ADDRESS);
    }

    @Test
    public void loadAudioZones_parsesContexts() throws Exception {
        CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings, mInputStream,
                mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

        SparseArray<CarAudioZone> zones = cazh.loadAudioZones();

        CarAudioZone primaryZone = zones.get(0);
        CarVolumeGroup volumeGroup = primaryZone.getVolumeGroups()[0];
        assertThat(volumeGroup.getContextsForAddress(BUS_0_ADDRESS)).asList()
                .containsExactly(CarAudioContext.MUSIC);

        CarAudioZone rearSeatEntertainmentZone = zones.get(2);
        CarVolumeGroup rseVolumeGroup = rearSeatEntertainmentZone.getVolumeGroups()[0];
        List<Integer> contextForBus100List =
                Arrays.stream(rseVolumeGroup.getContextsForAddress(BUS_100_ADDRESS))
                        .boxed().collect(Collectors.toList());
        List<Integer> contextsList =
                Arrays.stream(CarAudioContext.CONTEXTS).boxed().collect(Collectors.toList());
        assertThat(contextForBus100List).containsExactlyElementsIn(contextsList);
    }

    @Test
    public void loadAudioZones_forVersionOne_bindsNonLegacyContextsToDefault() throws Exception {
        InputStream versionOneStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_V1);

        CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings, versionOneStream,
                mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

        SparseArray<CarAudioZone> zones = cazh.loadAudioZones();

        CarAudioZone defaultZone = zones.get(0);
        CarVolumeGroup volumeGroup = defaultZone.getVolumeGroups()[0];
        List<Integer> audioContexts = Arrays.stream(volumeGroup.getContexts()).boxed()
                .collect(Collectors.toList());

        assertThat(audioContexts).containsAllOf(DEFAULT_AUDIO_CONTEXT, CarAudioContext.EMERGENCY,
                CarAudioContext.SAFETY, CarAudioContext.VEHICLE_STATUS,
                CarAudioContext.ANNOUNCEMENT);
    }

    @Test
    public void loadAudioZones_forVersionOneWithNonLegacyContexts_throws() {
        InputStream v1NonLegacyContextStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_V1_with_non_legacy_contexts);

        CarAudioZonesHelper cazh =
                new CarAudioZonesHelper(mCarAudioSettings, v1NonLegacyContextStream,
                        mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

        IllegalArgumentException exception = expectThrows(IllegalArgumentException.class,
                cazh::loadAudioZones);

        assertThat(exception).hasMessageThat().contains("Non-legacy audio contexts such as");
    }

    @Test
    public void loadAudioZones_passesOnMissingAudioZoneIdForPrimary() throws Exception {
        try (InputStream missingAudioZoneIdStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_no_audio_zone_id_for_primary_zone)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, missingAudioZoneIdStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

            SparseArray<CarAudioZone> zones = cazh.loadAudioZones();

            assertThat(zones.size()).isEqualTo(2);
            assertThat(zones.contains(CarAudioManager.PRIMARY_AUDIO_ZONE)).isTrue();
            assertThat(zones.contains(SECONDARY_ZONE_ID)).isTrue();
        }
    }

    @Test
    public void loadAudioZones_versionOneFailsOnAudioZoneId() throws Exception {
        try (InputStream versionOneAudioZoneIdStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_V1_with_audio_zone_id)) {
            CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings,
                    versionOneAudioZoneIdStream, mCarAudioOutputDeviceInfos,
                    mInputAudioDeviceInfos);
            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("Invalid audio attribute audioZoneId");
        }
    }

    @Test
    public void loadAudioZones_versionOneFailsOnOccupantZoneId() throws Exception {
        try (InputStream versionOneOccupantIdStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_V1_with_occupant_zone_id)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, versionOneOccupantIdStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);
            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("Invalid audio attribute occupantZoneId");
        }
    }

    @Test
    public void loadAudioZones_parsesInputDevices() throws Exception {
        try (InputStream inputDevicesStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_with_input_devices)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, inputDevicesStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

            SparseArray<CarAudioZone> zones = cazh.loadAudioZones();

            CarAudioZone primaryZone = zones.get(0);
            List<AudioDeviceAttributes> primaryZoneInputDevices =
                    primaryZone.getInputAudioDevices();
            assertThat(primaryZoneInputDevices).hasSize(2);

            List<String> primaryZoneInputAddresses =
                    primaryZoneInputDevices.stream().map(a -> a.getAddress()).collect(
                            Collectors.toList());
            assertThat(primaryZoneInputAddresses).containsExactly(PRIMARY_ZONE_FM_TUNER_ADDRESS,
                    PRIMARY_ZONE_MICROPHONE_ADDRESS).inOrder();

            CarAudioZone secondaryZone = zones.get(1);
            List<AudioDeviceAttributes> secondaryZoneInputDevices =
                    secondaryZone.getInputAudioDevices();
            List<String> secondaryZoneInputAddresses =
                    secondaryZoneInputDevices.stream().map(a -> a.getAddress()).collect(
                            Collectors.toList());
            assertThat(secondaryZoneInputAddresses).containsExactly(
                    SECONDARY_ZONE_BUS_1000_INPUT_ADDRESS,
                    SECONDARY_ZONE_BACK_MICROPHONE_ADDRESS).inOrder();
        }
    }

    @Test
    public void loadAudioZones_failsOnDuplicateOccupantZoneId() throws Exception {
        try (InputStream duplicateOccupantZoneIdStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_duplicate_occupant_zone_id)) {
            CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings,
                    duplicateOccupantZoneIdStream, mCarAudioOutputDeviceInfos,
                    mInputAudioDeviceInfos);
            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("already associated with a zone");
        }
    }

    @Test
    public void loadAudioZones_failsOnDuplicateAudioZoneId() throws Exception {
        try (InputStream duplicateAudioZoneIdStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_duplicate_audio_zone_id)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, duplicateAudioZoneIdStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);
            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("already associated with a zone");
        }
    }

    @Test
    public void loadAudioZones_failsOnEmptyInputDeviceAddress() throws Exception {
        try (InputStream inputDevicesStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_empty_input_device)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, inputDevicesStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("empty.");
        }
    }

    @Test
    public void loadAudioZones_failsOnNonNumericalAudioZoneId() throws Exception {
        try (InputStream nonNumericalStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_non_numerical_audio_zone_id)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, nonNumericalStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);
            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("was \"primary\" instead.");
        }
    }

    @Test
    public void loadAudioZones_failsOnNegativeAudioZoneId() throws Exception {
        try (InputStream negativeAudioZoneIdStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_negative_audio_zone_id)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, negativeAudioZoneIdStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);
            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("but was \"-1\" instead.");
        }
    }

    @Test
    public void loadAudioZones_failsOnMissingInputDevice() throws Exception {
        try (InputStream inputDevicesStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_missing_address)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, inputDevicesStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

            NullPointerException thrown =
                    expectThrows(NullPointerException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("present.");
        }
    }

    @Test
    public void loadAudioZones_failsOnNonNumericalOccupantZoneId() throws Exception {
        try (InputStream nonNumericalStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_non_numerical_occupant_zone_id)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, nonNumericalStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);
            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("was \"one\" instead.");
        }
    }

    @Test
    public void loadAudioZones_failsOnNegativeOccupantZoneId() throws Exception {
        try (InputStream negativeOccupantZoneIdStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_negative_occupant_zone_id)) {
            CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings,
                    negativeOccupantZoneIdStream, mCarAudioOutputDeviceInfos,
                    mInputAudioDeviceInfos);
            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("was \"-1\" instead.");
        }
    }

    @Test
    public void loadAudioZones_failsOnNonExistentInputDevice() throws Exception {
        try (InputStream inputDevicesStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_non_existent_input_device)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, inputDevicesStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("does not exist");
        }
    }

    @Test
    public void loadAudioZones_failsOnEmptyOccupantZoneId() throws Exception {
        try (InputStream emptyOccupantZoneIdStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_empty_occupant_zone_id)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, emptyOccupantZoneIdStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);
            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("but was \"\" instead.");
        }
    }

    @Test
    public void loadAudioZones_failsOnNonZeroAudioZoneIdForPrimary() throws Exception {
        try (InputStream nonZeroForPrimaryStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_primary_zone_with_non_zero_audio_zone_id)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, nonZeroForPrimaryStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);
            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("it can be left empty.");
        }
    }

    @Test
    public void loadAudioZones_failsOnZeroAudioZoneIdForSecondary() throws Exception {
        try (InputStream zeroZoneIdForSecondaryStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_non_primary_zone_with_primary_audio_zone_id)) {
            CarAudioZonesHelper cazh = new CarAudioZonesHelper(mCarAudioSettings,
                    zeroZoneIdForSecondaryStream, mCarAudioOutputDeviceInfos,
                    mInputAudioDeviceInfos);
            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains(PRIMARY_ZONE_NAME);
        }
    }

    @Test
    public void loadAudioZones_failsOnRepeatedInputDevice() throws Exception {
        try (InputStream inputDevicesStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_repeat_input_device)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, inputDevicesStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

            IllegalArgumentException thrown =
                    expectThrows(IllegalArgumentException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains("can not repeat.");
        }
    }

    @Test
    public void loadAudioZones_failsOnMissingOutputDevice() throws Exception {
        try (InputStream outputDevicesStream = mContext.getResources().openRawResource(
                R.raw.car_audio_configuration_output_address_does_not_exist)) {
            CarAudioZonesHelper cazh =
                    new CarAudioZonesHelper(mCarAudioSettings, outputDevicesStream,
                            mCarAudioOutputDeviceInfos, mInputAudioDeviceInfos);

            IllegalStateException thrown =
                    expectThrows(IllegalStateException.class,
                            () -> cazh.loadAudioZones());
            assertThat(thrown).hasMessageThat().contains(BUS_1000_ADDRESS_DOES_NOT_EXIST);
        }
    }

    private List<Integer> getListOfZoneIds(SparseArray<CarAudioZone> zones) {
        List<Integer> zoneIds = new ArrayList<>();
        for (int i = 0; i < zones.size(); i++) {
            zoneIds.add(zones.keyAt(i));
        }
        return zoneIds;
    }
}
