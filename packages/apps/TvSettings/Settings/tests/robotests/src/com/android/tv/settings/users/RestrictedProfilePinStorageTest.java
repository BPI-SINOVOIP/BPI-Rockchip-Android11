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

package com.android.tv.settings.users;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.RemoteException;

import com.android.internal.widget.ILockSettings;
import com.android.internal.widget.LockPatternUtils;
import com.android.internal.widget.LockPatternUtils.RequestThrottledException;
import com.android.internal.widget.LockscreenCredential;
import com.android.internal.widget.VerifyCredentialResponse;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;

/**
 * Tests for {@link RestrictedProfilePinStorage}.
 */
@RunWith(RobolectricTestRunner.class)
public class RestrictedProfilePinStorageTest {

    private static final int USER_ID = 0;

    @Mock
    private ILockSettings mLockSettings;
    @Mock
    private LockPatternUtils mLockPatternUtils;
    @Mock
    private RestrictedProfilePinServiceConnection mConnection;
    @Mock
    private IRestrictedProfilePinService mPinService;

    private RestrictedProfilePinStorage mPinStorage;

    private String mOriginalPin;
    private String mNewPin;
    private String mWrongPin;

    @Before
    public void setUp() throws RemoteException {
        MockitoAnnotations.initMocks(this);
        when(mLockPatternUtils.getLockSettings()).thenReturn(mLockSettings);

        when(mConnection.getPinService()).thenReturn(mPinService);
        mPinStorage = new RestrictedProfilePinStorage(mLockPatternUtils, USER_ID, mConnection);
        mPinStorage.bind();

        mOriginalPin = "0000";
        mNewPin = "1234";
        mWrongPin = "1111";
    }

    @Test
    public void testSetPinWhenNoPinIsSet_success()
            throws RemoteException, RequestThrottledException {
        mOriginalPin = null;
        initPin(mOriginalPin);

        mPinStorage.setPin(mNewPin, mOriginalPin);

        verify(mPinService).isPinSet();
        verify(mPinService, never()).isPinCorrect(anyString());

        verify(mPinService).setPin(mNewPin);
        verify(mLockPatternUtils, never()).setLockCredential(
                LockscreenCredential.createPinOrNone(anyString()),
                LockscreenCredential.createPinOrNone(anyString()), eq(USER_ID));
    }

    @Test
    public void testSetPinWhenWrongPinIsInput_fail()
            throws RemoteException, RequestThrottledException {
        initPin(mOriginalPin);

        mPinStorage.setPin(mNewPin, mWrongPin);

        verify(mPinService).isPinCorrect(eq(mWrongPin));

        verify(mPinService, never()).setPin(anyString());
        verify(mLockPatternUtils, never()).setLockCredential(
                LockscreenCredential.createPinOrNone(anyString()),
                LockscreenCredential.createPinOrNone(anyString()), eq(USER_ID));
    }

    @Test
    public void testSetPinWhenCorrectPinIsInput_success()
            throws RemoteException, RequestThrottledException {
        initPin(mOriginalPin);

        mPinStorage.setPin(mNewPin, mOriginalPin);

        verify(mPinService, atLeastOnce()).isPinSet();
        verify(mPinService).isPinCorrect(eq(mOriginalPin));

        verify(mPinService).setPin(eq(mNewPin));
        verify(mLockPatternUtils, never()).setLockCredential(
                LockscreenCredential.createPinOrNone(anyString()),
                eq(LockscreenCredential.createPinOrNone(mOriginalPin)), eq(USER_ID));
    }

    @Test
    public void testDeletePin_success() throws RemoteException, RequestThrottledException {
        initPin(mOriginalPin);

        mPinStorage.deletePin(mOriginalPin);

        verify(mPinService).isPinCorrect(mOriginalPin);
        verify(mPinService).deletePin();
        verify(mLockPatternUtils, never()).setLockCredential(
                LockscreenCredential.createPinOrNone(anyString()),
                eq(LockscreenCredential.createPinOrNone(mOriginalPin)), eq(USER_ID));
    }

    @Test
    public void testDeletePin_fail() throws RemoteException, RequestThrottledException {
        initPin(mOriginalPin);

        mPinStorage.deletePin(mWrongPin);

        verify(mPinService).isPinCorrect(mWrongPin);
        verify(mPinService, never()).deletePin();
        verify(mLockPatternUtils, never()).setLockCredential(
                LockscreenCredential.createPinOrNone(anyString()),
                LockscreenCredential.createPinOrNone(anyString()), eq(USER_ID));
    }

