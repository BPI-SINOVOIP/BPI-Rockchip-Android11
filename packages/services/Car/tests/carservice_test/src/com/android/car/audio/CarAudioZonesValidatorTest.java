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

import static org.mockito.Mockito.when;

import android.car.media.CarAudioManager;
import android.util.SparseArray;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import java.util.List;

@RunWith(AndroidJUnit4.class)
public class CarAudioZonesValidatorTest {
    @Rule
    public final ExpectedException thrown = ExpectedException.none();

    @Test
    public void validate_thereIsAtLeastOneZone() {
        thrown.expect(RuntimeException.class);
        thrown.expectMessage("At least one zone should be defined");

        CarAudioZonesValidator.validate(new SparseArray<CarAudioZone>());
    }

    @Test
    public void validate_volumeGroupsForEachZone() {
        SparseArray<CarAudioZone> zones = generateAudioZonesWithPrimary();
        CarAudioZone zoneOne = new MockBuilder()
                .withInvalidVolumeGroups()
                .withZoneId(1)
                .build();
        zones.put(zoneOne.getId(), zoneOne);

        thrown.expect(RuntimeException.class);
        thrown.expectMessage("Invalid volume groups configuration for zone " + 1);

        CarAudioZonesValidator.validate(zones);
    }

    @Test
    public void validate_eachAddressAppearsInOnlyOneZone() {
        CarVolumeGroup mockVolumeGroup = generateVolumeGroup(List.of("one", "two", "three"));

        CarAudioZone primaryZone = new MockBuilder()
                .withVolumeGroups(new CarVolumeGroup[]{mockVolumeGroup})
                .build();

        CarVolumeGroup mockSecondaryVolumeGroup = generateVolumeGroup(
                List.of("three", "four", "five"));

        CarAudioZone secondaryZone = new MockBuilder()
                .withZoneId(1)
                .withVolumeGroups(new CarVolumeGroup[]{mockSecondaryVolumeGroup})
                .build();
        SparseArray<CarAudioZone> zones = new SparseArray<>();
        zones.put(primaryZone.getId(), primaryZone);
        zones.put(secondaryZone.getId(), secondaryZone);


        thrown.expect(RuntimeException.class);
        thrown.expectMessage(
                "Device with address three appears in multiple volume groups or audio zones");

        CarAudioZonesValidator.validate(zones);
    }

    @Test
    public void validate_passesWithoutExceptionForValidZoneConfiguration() {
        SparseArray<CarAudioZone> zones = generateAudioZonesWithPrimary();

        CarAudioZonesValidator.validate(zones);
    }

    private SparseArray<CarAudioZone> generateAudioZonesWithPrimary() {
        CarAudioZone zone = new MockBuilder().build();
        SparseArray<CarAudioZone> zones = new SparseArray<>();
        zones.put(zone.getId(), zone);
        return zones;
    }

    private CarVolumeGroup generateVolumeGroup(List<String> deviceAddresses) {
        CarVolumeGroup mockVolumeGroup = Mockito.mock(CarVolumeGroup.class);
        when(mockVolumeGroup.getAddresses()).thenReturn(deviceAddresses);
        return mockVolumeGroup;
    }

    private CarAudioZone getMockPrimaryZone() {
        CarAudioZone zoneMock = Mockito.mock(CarAudioZone.class);
        when(zoneMock.getId()).thenReturn(CarAudioManager.PRIMARY_AUDIO_ZONE);
        return zoneMock;
    }
    private static class MockBuilder {
        private boolean mHasValidVolumeGroups = true;
        private int mZoneId = 0;
        private CarVolumeGroup[] mVolumeGroups = new CarVolumeGroup[0];

        CarAudioZone build() {
            CarAudioZone zoneMock = Mockito.mock(CarAudioZone.class);
            when(zoneMock.getId()).thenReturn(mZoneId);
            when(zoneMock.validateVolumeGroups()).thenReturn(mHasValidVolumeGroups);
            when(zoneMock.getVolumeGroups()).thenReturn(mVolumeGroups);
            return zoneMock;
        }

        MockBuilder withInvalidVolumeGroups() {
            mHasValidVolumeGroups = false;
            return this;
        }

        MockBuilder withZoneId(int zoneId) {
            mZoneId = zoneId;
            return this;
        }

        MockBuilder withVolumeGroups(CarVolumeGroup[] volumeGroups) {
            mVolumeGroups = volumeGroups;
            return this;
        }
    }
}