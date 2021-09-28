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

package com.android.tv.settings.system;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.robolectric.Robolectric.buildActivity;

import android.os.RemoteException;

import com.android.internal.widget.LockscreenCredential;
import com.android.server.locksettings.LockSettingsService;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.android.controller.ActivityController;

@RunWith(RobolectricTestRunner.class)
public class FallbackHomeTest {
    private FallbackHome mFallbackHome;
    @Mock
    private LockSettingsService mLockSettings;


    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        ActivityController<FallbackHome> fallbackHomeController = buildActivity(FallbackHome.class);
        mFallbackHome = spy(fallbackHomeController.get());
    }

    @Test
    public void startPinDialogWhenNoPinIsSaved() {
        mockDeviceLocked();
        mFallbackHome.maybeStartPinDialog();

        verify(mFallbackHome).showPinDialogToUnlockDevice();
        verify(mFallbackHome, never()).unlockDeviceWithPin(
                LockscreenCredential.createPinOrNone(anyString()));
    }

    @Test
    public void unlockDeviceAutomaticallyWhenPinIsSaved() throws RemoteException {
        mockDeviceLocked();

        LockscreenCredential pin = LockscreenCredential.createPinOrNone("0000");
        doReturn(pin).when(mFallbackHome).getPinFromSharedPreferences();
        doReturn(mLockSettings).when(mFallbackHome).getLockSettings();

        mFallbackHome.maybeStartPinDialog();

        verify(mFallbackHome, never()).showPinDialogToUnlockDevice();
        verify(mFallbackHome).unlockDeviceWithPin(pin);
        verify(mLockSettings).checkCredential(eq(pin), anyInt(), any());
    }

    private void mockDeviceLocked() {
        doReturn(false).when(mFallbackHome).isUserUnlocked();
        doReturn(true).when(mFallbackHome).hasLockscreenSecurity(anyInt());
        doReturn(true).when(mFallbackHome).isFileEncryptionEnabled();
    }

}
