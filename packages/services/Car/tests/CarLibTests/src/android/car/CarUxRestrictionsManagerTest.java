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

package android.car;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import android.app.Application;
import android.car.drivingstate.CarUxRestrictions;
import android.car.drivingstate.CarUxRestrictionsManager;
import android.car.drivingstate.CarUxRestrictionsManager.OnUxRestrictionsChangedListener;
import android.car.testapi.CarUxRestrictionsController;
import android.car.testapi.FakeCar;
import android.os.RemoteException;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.internal.DoNotInstrument;

@RunWith(RobolectricTestRunner.class)
@DoNotInstrument
public class CarUxRestrictionsManagerTest {
    @Rule
    public MockitoRule rule = MockitoJUnit.rule();

    CarUxRestrictionsManager mCarUxRestrictionsManager;
    CarUxRestrictionsController mCarUxRestrictionsController;

    @Mock
    OnUxRestrictionsChangedListener mListener;

    @Before
    public void setUp() {
        Application context = ApplicationProvider.getApplicationContext();
        FakeCar fakeCar = FakeCar.createFakeCar(context);
        Car carApi = fakeCar.getCar();

        mCarUxRestrictionsManager =
                (CarUxRestrictionsManager) carApi.getCarManager(Car.CAR_UX_RESTRICTION_SERVICE);
        mCarUxRestrictionsController = fakeCar.getCarUxRestrictionController();
    }

    @Test
    public void getRestrictions_noRestrictionsSet_noRestrictionsPresent() {
        assertThat(mCarUxRestrictionsManager.getCurrentCarUxRestrictions().getActiveRestrictions())
                .isEqualTo(CarUxRestrictions.UX_RESTRICTIONS_BASELINE);
    }

    @Test
    public void setUxRestrictions_restrictionsRegistered() throws RemoteException {
        mCarUxRestrictionsController.setUxRestrictions(CarUxRestrictions.UX_RESTRICTIONS_NO_VIDEO);

        assertThat(mCarUxRestrictionsManager.getCurrentCarUxRestrictions().getActiveRestrictions())
                .isEqualTo(CarUxRestrictions.UX_RESTRICTIONS_NO_VIDEO);
    }

    @Test
    public void clearUxRestrictions_restrictionsCleared() throws RemoteException {
        mCarUxRestrictionsController
                .setUxRestrictions(CarUxRestrictions.UX_RESTRICTIONS_FULLY_RESTRICTED);
        mCarUxRestrictionsController.clearUxRestrictions();

        assertThat(mCarUxRestrictionsManager.getCurrentCarUxRestrictions().getActiveRestrictions())
                .isEqualTo(CarUxRestrictions.UX_RESTRICTIONS_BASELINE);
    }

    @Test
    public void isListenerRegistered_noListenerSet_returnsFalse() {
        assertThat(mCarUxRestrictionsController.isListenerRegistered()).isFalse();
    }

    @Test
    public void isListenerRegistered_listenerSet_returnsTrue() {
        mCarUxRestrictionsManager.registerListener(mListener);

        assertThat(mCarUxRestrictionsController.isListenerRegistered()).isTrue();
    }

    @Test
    public void setUxRestrictions_listenerRegistered_listenerTriggered() throws RemoteException {
        mCarUxRestrictionsManager.registerListener(mListener);
        mCarUxRestrictionsController
                .setUxRestrictions(CarUxRestrictions.UX_RESTRICTIONS_NO_TEXT_MESSAGE);

        verify(mListener).onUxRestrictionsChanged(any());
    }
}

