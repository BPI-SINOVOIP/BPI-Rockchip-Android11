/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.car.apitest;

import static com.google.common.truth.Truth.assertThat;

import android.car.Car;
import android.car.CarFeatures;
import android.test.suitebuilder.annotation.SmallTest;

import org.junit.Test;

import java.util.Arrays;
import java.util.List;

@SmallTest
public class CarFeatureTest extends CarApiTestBase  {
    // List in CarFeatureController should be inline with this.
    private static final List<String> MANDATORY_FEATURES = Arrays.asList(
            Car.APP_FOCUS_SERVICE,
            Car.AUDIO_SERVICE,
            Car.BLUETOOTH_SERVICE,
            Car.CAR_BUGREPORT_SERVICE,
            Car.CAR_CONFIGURATION_SERVICE,
            Car.CAR_DRIVING_STATE_SERVICE,
            Car.CAR_MEDIA_SERVICE,
            Car.CAR_OCCUPANT_ZONE_SERVICE,
            Car.CAR_TRUST_AGENT_ENROLLMENT_SERVICE,
            Car.CAR_USER_SERVICE,
            Car.CAR_UX_RESTRICTION_SERVICE,
            Car.INFO_SERVICE,
            Car.PACKAGE_SERVICE,
            Car.POWER_SERVICE,
            Car.PROJECTION_SERVICE,
            Car.PROPERTY_SERVICE,
            Car.TEST_SERVICE,
            // All items below here are deprecated, but still should be supported
            Car.CAR_INSTRUMENT_CLUSTER_SERVICE,
            Car.CABIN_SERVICE,
            Car.HVAC_SERVICE,
            Car.SENSOR_SERVICE,
            Car.VENDOR_EXTENSION_SERVICE
    );

    private static final List<String> OPTIONAL_FEATURES = Arrays.asList(
            CarFeatures.FEATURE_CAR_USER_NOTICE_SERVICE,
            Car.CAR_NAVIGATION_SERVICE,
            Car.DIAGNOSTIC_SERVICE,
            Car.STORAGE_MONITORING_SERVICE,
            Car.VEHICLE_MAP_SERVICE
    );

    private static final String NON_EXISTING_FEATURE = "ThisFeatureDoesNotExist";

    @Test
    public void checkMandatoryFeatures() {
        Car car = getCar();
        assertThat(car).isNotNull();
        for (String feature : MANDATORY_FEATURES) {
            assertThat(car.isFeatureEnabled(feature)).isTrue();
        }
    }

    @Test
    public void toggleOptionalFeature() {
        Car car = getCar();
        assertThat(car).isNotNull();
        for (String feature : OPTIONAL_FEATURES) {
            boolean enabled = getCar().isFeatureEnabled(feature);
            toggleOptionalFeature(feature, !enabled, enabled);
            toggleOptionalFeature(feature, enabled, enabled);
        }
    }

    @Test
    public void testGetAllEnabledFeatures() {
        Car car = getCar();
        assertThat(car).isNotNull();
        List<String> allEnabledFeatures = car.getAllEnabledFeatures();
        assertThat(allEnabledFeatures).isNotEmpty();
        for (String feature : MANDATORY_FEATURES) {
            assertThat(allEnabledFeatures).contains(feature);
        }
    }

    @Test
    public void testEnableDisableForMandatoryFeatures() {
        for (String feature : MANDATORY_FEATURES) {
            assertThat(getCar().enableFeature(feature)).isEqualTo(Car.FEATURE_REQUEST_MANDATORY);
            assertThat(getCar().disableFeature(feature)).isEqualTo(Car.FEATURE_REQUEST_MANDATORY);
        }
    }

    @Test
    public void testEnableDisableForNonExistingFeature() {
        assertThat(getCar().enableFeature(NON_EXISTING_FEATURE)).isEqualTo(
                Car.FEATURE_REQUEST_NOT_EXISTING);
        assertThat(getCar().disableFeature(NON_EXISTING_FEATURE)).isEqualTo(
                Car.FEATURE_REQUEST_NOT_EXISTING);
    }

    private void toggleOptionalFeature(String feature, boolean enable, boolean originallyEnabled) {
        if (enable) {
            if (originallyEnabled) {
                assertThat(getCar().enableFeature(feature)).isEqualTo(
                        Car.FEATURE_REQUEST_ALREADY_IN_THE_STATE);
            } else {
                assertThat(getCar().enableFeature(feature)).isEqualTo(Car.FEATURE_REQUEST_SUCCESS);
                assertThat(getCar().getAllPendingEnabledFeatures()).contains(feature);
            }
            assertThat(getCar().getAllPendingDisabledFeatures()).doesNotContain(feature);
        } else {
            if (originallyEnabled) {
                assertThat(getCar().disableFeature(feature)).isEqualTo(Car.FEATURE_REQUEST_SUCCESS);
                assertThat(getCar().getAllPendingDisabledFeatures()).contains(feature);
            } else {
                assertThat(getCar().disableFeature(feature)).isEqualTo(
                        Car.FEATURE_REQUEST_ALREADY_IN_THE_STATE);
            }
            assertThat(getCar().getAllPendingEnabledFeatures()).doesNotContain(feature);
        }
    }
}
