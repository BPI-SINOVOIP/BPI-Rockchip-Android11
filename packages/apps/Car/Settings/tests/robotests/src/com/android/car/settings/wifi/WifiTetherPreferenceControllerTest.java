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

package com.android.car.settings.wifi;

import static com.android.car.settings.common.PreferenceController.AVAILABLE;
import static com.android.car.settings.common.PreferenceController.AVAILABLE_FOR_VIEWING;
import static com.android.car.settings.common.PreferenceController.UNSUPPORTED_ON_DEVICE;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.net.TetheringManager;
import android.net.wifi.SoftApConfiguration;

import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;
import androidx.test.core.app.ApplicationProvider;

import com.android.car.settings.common.PreferenceControllerTestHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.shadows.ShadowApplication;

import java.util.concurrent.Executor;

@RunWith(RobolectricTestRunner.class)
public class WifiTetherPreferenceControllerTest {

    private Context mContext;
    private Preference mPreference;
    private PreferenceControllerTestHelper<WifiTetherPreferenceController> mControllerHelper;
    @Mock
    private TetheringManager mTetheringManager;
    private SoftApConfiguration mSoftApConfiguration;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mContext = ApplicationProvider.getApplicationContext();
        ShadowApplication.getInstance().setSystemService(
                Context.TETHERING_SERVICE, mTetheringManager);

        mPreference = new Preference(mContext);
        mControllerHelper =
                new PreferenceControllerTestHelper<WifiTetherPreferenceController>(mContext,
                        WifiTetherPreferenceController.class, mPreference);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
    }

    @Test
    public void onStart_isAvailableForViewing() {
        assertThat(mControllerHelper.getController().getAvailabilityStatus()).isEqualTo(
                AVAILABLE_FOR_VIEWING);
    }

    @Test
    public void onStart_registersTetheringEventCallback() {
        verify(mTetheringManager).registerTetheringEventCallback(
                any(Executor.class), any(TetheringManager.TetheringEventCallback.class));
    }

    @Test
    public void onStop_unregistersTetheringEventCallback() {
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_STOP);

        verify(mTetheringManager).unregisterTetheringEventCallback(
                any(TetheringManager.TetheringEventCallback.class));
    }

    @Test
    public void onTetheringSupported_false_isUnsupportedOnDevice() {
        ArgumentCaptor<TetheringManager.TetheringEventCallback> captor =
                ArgumentCaptor.forClass(TetheringManager.TetheringEventCallback.class);
        verify(mTetheringManager).registerTetheringEventCallback(
                any(Executor.class), captor.capture());

        captor.getValue().onTetheringSupported(false);

        assertThat(mControllerHelper.getController().getAvailabilityStatus()).isEqualTo(
                UNSUPPORTED_ON_DEVICE);
    }

    @Test
    public void onTetheringSupported_true_isAvailable() {
        ArgumentCaptor<TetheringManager.TetheringEventCallback> captor =
                ArgumentCaptor.forClass(TetheringManager.TetheringEventCallback.class);
        verify(mTetheringManager).registerTetheringEventCallback(
                any(Executor.class), captor.capture());
        captor.getValue().onTetheringSupported(false);

        captor.getValue().onTetheringSupported(true);

        assertThat(mControllerHelper.getController().getAvailabilityStatus()).isEqualTo(AVAILABLE);
    }
}
