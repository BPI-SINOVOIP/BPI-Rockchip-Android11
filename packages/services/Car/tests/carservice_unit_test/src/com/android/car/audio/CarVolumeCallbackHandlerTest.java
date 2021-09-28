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

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.car.media.ICarVolumeCallback;
import android.os.RemoteException;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class CarVolumeCallbackHandlerTest {
    private static final int ZONE_ID = 2;
    private static final int GROUP_ID = 5;
    private static final int FLAGS = 0;

    private CarVolumeCallbackHandler mHandler;
    private TestCarVolumeCallback mCallback1;
    private TestCarVolumeCallback mCallback2;

    @Before
    public void setUp() {
        mHandler = new CarVolumeCallbackHandler();
        mCallback1 = new TestCarVolumeCallback(spy(ICarVolumeCallback.class));
        mHandler.registerCallback(mCallback1.asBinder());
        mCallback2 = new TestCarVolumeCallback(spy(ICarVolumeCallback.class));
        mHandler.registerCallback(mCallback2.asBinder());
    }

    @After
    public void tearDown() {
        mHandler.unregisterCallback(mCallback1.asBinder());
        mHandler.unregisterCallback(mCallback2.asBinder());
    }

    @Test
    public void onVolumeGroupChange_callsAllRegisteredCallbacks() throws RemoteException {
        mHandler.onVolumeGroupChange(ZONE_ID, GROUP_ID, FLAGS);

        verify(mCallback1.getSpy()).onGroupVolumeChanged(ZONE_ID, GROUP_ID, FLAGS);
        verify(mCallback2.getSpy()).onGroupVolumeChanged(ZONE_ID, GROUP_ID, FLAGS);
    }

    @Test
    public void onVolumeGroupChange_doesntCallUnregisteredCallbacks() throws RemoteException {
        mHandler.unregisterCallback(mCallback1.asBinder());
        mHandler.onVolumeGroupChange(ZONE_ID, GROUP_ID, FLAGS);

        verify(mCallback1.getSpy(), never()).onGroupVolumeChanged(ZONE_ID, GROUP_ID, FLAGS);
        verify(mCallback2.getSpy()).onGroupVolumeChanged(ZONE_ID, GROUP_ID, FLAGS);
    }

    @Test
    public void onMasterMuteChanged_callsAllRegisteredCallbacks() throws RemoteException {
        mHandler.onMasterMuteChanged(ZONE_ID, FLAGS);

        verify(mCallback1.getSpy()).onMasterMuteChanged(ZONE_ID, FLAGS);
        verify(mCallback2.getSpy()).onMasterMuteChanged(ZONE_ID, FLAGS);
    }

    // Because the binder logic uses transact, spying on the object directly doesn't work. So
    // instead we pass a mSpy in and have the Test wrapper call it so we can test the behavior
    private class TestCarVolumeCallback extends ICarVolumeCallback.Stub {
        private final ICarVolumeCallback mSpy;

        TestCarVolumeCallback(ICarVolumeCallback spy) {
            this.mSpy = spy;
        }

        public ICarVolumeCallback getSpy() {
            return mSpy;
        }

        @Override
        public void onGroupVolumeChanged(int zoneId, int groupId, int flags)
                throws RemoteException {
            mSpy.onGroupVolumeChanged(zoneId, groupId, flags);
        }

        @Override
        public void onMasterMuteChanged(int zoneId, int flags) throws RemoteException {
            mSpy.onMasterMuteChanged(zoneId, flags);
        }
    }
}