    @Test
    public void testIsPinCorrect() throws RemoteException, RequestThrottledException {
        initPin(mOriginalPin);

        boolean isPinCorrect = mPinStorage.isPinCorrect(mOriginalPin);

        verify(mPinService).isPinSet();
        verify(mLockPatternUtils, never()).isSecure(eq(USER_ID));
        verify(mPinService).isPinCorrect(eq(mOriginalPin));
        assertThat(isPinCorrect).isTrue();
    }

    @Test
    public void testIsPinCorrect_legacy() throws RemoteException, RequestThrottledException {
        initPinLegacy(mOriginalPin);

        mPinStorage.isPinCorrect(mOriginalPin);

        verify(mPinService).isPinSet();
        verify(mLockPatternUtils).isSecure(eq(USER_ID));
        verify(mPinService, never()).isPinCorrect(anyString());
        verify(mLockPatternUtils).checkCredential(any(), anyInt(), any());
    }

    @Test
    public void testIsPinSet() throws RemoteException, RequestThrottledException {
        initPinInternal(mOriginalPin);

        mPinStorage.isPinSet();

        verify(mPinService).isPinSet();
        verify(mLockPatternUtils, never()).isSecure(eq(USER_ID));
    }

    @Test
    public void testIsPinSet_legacy() throws RemoteException, RequestThrottledException {
        initPinLegacy(mOriginalPin);

        mPinStorage.isPinSet();

        verify(mPinService).isPinSet();
        verify(mLockPatternUtils).isSecure(eq(USER_ID));
    }

    @Test
    public void testMigrationToInternalStorage_IsPinCorrect()
            throws RemoteException, RequestThrottledException {
        initPinLegacy(mOriginalPin);

        mPinStorage.isPinCorrect(mOriginalPin);

        verify(mPinService).isPinSet();
        verify(mLockPatternUtils).isSecure(eq(USER_ID));
        verify(mPinService, never()).isPinCorrect(anyString());
        verify(mLockPatternUtils).checkCredential(any(), anyInt(), any());
    }

    @Test
    public void testMigrationToInternalStorage_SetPin()
            throws RemoteException, RequestThrottledException {
        initPinLegacy(mOriginalPin);

        mPinStorage.setPin(mNewPin, mOriginalPin);

        verify(mPinService, atLeastOnce()).isPinSet();
        verify(mLockPatternUtils, atLeastOnce()).isSecure(eq(USER_ID));
        verify(mPinService, never()).isPinCorrect(anyString());
        verify(mLockPatternUtils, never()).setLockCredential(
                LockscreenCredential.createPinOrNone(anyString()),
                eq(LockscreenCredential.createPinOrNone(mOriginalPin)), eq(USER_ID));
        verify(mPinService).setPin(eq(mNewPin));
    }

    private void initPin(String pin) throws RemoteException, RequestThrottledException {
        initPinInternal(pin);
        initPinLegacy(pin);
    }

    private void initPinInternal(String pin) throws RemoteException, RequestThrottledException {
        when(mPinService.isPinSet()).thenReturn(pin != null);
        if (pin != null) {
            when(mPinService.isPinCorrect(any())).thenReturn(false);
            when(mPinService.isPinCorrect(eq(pin))).thenReturn(true);
        }
    }

    private void initPinLegacy(String pin) throws RemoteException, RequestThrottledException {
        LockscreenCredential credential = LockscreenCredential.createPinOrNone(pin);
        when(mLockPatternUtils.isSecure(anyInt())).thenReturn(pin != null);
        when(mLockSettings.checkCredential(any(), anyInt(), any()))
                .thenReturn(VerifyCredentialResponse.ERROR);
        when(mLockPatternUtils.checkCredential(any(), anyInt(), any()))
                .thenReturn(false);
        if (pin != null) {
            when(mLockSettings.checkCredential(eq(credential), eq(USER_ID), any()))
                    .thenReturn(VerifyCredentialResponse.OK);
            when(mLockPatternUtils.checkCredential(any(), eq(USER_ID), any()))
                    .thenReturn(true);
        }
    }

    private byte[] getBytes(String s) {
        return s == null ? null : s.getBytes();
    }
}
