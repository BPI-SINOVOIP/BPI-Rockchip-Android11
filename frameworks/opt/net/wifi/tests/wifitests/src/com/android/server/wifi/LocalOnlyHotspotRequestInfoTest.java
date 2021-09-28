/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.server.wifi;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.*;

import android.net.wifi.ILocalOnlyHotspotCallback;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.WifiManager;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/*
 * Unit tests for {@link com.android.server.wifi.LocalOnlyHotspotRequestInfo}.
 */
@SmallTest
public class LocalOnlyHotspotRequestInfoTest extends WifiBaseTest {

    private static final String TAG = "LocalOnlyHotspotRequestInfoTest";
    @Mock IBinder mAppBinder;
    @Mock ILocalOnlyHotspotCallback mCallback;
    @Mock LocalOnlyHotspotRequestInfo.RequestingApplicationDeathCallback mDeathCallback;
    RemoteException mRemoteException;
    private LocalOnlyHotspotRequestInfo mLOHSRequestInfo;

    /**
     * Setup test.
     */
    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mCallback.asBinder()).thenReturn(mAppBinder);
        mRemoteException = new RemoteException("Test Remote Exception");
    }

    /**
     * Make sure we link the call to request LocalOnlyHotspot by an app is linked to their Binder
     * call.  This allows us to clean up if the app dies.
     */
    @Test
    public void verifyBinderLinkToDeathIsCalled() throws Exception {
        mLOHSRequestInfo = new LocalOnlyHotspotRequestInfo(mCallback, mDeathCallback, null);
        verify(mAppBinder).linkToDeath(eq(mLOHSRequestInfo), eq(0));
    }

    /**
     * Calls to link the requestor to binder death should not pass null binder
     */
    @Test(expected = NullPointerException.class)
    public void verifyNullCallbackChecked() throws Exception {
        mLOHSRequestInfo = new LocalOnlyHotspotRequestInfo(null, mDeathCallback, null);
    }

    /**
     * Calls to create the requestor to binder death should not pass null death callback.
     */
    @Test(expected = NullPointerException.class)
    public void verifyNullDeathCallbackChecked() throws Exception {
        mLOHSRequestInfo = new LocalOnlyHotspotRequestInfo(mCallback, null, null);
    }

    /**
     * Calls to unlink the DeathRecipient should call to unlink from Binder.
     */
    @Test
    public void verifyUnlinkDeathRecipientUnlinksFromBinder() throws Exception {
        mLOHSRequestInfo = new LocalOnlyHotspotRequestInfo(mCallback, mDeathCallback, null);
        mLOHSRequestInfo.unlinkDeathRecipient();
        verify(mAppBinder).unlinkToDeath(eq(mLOHSRequestInfo), eq(0));
    }

    /**
     * Binder death notification should trigger a callback on the requestor.
     */
    @Test
    public void verifyBinderDeathTriggersCallback() {
        mLOHSRequestInfo = new LocalOnlyHotspotRequestInfo(mCallback, mDeathCallback, null);
        mLOHSRequestInfo.binderDied();
        verify(mDeathCallback).onLocalOnlyHotspotRequestorDeath(eq(mLOHSRequestInfo));
    }

    /**
     * Verify a RemoteException when calling linkToDeath triggers the callback.
     */
    @Test
    public void verifyRemoteExceptionTriggersCallback() throws Exception {
        doThrow(mRemoteException).when(mAppBinder)
                .linkToDeath(any(IBinder.DeathRecipient.class), eq(0));
        mLOHSRequestInfo = new LocalOnlyHotspotRequestInfo(mCallback, mDeathCallback, null);
        verify(mDeathCallback).onLocalOnlyHotspotRequestorDeath(eq(mLOHSRequestInfo));
    }

    /**
     * Verify the pid is properly set.
     */
    @Test
    public void verifyPid() {
        mLOHSRequestInfo = new LocalOnlyHotspotRequestInfo(mCallback, mDeathCallback, null);
        assertEquals(Process.myPid(), mLOHSRequestInfo.getPid());
    }

    @Test
    public void verifyCustomConfig() {
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setSsid("customSsid")
                .build();
        mLOHSRequestInfo = new LocalOnlyHotspotRequestInfo(mCallback, mDeathCallback, config);
        assertEquals(config, mLOHSRequestInfo.getCustomConfig());
    }

    /**
     * Verify that sendHotspotFailedMessage does send a Message properly
     */
    @Test
    public void verifySendFailedMessage() throws Exception {
        mLOHSRequestInfo = new LocalOnlyHotspotRequestInfo(mCallback, mDeathCallback, null);
        mLOHSRequestInfo.sendHotspotFailedMessage(
                WifiManager.LocalOnlyHotspotCallback.ERROR_GENERIC);
        verify(mCallback).onHotspotFailed(WifiManager.LocalOnlyHotspotCallback.ERROR_GENERIC);
    }

    /**
     * Verify that sendHotspotStartedMessage does send a Message properly
     */
    @Test
    public void verifySendStartedMessage() throws Exception {
        mLOHSRequestInfo = new LocalOnlyHotspotRequestInfo(mCallback, mDeathCallback, null);
        SoftApConfiguration config = mock(SoftApConfiguration.class);
        mLOHSRequestInfo.sendHotspotStartedMessage(config);
        ArgumentCaptor<SoftApConfiguration> mSoftApConfigCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);

        verify(mCallback).onHotspotStarted(mSoftApConfigCaptor.capture());
        assertEquals(config, mSoftApConfigCaptor.getValue());
    }

    /**
     * Verify that sendHotspotStoppedMessage does send a Message properly
     */
    @Test
    public void verifySendStoppedMessage() throws Exception {
        mLOHSRequestInfo = new LocalOnlyHotspotRequestInfo(mCallback, mDeathCallback, null);
        mLOHSRequestInfo.sendHotspotStoppedMessage();
        verify(mCallback).onHotspotStopped();
    }
}
