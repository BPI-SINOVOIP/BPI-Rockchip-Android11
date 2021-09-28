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

package com.android.car;

import static com.google.common.truth.Truth.assertThat;

import android.car.Car;
import android.car.CarFeatures;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.util.Log;

import androidx.test.annotation.UiThreadTest;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;

import com.android.car.vehiclehal.VehiclePropValueBuilder;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class CarFeatureControllerTest extends MockedCarTestBase {
    private static final String TAG = CarFeatureControllerTest.class.getSimpleName();
    private static final String[] ENABLED_OPTIONAL_FEATURES = {
            CarFeatures.FEATURE_CAR_USER_NOTICE_SERVICE,
            Car.STORAGE_MONITORING_SERVICE
    };
    private String mDisabledOptionalFeatures = "";

    @Override
    protected void configureMockedHal() {
        Log.i(TAG, "mDisabledOptionalFeatures:" + mDisabledOptionalFeatures);
        addProperty(VehicleProperty.DISABLED_OPTIONAL_FEATURES,
                VehiclePropValueBuilder.newBuilder(VehicleProperty.DISABLED_OPTIONAL_FEATURES)
                        .setStringValue(mDisabledOptionalFeatures)
                        .build());
    }

    @Override
    protected void configureResourceOverrides(MockResources resources) {
        super.configureResourceOverrides(resources);
        resources.overrideResource(com.android.car.R.array.config_allowed_optional_car_features,
                ENABLED_OPTIONAL_FEATURES);
    }

    @Override
    public void setUp() throws Exception {
        // Do nothing so that we can call super.setUp in test itself.
    }

    @Test
    @UiThreadTest
    public void testParsingVhalEmptyList() throws Exception {
        super.setUp();
        CarFeatureController featureController = CarLocalServices.getService(
                CarFeatureController.class);
        assertThat(featureController).isNotNull();
        List<String> disabledFeatures = featureController.getDisabledFeaturesFromVhal();
        assertThat(disabledFeatures).isEmpty();
    }

    @Test
    @UiThreadTest
    public void testParsingVhalMultipleEntries() throws Exception {
        String[] disabledFeaturesExpected = {"com.aaa", "com.bbb"};
        mDisabledOptionalFeatures = String.join(",", disabledFeaturesExpected);
        super.setUp();
        CarFeatureController featureController = CarLocalServices.getService(
                CarFeatureController.class);
        assertThat(featureController).isNotNull();
        List<String> disabledFeatures = featureController.getDisabledFeaturesFromVhal();
        assertThat(disabledFeatures).hasSize(disabledFeaturesExpected.length);
        for (String feature: disabledFeaturesExpected) {
            assertThat(disabledFeatures).contains(feature);
        }
    }

    @Test
    @UiThreadTest
    public void testUserNoticeDisabledFromVhal() throws Exception {
        mDisabledOptionalFeatures = CarFeatures.FEATURE_CAR_USER_NOTICE_SERVICE;
        super.setUp();
        CarFeatureController featureController = CarLocalServices.getService(
                CarFeatureController.class);
        assertThat(featureController).isNotNull();
        List<String> disabledFeatures = featureController.getDisabledFeaturesFromVhal();
        assertThat(disabledFeatures).contains(CarFeatures.FEATURE_CAR_USER_NOTICE_SERVICE);
        assertThat(featureController.isFeatureEnabled(CarFeatures.FEATURE_CAR_USER_NOTICE_SERVICE))
                .isFalse();
        assertThat(featureController.isFeatureEnabled(Car.STORAGE_MONITORING_SERVICE)).isTrue();
    }
}
