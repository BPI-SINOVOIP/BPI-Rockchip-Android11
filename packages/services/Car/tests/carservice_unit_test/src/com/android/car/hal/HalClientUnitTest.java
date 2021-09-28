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
package com.android.car.hal;

import static android.car.test.mocks.CarArgumentMatchers.isProperty;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.testng.Assert.expectThrows;

import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.hardware.automotive.vehicle.V2_0.IVehicle;
import android.hardware.automotive.vehicle.V2_0.IVehicleCallback;
import android.hardware.automotive.vehicle.V2_0.StatusCode;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.os.Looper;
import android.os.RemoteException;
import android.os.ServiceSpecificException;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;

public final class HalClientUnitTest extends AbstractExtendedMockitoTestCase {

    private static final int WAIT_CAP_FOR_RETRIABLE_RESULT_MS = 100;
    private static final int SLEEP_BETWEEN_RETRIABLE_INVOKES_MS = 50;

    private static final int PROP = 42;
    private static final int AREA_ID = 108;

    private final VehiclePropValue mProp = new VehiclePropValue();

    @Mock IVehicle mIVehicle;
    @Mock IVehicleCallback mIVehicleCallback;

    private HalClient mClient;

    @Before
    public void setFixtures() {
        mClient = new HalClient(mIVehicle, Looper.getMainLooper(), mIVehicleCallback,
                WAIT_CAP_FOR_RETRIABLE_RESULT_MS, SLEEP_BETWEEN_RETRIABLE_INVOKES_MS);
        mProp.prop = PROP;
        mProp.areaId = AREA_ID;
    }

    @Test
    public void testSet_remoteExceptionThenFail() throws Exception {
        when(mIVehicle.set(isProperty(PROP)))
            .thenThrow(new RemoteException("Never give up, never surrender!"))
            .thenThrow(new RemoteException("D'OH!"));

        Exception actualException = expectThrows(ServiceSpecificException.class,
                () -> mClient.setValue(mProp));

        assertThat(actualException).hasMessageThat().contains(Integer.toHexString(PROP));
        assertThat(actualException).hasMessageThat().contains(Integer.toHexString(AREA_ID));
    }

    @Test
    public void testSet_remoteExceptionThenOk() throws Exception {
        when(mIVehicle.set(isProperty(PROP)))
            .thenThrow(new RemoteException("Never give up, never surrender!"))
            .thenReturn(StatusCode.OK);

        mClient.setValue(mProp);
    }

    @Test
    public void testSet_invalidArgument() throws Exception {
        when(mIVehicle.set(isProperty(PROP))).thenReturn(StatusCode.INVALID_ARG);

        Exception actualException = expectThrows(IllegalArgumentException.class,
                () -> mClient.setValue(mProp));

        assertThat(actualException).hasMessageThat().contains(Integer.toHexString(PROP));
        assertThat(actualException).hasMessageThat().contains(Integer.toHexString(AREA_ID));
    }

    @Test
    public void testSet_otherError() throws Exception {
        when(mIVehicle.set(isProperty(PROP))).thenReturn(StatusCode.INTERNAL_ERROR);

        Exception actualException = expectThrows(ServiceSpecificException.class,
                () -> mClient.setValue(mProp));

        assertThat(actualException).hasMessageThat().contains(Integer.toHexString(PROP));
        assertThat(actualException).hasMessageThat().contains(Integer.toHexString(AREA_ID));
    }

    @Test
    public void testSet_ok() throws Exception {
        when(mIVehicle.set(isProperty(PROP))).thenReturn(StatusCode.OK);
    }
}
