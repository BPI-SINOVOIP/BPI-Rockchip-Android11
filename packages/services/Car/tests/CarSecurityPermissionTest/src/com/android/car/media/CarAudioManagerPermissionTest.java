/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.media;

import static android.car.Car.AUDIO_SERVICE;
import static android.car.Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS;
import static android.car.Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME;
import static android.car.Car.createCar;
import static android.car.media.CarAudioManager.PRIMARY_AUDIO_ZONE;
import static android.media.AudioAttributes.USAGE_MEDIA;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.expectThrows;

import android.car.Car;
import android.car.media.CarAudioManager;
import android.content.Context;
import android.os.Handler;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Objects;

/**
 * This class contains security permission tests for the {@link CarAudioManager}'s system APIs.
 */
@RunWith(AndroidJUnit4.class)
public final class CarAudioManagerPermissionTest {
    private static final int GROUP_ID = 0;
    private static final int UID = 10;

    private CarAudioManager mCarAudioManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Car car = Objects.requireNonNull(createCar(mContext, (Handler) null));
        mCarAudioManager = (CarAudioManager) car.getCarManager(AUDIO_SERVICE);
    }

    @Test
    public void setGroupVolumePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.setGroupVolume(GROUP_ID, 0, 0));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void setGroupVolumeWithZonePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.setGroupVolume(PRIMARY_AUDIO_ZONE, GROUP_ID, 0, 0));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getGroupMaxVolumePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getGroupMaxVolume(GROUP_ID));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getGroupMaxVolumeWithZonePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getGroupMaxVolume(PRIMARY_AUDIO_ZONE, GROUP_ID));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getGroupMinVolumePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getGroupMinVolume(GROUP_ID));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getGroupMinVolumeWithZonePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getGroupMinVolume(PRIMARY_AUDIO_ZONE, GROUP_ID));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getGroupVolumePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getGroupVolume(GROUP_ID));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getGroupVolumeWithZonePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getGroupVolume(PRIMARY_AUDIO_ZONE, GROUP_ID));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void setFadeTowardFrontPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.setFadeTowardFront(0));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void setBalanceTowardRightPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.setBalanceTowardRight(0));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getExternalSourcesPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getExternalSources());
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_SETTINGS);
    }

    @Test
    public void createAudioPatchPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.createAudioPatch("address", USAGE_MEDIA, 0));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_SETTINGS);
    }

    @Test
    public void releaseAudioPatchPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.releaseAudioPatch(null));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_SETTINGS);
    }

    @Test
    public void getVolumeGroupCountPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getVolumeGroupCount());
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getVolumeGroupCountWithZonePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getVolumeGroupCount(PRIMARY_AUDIO_ZONE));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getVolumeGroupIdForUsagePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getVolumeGroupIdForUsage(USAGE_MEDIA));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getVolumeGroupIdForUsageWithZonePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getVolumeGroupIdForUsage(PRIMARY_AUDIO_ZONE, USAGE_MEDIA));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getUsagesForVolumeGroupIdPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getUsagesForVolumeGroupId(GROUP_ID));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void getAudioZoneIdsPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getAudioZoneIds());
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_SETTINGS);
    }

    @Test
    public void getZoneIdForUidPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getZoneIdForUid(UID));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_SETTINGS);
    }

    @Test
    public void setZoneIdForUidPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.setZoneIdForUid(PRIMARY_AUDIO_ZONE, UID));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_SETTINGS);
    }

    @Test
    public void clearZoneIdForUidPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.clearZoneIdForUid(UID));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_SETTINGS);
    }

    @Test
    public void getOutputDeviceForUsagePermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.getOutputDeviceForUsage(PRIMARY_AUDIO_ZONE, USAGE_MEDIA));
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_SETTINGS);
    }

    @Test
    public void onCarDisconnectedPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.onCarDisconnected());
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }

    @Test
    public void onCarDisconnected() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarAudioManager.onCarDisconnected());
        assertThat(e.getMessage()).contains(PERMISSION_CAR_CONTROL_AUDIO_VOLUME);
    }
}
