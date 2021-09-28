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

package com.android.car;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.car.IPerUserCarService;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.RemoteException;
import android.provider.Settings;
import android.test.mock.MockContentProvider;
import android.test.mock.MockContentResolver;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnitRunner;
import org.mockito.stubbing.Answer;

/**
 * Unit tests for {@link CarBluetoothService}
 *
 * Run:
 * atest CarBluetoothServiceTest
 *
 * Tests:
 * 1) Verify that, when the useDefaultConnectionPolicy resource overlay flag is true, we create and
 *    use the default connection policy.
 * 2) Verify that, when the useDefaultConnectionPolicy resource overlay flag is false, we do not
 *    create and use the default connection policy.
 */
@RunWith(MockitoJUnitRunner.class)
public class CarBluetoothServiceTest {
    private CarBluetoothService mCarBluetoothService;

    @Mock private Context mMockContext;
    @Mock private Resources mMockResources;
    private MockContentResolver mMockContentResolver;
    private MockContentProvider mMockContentProvider;
    @Mock private PackageManager mMockPackageManager;

    @Mock private PerUserCarServiceHelper mMockUserSwitchService;
    @Mock private IPerUserCarService mMockPerUserCarService;
    @Mock private CarBluetoothUserService mMockBluetoothUserService;
    private PerUserCarServiceHelper.ServiceCallback mUserSwitchCallback;

    //--------------------------------------------------------------------------------------------//
    // Setup/TearDown                                                                             //
    //--------------------------------------------------------------------------------------------//

    @Before
    public void setUp() {
        mMockContentResolver = new MockContentResolver(null);
        mMockContentProvider = new MockContentProvider() {
            @Override
            public Bundle call(String method, String request, Bundle args) {
                return new Bundle();
            }
        };
        mMockContentResolver.addProvider(Settings.AUTHORITY, mMockContentProvider);

        when(mMockContext.getResources()).thenReturn(mMockResources);
        when(mMockContext.getContentResolver()).thenReturn(mMockContentResolver);
        when(mMockContext.getApplicationContext()).thenReturn(mMockContext);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);

        // Make sure we grab and store CarBluetoothService's user switch callback so we can
        // invoke it at any time.
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                Object[] arguments = invocation.getArguments();
                if (arguments != null && arguments.length == 1 && arguments[0] != null) {
                    mUserSwitchCallback =
                            (PerUserCarServiceHelper.ServiceCallback) arguments[0];
                }
                return null;
            }
        }).when(mMockUserSwitchService).registerServiceCallback(any(
                PerUserCarServiceHelper.ServiceCallback.class));

        try {
            when(mMockPerUserCarService.getBluetoothUserService()).thenReturn(
                    mMockBluetoothUserService);
        } catch (RemoteException e) {
            Assert.fail();
        }
    }

    @After
    public void tearDown() {
        if (mCarBluetoothService != null) {
            mCarBluetoothService.release();
            mCarBluetoothService = null;
        }
    }

    //--------------------------------------------------------------------------------------------//
    // Policy Initialization Tests                                                                //
    //--------------------------------------------------------------------------------------------//

    /**
     * Preconditions:
     * - Policy flag is true
     *
     * Action:
     * - Initialize service
     *
     * Outcome:
     * - Default policy should be created
     */
    @Test
    public void testResourceFlagTrue_doCreateDefaultPolicy() {
        when(mMockResources.getBoolean(
                R.bool.useDefaultBluetoothConnectionPolicy)).thenReturn(true);
        mCarBluetoothService = new CarBluetoothService(mMockContext, mMockUserSwitchService);
        mCarBluetoothService.init();
        mUserSwitchCallback.onServiceConnected(mMockPerUserCarService);
        Assert.assertTrue(mCarBluetoothService.isUsingDefaultConnectionPolicy());
    }

    /**
     * Preconditions:
     * - Policy flag is false
     *
     * Action:
     * - Initialize service
     *
     * Outcome:
     * - Default policy should not be created
     */
    @Test
    public void testResourceFlagFalse_doNotCreateDefaultPolicy() {
        when(mMockResources.getBoolean(
                R.bool.useDefaultBluetoothConnectionPolicy)).thenReturn(false);
        mCarBluetoothService = new CarBluetoothService(mMockContext, mMockUserSwitchService);
        mCarBluetoothService.init();
        mUserSwitchCallback.onServiceConnected(mMockPerUserCarService);
        Assert.assertFalse(mCarBluetoothService.isUsingDefaultConnectionPolicy());
    }
}
