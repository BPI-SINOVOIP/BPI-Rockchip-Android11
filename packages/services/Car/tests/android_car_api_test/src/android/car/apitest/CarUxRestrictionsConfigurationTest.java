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

package android.car.apitest;

import static android.car.drivingstate.CarDrivingStateEvent.DRIVING_STATE_IDLING;
import static android.car.drivingstate.CarDrivingStateEvent.DRIVING_STATE_MOVING;
import static android.car.drivingstate.CarDrivingStateEvent.DRIVING_STATE_PARKED;
import static android.car.drivingstate.CarDrivingStateEvent.DRIVING_STATE_UNKNOWN;
import static android.car.drivingstate.CarUxRestrictions.UX_RESTRICTIONS_BASELINE;
import static android.car.drivingstate.CarUxRestrictions.UX_RESTRICTIONS_FULLY_RESTRICTED;
import static android.car.drivingstate.CarUxRestrictions.UX_RESTRICTIONS_NO_VIDEO;
import static android.car.drivingstate.CarUxRestrictionsConfiguration.Builder.SpeedRange.MAX_SPEED;
import static android.car.drivingstate.CarUxRestrictionsManager.UX_RESTRICTION_MODE_BASELINE;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.car.drivingstate.CarUxRestrictions;
import android.car.drivingstate.CarUxRestrictionsConfiguration;
import android.car.drivingstate.CarUxRestrictionsConfiguration.Builder;
import android.car.drivingstate.CarUxRestrictionsConfiguration.DrivingStateRestrictions;
import android.os.Parcel;
import android.util.JsonReader;
import android.util.JsonWriter;

import androidx.test.filters.SmallTest;

import org.junit.Test;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.io.StringReader;

/**
 * Unit test for UXR config and its subclasses.
 */
@SmallTest
public class CarUxRestrictionsConfigurationTest {

    private static final String UX_RESTRICTION_MODE_PASSENGER = "passenger";

