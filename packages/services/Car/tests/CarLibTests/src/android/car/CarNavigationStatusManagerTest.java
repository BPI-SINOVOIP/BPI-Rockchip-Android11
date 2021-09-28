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

import android.car.navigation.CarNavigationInstrumentCluster;
import android.car.navigation.CarNavigationStatusManager;
import android.car.testapi.CarNavigationStatusController;
import android.car.testapi.FakeCar;
import android.content.Context;
import android.os.Bundle;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.internal.DoNotInstrument;

@RunWith(RobolectricTestRunner.class)
@DoNotInstrument
public class CarNavigationStatusManagerTest {
    private Context mContext;
    private FakeCar mFakeCar;
    private Car mCar;
    private CarNavigationStatusManager mCarNavigationStatusManager;
    private CarNavigationStatusController mCarNavigationStatusController;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mFakeCar = FakeCar.createFakeCar(mContext);
        mCar = mFakeCar.getCar();
        mCarNavigationStatusManager =
                (CarNavigationStatusManager) mCar.getCarManager(Car.CAR_NAVIGATION_SERVICE);
        mCarNavigationStatusController = mFakeCar.getCarNavigationStatusController();

        // There should be no value after set up of the service.
        assertThat(mCarNavigationStatusController.getCurrentNavState()).isNull();
    }

    @Test
    public void onNavigationStateChanged_bundleIsReceived() {
        Bundle bundle = new Bundle();
        mCarNavigationStatusManager.sendNavigationStateChange(bundle);

        assertThat(mCarNavigationStatusController.getCurrentNavState()).isEqualTo(bundle);
    }

    @Test
    public void getInstrumentClusterInfo_returnsImageCodeCluster() {
        // default cluster should be an image code cluster (no custom images)
        assertThat(mCarNavigationStatusManager.getInstrumentClusterInfo().getType()).isEqualTo(
                CarNavigationInstrumentCluster.CLUSTER_TYPE_IMAGE_CODES_ONLY);
    }

    @Test
    public void setImageCodeClusterInfo_returnsImageCodeCluster() {
        mCarNavigationStatusController.setImageCodeClusterInfo(42);

        CarNavigationInstrumentCluster instrumentCluster =
                mCarNavigationStatusManager.getInstrumentClusterInfo();

        assertThat(instrumentCluster.getType())
                .isEqualTo(CarNavigationInstrumentCluster.CLUSTER_TYPE_IMAGE_CODES_ONLY);
        assertThat(instrumentCluster.getMinIntervalMillis()).isEqualTo(42);
    }

    @Test
    public void setCustomImageClusterInfo_returnsCustomImageCluster() {
        mCarNavigationStatusController.setCustomImageClusterInfo(
                100,
                1024,
                768,
                32);

        CarNavigationInstrumentCluster instrumentCluster =
                mCarNavigationStatusManager.getInstrumentClusterInfo();

        assertThat(instrumentCluster.getType())
                .isEqualTo(CarNavigationInstrumentCluster.CLUSTER_TYPE_CUSTOM_IMAGES_SUPPORTED);
        assertThat(instrumentCluster.getMinIntervalMillis()).isEqualTo(100);
        assertThat(instrumentCluster.getImageWidth()).isEqualTo(1024);
        assertThat(instrumentCluster.getImageHeight()).isEqualTo(768);
        assertThat(instrumentCluster.getImageColorDepthBits()).isEqualTo(32);
    }
}