    // This test verifies the expected way to build config would succeed.
    @Test
    public void testConstruction() {
        new Builder().build();

        new Builder()
                .setMaxStringLength(1)
                .build();

        new Builder()
                .setUxRestrictions(DRIVING_STATE_PARKED, false, UX_RESTRICTIONS_BASELINE)
                .build();

        new Builder()
                .setUxRestrictions(DRIVING_STATE_MOVING, true, UX_RESTRICTIONS_FULLY_RESTRICTED)
                .build();

        new Builder()
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED)
                        .setSpeedRange(new Builder.SpeedRange(0f, MAX_SPEED)))
                .build();

        new Builder()
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED)
                        .setSpeedRange(new Builder.SpeedRange(0f, 1f)))
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED)
                        .setSpeedRange(new Builder.SpeedRange(1f, MAX_SPEED)))
                .build();
    }

    @Test
    public void testUnspecifiedDrivingStateUsesDefaultRestriction() {
        CarUxRestrictionsConfiguration config = new Builder().build();

        CarUxRestrictions parkedRestrictions = config.getUxRestrictions(DRIVING_STATE_PARKED, 0f);
        assertThat(parkedRestrictions.isRequiresDistractionOptimization()).isTrue();
        assertThat(parkedRestrictions.getActiveRestrictions())
                .isEqualTo(UX_RESTRICTIONS_FULLY_RESTRICTED);

        CarUxRestrictions movingRestrictions = config.getUxRestrictions(DRIVING_STATE_MOVING, 1f);
        assertThat(movingRestrictions.isRequiresDistractionOptimization()).isTrue();
        assertThat(movingRestrictions.getActiveRestrictions())
                .isEqualTo(UX_RESTRICTIONS_FULLY_RESTRICTED);
    }

    @Test
    public void testBuilderValidation_UnspecifiedStateUsesRestrictiveDefault() {
        CarUxRestrictionsConfiguration config = new Builder()
                .setUxRestrictions(DRIVING_STATE_MOVING, true, UX_RESTRICTIONS_FULLY_RESTRICTED)
                .build();
        assertThat(config.getUxRestrictions(DRIVING_STATE_PARKED, 0f)
        .isRequiresDistractionOptimization()).isTrue();
        assertThat(config.getUxRestrictions(DRIVING_STATE_IDLING, 0f)
        .isRequiresDistractionOptimization()).isTrue();
    }

    @Test
    public void testBuilderValidation_NonMovingStateHasOneRestriction() {
        Builder builder = new Builder();
        builder.setUxRestrictions(DRIVING_STATE_IDLING,
                true, UX_RESTRICTIONS_NO_VIDEO);
        builder.setUxRestrictions(DRIVING_STATE_IDLING,
                false, UX_RESTRICTIONS_BASELINE);

        assertThrows(Exception.class, () -> builder.build());
    }

    @Test
    public void testBuilderValidation_PassengerModeNoSpeedRangeOverlap() {
        Builder builder = new Builder();
        builder.setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                .setDistractionOptimizationRequired(true)
                .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED)
                .setSpeedRange(new Builder.SpeedRange(1f, 2f)));
        builder.setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                .setDistractionOptimizationRequired(true)
                .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED)
                .setSpeedRange(new Builder.SpeedRange(1f)));
        assertThrows(Exception.class, () -> builder.build());
    }

    @Test
    public void testBuilderValidation_PassengerModeCanSpecifySubsetOfSpeedRange() {
        CarUxRestrictionsConfiguration config = new Builder()
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER)
                        .setSpeedRange(new Builder.SpeedRange(1f, 2f)))
                .build();

        assertThat(config.getUxRestrictions(DRIVING_STATE_MOVING, 1f, UX_RESTRICTION_MODE_PASSENGER)
                .isRequiresDistractionOptimization()).isTrue();
    }

    @Test
    public void testBuilderValidation_MultipleSpeedRange_NonZeroStart() {
        Builder builder = new Builder();
        builder.setUxRestrictions(DRIVING_STATE_MOVING,
                new Builder.SpeedRange(1, 2),
                true, UX_RESTRICTIONS_FULLY_RESTRICTED);
        builder.setUxRestrictions(DRIVING_STATE_MOVING,
                new Builder.SpeedRange(2, MAX_SPEED),
                true, UX_RESTRICTIONS_FULLY_RESTRICTED);
        assertThrows(Exception.class, () -> builder.build());
    }

    @Test
    public void testBuilderValidation_SpeedRange_NonZeroStart() {
        Builder builder = new Builder();
        builder.setUxRestrictions(DRIVING_STATE_MOVING,
                new Builder.SpeedRange(1, MAX_SPEED),
                true, UX_RESTRICTIONS_FULLY_RESTRICTED);
        assertThrows(Exception.class, () -> builder.build());
    }

    @Test
    public void testBuilderValidation_SpeedRange_Overlap() {
        Builder builder = new Builder();
        builder.setUxRestrictions(DRIVING_STATE_MOVING,
                new Builder.SpeedRange(0, 5), true,
                UX_RESTRICTIONS_FULLY_RESTRICTED);
        builder.setUxRestrictions(DRIVING_STATE_MOVING,
                new Builder.SpeedRange(4), true,
                UX_RESTRICTIONS_FULLY_RESTRICTED);
        assertThrows(Exception.class, () -> builder.build());
    }

    @Test
    public void testBuilderValidation_SpeedRange_Gap() {
        Builder builder = new Builder();
        builder.setUxRestrictions(DRIVING_STATE_MOVING,
                new Builder.SpeedRange(0, 5), true,
                UX_RESTRICTIONS_FULLY_RESTRICTED);
        builder.setUxRestrictions(DRIVING_STATE_MOVING,
                new Builder.SpeedRange(8), true,
                UX_RESTRICTIONS_FULLY_RESTRICTED);
        assertThrows(Exception.class, () -> builder.build());
    }

    @Test
    public void testBuilderValidation_NonMovingStateCannotUseSpeedRange() {
        Builder builder = new Builder();
        assertThrows(Exception.class, () -> builder.setUxRestrictions(DRIVING_STATE_PARKED,
                new Builder.SpeedRange(0, 5), true, UX_RESTRICTIONS_FULLY_RESTRICTED));
    }

    @Test
    public void testBuilderValidation_MultipleMovingRestrictionsShouldAllContainSpeedRange() {
        Builder builder = new Builder();
        builder.setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                .setDistractionOptimizationRequired(true)
                .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED));
        builder.setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                .setDistractionOptimizationRequired(true)
                .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED)
                .setSpeedRange(new Builder.SpeedRange(1f)));
        assertThrows(Exception.class, () -> builder.build());
    }

    @Test
    public void testSpeedRange_Construction() {
        new Builder.SpeedRange(0f);
        new Builder.SpeedRange(0f, 1f);
        new Builder.SpeedRange(0f, MAX_SPEED);
    }

    @Test
    public void testSpeedRange_NoNegativeMin() {
        assertThrows(Exception.class, () -> new Builder.SpeedRange(-2f, 1f));
    }

    @Test
    public void testSpeedRange_NoNegativeMax() {
        assertThrows(Exception.class, () -> new Builder.SpeedRange(2f, -1f));
    }

    @Test
    public void testSpeedRange_MinCannotBeMaxSpeed() {
        assertThrows(Exception.class, () -> new Builder.SpeedRange(MAX_SPEED, 1f));
    }

    @Test
    public void testSpeedRange_MinGreaterThanMax() {
        assertThrows(Exception.class, () -> new Builder.SpeedRange(5f, 2f));
    }

    @Test
    public void testSpeedRangeComparison_DifferentMin() {
        Builder.SpeedRange s1 =
                new Builder.SpeedRange(1f);
        Builder.SpeedRange s2 =
                new Builder.SpeedRange(2f);
        assertThat(s1.compareTo(s2)).isLessThan(0);
        assertThat(s2.compareTo(s1)).isGreaterThan(0);
    }

    @Test
    public void testSpeedRangeComparison_SameMin() {
        Builder.SpeedRange s1 =
                new Builder.SpeedRange(1f);
        Builder.SpeedRange s2 =
                new Builder.SpeedRange(1f);
        assertThat(s1.compareTo(s2)).isEqualTo(0);
    }

    @Test
    public void testSpeedRangeComparison_SameMinDifferentMax() {
        Builder.SpeedRange s1 =
                new Builder.SpeedRange(0f, 1f);
        Builder.SpeedRange s2 =
                new Builder.SpeedRange(0f, 2f);
        assertThat(s1.compareTo(s2)).isLessThan(0);
        assertThat(s2.compareTo(s1)).isGreaterThan(0);
    }

    @Test
    public void testSpeedRangeComparison_MaxSpeed() {
        Builder.SpeedRange s1 =
                new Builder.SpeedRange(0f, 1f);
        Builder.SpeedRange s2 =
                new Builder.SpeedRange(0f);
        assertThat(s1.compareTo(s2)).isLessThan(0);
        assertThat(s2.compareTo(s1)).isGreaterThan(0);
    }

    @Test
    @SuppressWarnings("TruthSelfEquals")
    public void testSpeedRangeEquals() {
        Builder.SpeedRange s1, s2;

        s1 = new Builder.SpeedRange(0f);
        assertThat(s1).isEqualTo(s1);

        s1 = new Builder.SpeedRange(1f);
        s2 = new Builder.SpeedRange(1f);
        assertThat(s1.compareTo(s2)).isEqualTo(0);
        assertThat(s2).isEqualTo(s1);

        s1 = new Builder.SpeedRange(0f, 1f);
        s2 = new Builder.SpeedRange(0f, 1f);
        assertThat(s2).isEqualTo(s1);

        s1 = new Builder.SpeedRange(0f, MAX_SPEED);
        s2 = new Builder.SpeedRange(0f, MAX_SPEED);
        assertThat(s2).isEqualTo(s1);

        s1 = new Builder.SpeedRange(0f);
        s2 = new Builder.SpeedRange(1f);
        assertThat(s1).isNotEqualTo(s2);

        s1 = new Builder.SpeedRange(0f, 1f);
        s2 = new Builder.SpeedRange(0f, 2f);
        assertThat(s1).isNotEqualTo(s2);
    }

    @Test
    public void testJsonSerialization_DefaultConstructor() throws Exception {
        CarUxRestrictionsConfiguration config =
                new Builder().build();

        verifyConfigThroughJsonSerialization(config, /* schemaVersion= */ 2);
    }

    @Test
    public void testJsonSerialization_RestrictionParameters() throws Exception {
        CarUxRestrictionsConfiguration config = new Builder()
                .setMaxStringLength(1)
                .setMaxCumulativeContentItems(1)
                .setMaxContentDepth(1)
                .build();

        verifyConfigThroughJsonSerialization(config, /* schemaVersion= */ 2);
    }

    @Test
    public void testJsonSerialization_NonMovingStateRestrictions() throws Exception {
        CarUxRestrictionsConfiguration config = new Builder()
                .setUxRestrictions(DRIVING_STATE_PARKED, false, UX_RESTRICTIONS_BASELINE)
                .build();

        verifyConfigThroughJsonSerialization(config, /* schemaVersion= */ 2);
    }

    @Test
    public void testJsonSerialization_MovingStateNoSpeedRange() throws Exception {
        CarUxRestrictionsConfiguration config = new Builder()
                .setUxRestrictions(DRIVING_STATE_MOVING, true, UX_RESTRICTIONS_FULLY_RESTRICTED)
                .build();

        verifyConfigThroughJsonSerialization(config, /* schemaVersion= */ 2);
    }

    @Test
    public void testJsonSerialization_MovingStateWithSpeedRange() throws Exception {
        CarUxRestrictionsConfiguration config = new Builder()
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED)
                        .setSpeedRange(new Builder.SpeedRange(0f, 5f)))
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED)
                        .setSpeedRange(new Builder.SpeedRange(5f, MAX_SPEED)))
                .build();

        verifyConfigThroughJsonSerialization(config, /* schemaVersion= */ 2);
    }

    @Test
    public void testJsonSerialization_UxRestrictionMode() throws Exception {
        CarUxRestrictionsConfiguration config = new Builder()
                // Passenger mode
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(false)
                        .setRestrictions(UX_RESTRICTIONS_BASELINE)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER))
                // Explicitly specify baseline mode
                .setUxRestrictions(DRIVING_STATE_PARKED, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_NO_VIDEO)
                        .setMode(UX_RESTRICTION_MODE_BASELINE))
                // Implicitly defaults to baseline mode
                .setUxRestrictions(DRIVING_STATE_IDLING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_NO_VIDEO))
                .build();

        verifyConfigThroughJsonSerialization(config, /* schemaVersion= */ 2);
    }

    @Test
    public void testJsonSerialization_ReadsV1() throws Exception {
        String v1LegacyJsonFormat = "{\"physical_port\":1,\"max_content_depth\":2,"
                + "\"max_cumulative_content_items\":20,\"max_string_length\":21,"
                + "\"parked_restrictions\":[{\"req_opt\":false,\"restrictions\":0}],"
                + "\"idling_restrictions\":[{\"req_opt\":true,\"restrictions\":7}],"
                + "\"moving_restrictions\":[{\"req_opt\":true,\"restrictions\":8}],"
                + "\"unknown_restrictions\":[{\"req_opt\":true,\"restrictions\":511}],"
                + "\"passenger_parked_restrictions\":[{\"req_opt\":false,\"restrictions\":0}],"
                + "\"passenger_idling_restrictions\":[{\"req_opt\":true,\"restrictions\":56}],"
                + "\"passenger_moving_restrictions\":[{\"req_opt\":true,\"restrictions\":57}],"
                + "\"passenger_unknown_restrictions\":[{\"req_opt\":true,\"restrictions\":510}]}";
        CarUxRestrictionsConfiguration expectedConfig = new Builder()
                .setPhysicalPort((byte) 1)
                .setMaxContentDepth(2)
                .setMaxCumulativeContentItems(20)
                .setMaxStringLength(21)
                .setUxRestrictions(DRIVING_STATE_PARKED, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(false)
                        .setRestrictions(UX_RESTRICTIONS_BASELINE)
                        .setMode(UX_RESTRICTION_MODE_BASELINE))
                .setUxRestrictions(DRIVING_STATE_IDLING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(7)
                        .setMode(UX_RESTRICTION_MODE_BASELINE))
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(8)
                        .setMode(UX_RESTRICTION_MODE_BASELINE))
                .setUxRestrictions(DRIVING_STATE_UNKNOWN, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(511)
                        .setMode(UX_RESTRICTION_MODE_BASELINE))
                .setUxRestrictions(DRIVING_STATE_PARKED, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(false)
                        .setRestrictions(UX_RESTRICTIONS_BASELINE)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER))
                .setUxRestrictions(DRIVING_STATE_IDLING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(56)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER))
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(57)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER))
                .setUxRestrictions(DRIVING_STATE_UNKNOWN, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(510)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER))
                .build();

        CarUxRestrictionsConfiguration deserialized = CarUxRestrictionsConfiguration.readJson(
                new JsonReader(new StringReader(v1LegacyJsonFormat)), /* schemaVersion= */ 1);
        assertThat(deserialized).isEqualTo(expectedConfig);
    }


    @Test
    public void testJsonSerialization_ReadUnsupportedVersion_ThrowsException() throws Exception {
        int unsupportedVersion = -1;
        assertThrows(IllegalArgumentException.class, () -> CarUxRestrictionsConfiguration.readJson(
                new JsonReader(new StringReader("")), unsupportedVersion));
    }

    @Test
    public void testDump() {
        CarUxRestrictionsConfiguration[] configs = new CarUxRestrictionsConfiguration[]{
                // Driving state with no speed range
                new Builder()
                        .setUxRestrictions(DRIVING_STATE_PARKED, false, UX_RESTRICTIONS_BASELINE)
                        .setUxRestrictions(DRIVING_STATE_IDLING, true, UX_RESTRICTIONS_NO_VIDEO)
                        .setUxRestrictions(DRIVING_STATE_MOVING, true, UX_RESTRICTIONS_NO_VIDEO)
                        .build(),
                // Parameters
                new Builder()
                        .setMaxStringLength(1)
                        .setMaxContentDepth(1)
                        .setMaxCumulativeContentItems(1)
                        .build(),
                // Driving state with single speed range
                new Builder()
                        .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                                .setDistractionOptimizationRequired(true)
                                .setRestrictions(UX_RESTRICTIONS_NO_VIDEO)
                                .setSpeedRange(new Builder.SpeedRange(0f)))
                        .build(),
                // Driving state with multiple speed ranges
                new Builder()
                        .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                                .setDistractionOptimizationRequired(true)
                                .setRestrictions(UX_RESTRICTIONS_NO_VIDEO)
                                .setSpeedRange(new Builder.SpeedRange(0f, 1f)))
                        .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                                .setDistractionOptimizationRequired(true)
                                .setRestrictions(UX_RESTRICTIONS_NO_VIDEO)
                                .setSpeedRange(new Builder.SpeedRange(1f)))
                        .build(),
                // Driving state with passenger mode
                new Builder()
                        .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                                .setDistractionOptimizationRequired(false)
                                .setRestrictions(UX_RESTRICTIONS_BASELINE)
                                .setMode(UX_RESTRICTION_MODE_PASSENGER))
                        .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                                .setDistractionOptimizationRequired(true)
                                .setRestrictions(UX_RESTRICTIONS_NO_VIDEO)
                                .setMode(UX_RESTRICTION_MODE_BASELINE))
                        .build(),
        };

        for (CarUxRestrictionsConfiguration config : configs) {
            config.dump(new PrintWriter(new ByteArrayOutputStream()));
        }
    }

    @Test
    public void testDumpContainsNecessaryInfo() {
        CarUxRestrictionsConfiguration config = new Builder()
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_NO_VIDEO)
                        .setSpeedRange(new Builder.SpeedRange(0f, 1f)))
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_NO_VIDEO)
                        .setSpeedRange(new Builder.SpeedRange(1f)))
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(false)
                        .setRestrictions(UX_RESTRICTIONS_BASELINE)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER))
                .build();
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        try (PrintWriter writer = new PrintWriter(output)) {
            config.dump(writer);
        }

        String dump = new String(output.toByteArray());
        assertThat(dump).contains("Max String length");
        assertThat(dump).contains("Max Cumulative Content Items");
        assertThat(dump).contains("Max Content depth");
        assertThat(dump).contains("State:moving");
        assertThat(dump).contains("Speed Range");
        assertThat(dump).contains("Requires DO?");
        assertThat(dump).contains("Restrictions");
        assertThat(dump).contains("passenger mode");
        assertThat(dump).contains("baseline mode");
    }

    @Test
    public void testSetUxRestrictions_UnspecifiedModeDefaultsToBaseline() {
        CarUxRestrictionsConfiguration config = new Builder()
                .setUxRestrictions(DRIVING_STATE_PARKED, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_NO_VIDEO))
                .build();

        CarUxRestrictions restrictions = config.getUxRestrictions(DRIVING_STATE_PARKED, 0f);
        assertThat(restrictions.isRequiresDistractionOptimization()).isTrue();
        assertThat(restrictions.getActiveRestrictions()).isEqualTo(UX_RESTRICTIONS_NO_VIDEO);

        assertThat(restrictions.isSameRestrictions(
        config.getUxRestrictions(DRIVING_STATE_PARKED, 0f, UX_RESTRICTION_MODE_BASELINE))).isTrue();
    }

    @Test
    public void testSetUxRestrictions_PassengerMode() {
        CarUxRestrictionsConfiguration config = new Builder()
                .setUxRestrictions(DRIVING_STATE_PARKED, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(false)
                        .setRestrictions(UX_RESTRICTIONS_BASELINE)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER))
                .setUxRestrictions(DRIVING_STATE_PARKED, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_NO_VIDEO))
                .build();

        CarUxRestrictions passenger = config.getUxRestrictions(
                DRIVING_STATE_PARKED, 0f, UX_RESTRICTION_MODE_PASSENGER);
        assertThat(passenger.isRequiresDistractionOptimization()).isFalse();

        CarUxRestrictions baseline = config.getUxRestrictions(
                DRIVING_STATE_PARKED, 0f, UX_RESTRICTION_MODE_BASELINE);
        assertThat(baseline.isRequiresDistractionOptimization()).isTrue();
        assertThat(baseline.getActiveRestrictions()).isEqualTo(UX_RESTRICTIONS_NO_VIDEO);
    }

    @Test
    public void testGetUxRestrictions_WithUndefinedMode_FallbackToBaseline() {
        CarUxRestrictionsConfiguration config = new Builder()
                .setUxRestrictions(DRIVING_STATE_PARKED, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_NO_VIDEO))
                .build();

        CarUxRestrictions passenger = config.getUxRestrictions(
                DRIVING_STATE_PARKED, 0f, UX_RESTRICTION_MODE_PASSENGER);
        assertThat(passenger.isRequiresDistractionOptimization()).isTrue();
        assertThat(passenger.getActiveRestrictions()).isEqualTo(UX_RESTRICTIONS_NO_VIDEO);
    }

    @Test
    public void testPassengerMode_GetMovingWhenNotDefined_FallbackToBaseline() {
        CarUxRestrictionsConfiguration config = new Builder()
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_NO_VIDEO))
                .setUxRestrictions(DRIVING_STATE_PARKED, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(false)
                        .setRestrictions(UX_RESTRICTIONS_BASELINE)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER))
                .build();

        // Retrieve with passenger mode for a moving state
        CarUxRestrictions passenger = config.getUxRestrictions(
                DRIVING_STATE_MOVING, 1f, UX_RESTRICTION_MODE_PASSENGER);
        assertThat(passenger.isRequiresDistractionOptimization()).isTrue();
        assertThat(passenger.getActiveRestrictions()).isEqualTo(UX_RESTRICTIONS_NO_VIDEO);
    }

    @Test
    public void testPassengerMode_GetSpeedOutsideDefinedRange_FallbackToBaseline() {
        CarUxRestrictionsConfiguration config = new Builder()
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(true)
                        .setRestrictions(UX_RESTRICTIONS_NO_VIDEO))
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setDistractionOptimizationRequired(false)
                        .setRestrictions(UX_RESTRICTIONS_BASELINE)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER)
                        .setSpeedRange(new Builder.SpeedRange(3f)))
                .build();

        // Retrieve at speed within passenger mode range.
        CarUxRestrictions passenger = config.getUxRestrictions(
                DRIVING_STATE_MOVING, 5f, UX_RESTRICTION_MODE_PASSENGER);
        assertThat(passenger.isRequiresDistractionOptimization()).isFalse();

        // Retrieve with passenger mode but outside speed range
        CarUxRestrictions baseline = config.getUxRestrictions(
                DRIVING_STATE_MOVING, 1f, UX_RESTRICTION_MODE_PASSENGER);
        assertThat(baseline.isRequiresDistractionOptimization()).isTrue();
        assertThat(baseline.getActiveRestrictions()).isEqualTo(UX_RESTRICTIONS_NO_VIDEO);
    }

    @Test
    public void testHasSameParameters_SameParameters() {
        CarUxRestrictionsConfiguration one = new CarUxRestrictionsConfiguration.Builder()
                .setMaxStringLength(1)
                .setMaxCumulativeContentItems(1)
                .setMaxContentDepth(1)
                .build();

        CarUxRestrictionsConfiguration other = new CarUxRestrictionsConfiguration.Builder()
                .setMaxStringLength(1)
                .setMaxCumulativeContentItems(1)
                .setMaxContentDepth(1)
                .build();

        assertThat(one.hasSameParameters(other)).isTrue();
    }

    @Test
    public void testHasSameParameters_DifferentParameters() {
        CarUxRestrictionsConfiguration one = new CarUxRestrictionsConfiguration.Builder()
                .setMaxStringLength(2)
                .setMaxCumulativeContentItems(1)
                .setMaxContentDepth(1)
                .build();

        CarUxRestrictionsConfiguration other = new CarUxRestrictionsConfiguration.Builder()
                .setMaxStringLength(1)
                .setMaxCumulativeContentItems(1)
                .setMaxContentDepth(1)
                .build();

        assertThat(one.hasSameParameters(other)).isFalse();
    }

    @Test
    public void testConfigurationEquals() {
        CarUxRestrictionsConfiguration one = new CarUxRestrictionsConfiguration.Builder()
                .setMaxStringLength(2)
                .setMaxCumulativeContentItems(1)
                .setMaxContentDepth(1)
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions())
                .setUxRestrictions(DRIVING_STATE_PARKED,
                        new DrivingStateRestrictions().setRestrictions(UX_RESTRICTIONS_NO_VIDEO))
                .build();

        CarUxRestrictionsConfiguration other = new CarUxRestrictionsConfiguration.Builder()
                .setMaxStringLength(2)
                .setMaxCumulativeContentItems(1)
                .setMaxContentDepth(1)
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions())
                .setUxRestrictions(DRIVING_STATE_PARKED,
                        new DrivingStateRestrictions().setRestrictions(UX_RESTRICTIONS_NO_VIDEO))
                .build();

        assertThat(other).isEqualTo(one);
        assertThat(other.hashCode()).isEqualTo(one.hashCode());
    }

    @Test
    public void testConfigurationEquals_DifferentRestrictions() {
        CarUxRestrictionsConfiguration one = new CarUxRestrictionsConfiguration.Builder()
                .setMaxStringLength(2)
                .setMaxCumulativeContentItems(1)
                .setMaxContentDepth(1)
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions())
                .setUxRestrictions(DRIVING_STATE_PARKED,
                        new DrivingStateRestrictions().setRestrictions(
                                UX_RESTRICTIONS_FULLY_RESTRICTED))
                .build();

        CarUxRestrictionsConfiguration other = new CarUxRestrictionsConfiguration.Builder()
                .setMaxStringLength(2)
                .setMaxCumulativeContentItems(1)
                .setMaxContentDepth(1)
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions())
                .setUxRestrictions(DRIVING_STATE_PARKED,
                        new DrivingStateRestrictions().setRestrictions(UX_RESTRICTIONS_BASELINE))
                .build();

        assertThat(one.equals(other)).isFalse();
    }

    @Test
    public void testParcelableConfiguration() {
        CarUxRestrictionsConfiguration config = new CarUxRestrictionsConfiguration.Builder()
                .setPhysicalPort((byte) 1)
                .setMaxStringLength(1)
                .setMaxCumulativeContentItems(1)
                .setMaxContentDepth(1)
                .setUxRestrictions(DRIVING_STATE_PARKED,
                        new DrivingStateRestrictions().setRestrictions(
                                UX_RESTRICTIONS_FULLY_RESTRICTED))
                .setUxRestrictions(DRIVING_STATE_PARKED, new DrivingStateRestrictions()
                        .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER))
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions())
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions()
                        .setRestrictions(UX_RESTRICTIONS_FULLY_RESTRICTED)
                        .setMode(UX_RESTRICTION_MODE_PASSENGER)
                        .setSpeedRange(new Builder.SpeedRange(0f, 5f)))
                .build();
        Parcel parcel = Parcel.obtain();
        config.writeToParcel(parcel, 0);

        // Reset parcel data position for reading.
        parcel.setDataPosition(0);

        CarUxRestrictionsConfiguration deserialized =
                CarUxRestrictionsConfiguration.CREATOR.createFromParcel(parcel);
        assertThat(config).isEqualTo(deserialized);
    }

    @Test
    public void testParcelableConfiguration_serializeNullPhysicalPort() {
        // Not setting physical port leaves it null.
        CarUxRestrictionsConfiguration config = new CarUxRestrictionsConfiguration.Builder()
                .setMaxStringLength(1)
                .setMaxCumulativeContentItems(1)
                .setMaxContentDepth(1)
                .setUxRestrictions(DRIVING_STATE_MOVING, new DrivingStateRestrictions())
                .setUxRestrictions(DRIVING_STATE_PARKED,
                        new DrivingStateRestrictions().setRestrictions(
                                UX_RESTRICTIONS_FULLY_RESTRICTED))
                .build();
        Parcel parcel = Parcel.obtain();
        config.writeToParcel(parcel, 0);

        // Reset parcel data position for reading.
        parcel.setDataPosition(0);

        CarUxRestrictionsConfiguration deserialized =
                CarUxRestrictionsConfiguration.CREATOR.createFromParcel(parcel);
        assertThat(config).isEqualTo(deserialized);
        assertThat(deserialized.getPhysicalPort()).isNull();
    }

    /**
     * Writes input config as json, then reads a config out of json.
     * Asserts the deserialized config is the same as input.
     */
    private void verifyConfigThroughJsonSerialization(CarUxRestrictionsConfiguration config,
            int schemaVersion) throws Exception {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        try (JsonWriter writer = new JsonWriter(new OutputStreamWriter(out))) {
            config.writeJson(writer);
        }

        ByteArrayInputStream in = new ByteArrayInputStream(out.toByteArray());
        try (JsonReader reader = new JsonReader(new InputStreamReader(in))) {
            CarUxRestrictionsConfiguration deserialized = CarUxRestrictionsConfiguration.readJson(
                    reader, schemaVersion);
            assertThat(deserialized).isEqualTo(config);
        }
    }
}
