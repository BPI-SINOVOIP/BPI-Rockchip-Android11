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
package android.platform.test.longevity;

import static com.google.common.truth.Truth.assertThat;

import android.os.Bundle;
import android.platform.test.longevity.proto.Configuration;
import android.platform.test.longevity.proto.Configuration.Scenario;
import android.platform.test.longevity.proto.Configuration.Schedule;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.Description;
import org.junit.runner.Runner;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Unit test the logic for {@link Profile}.
 */
@RunWith(JUnit4.class)
public class ProfileTest {
    // TODO(b/120508148): Add tests for current scenario and timestamp book-keeping logic and
    // I/O operations regarding parsing the profile.

    protected static final String PROFILE_OPTION_NAME = "profile";

    protected static final String VALID_CONFIG_KEY = "valid_config";
    protected static final Configuration VALID_CONFIG =
            Configuration.newBuilder()
                    .setSchedule(Schedule.TIMESTAMPED)
                    .addScenarios(
                            Scenario.newBuilder()
                                    .setAt("00:01:00")
                                    .setJourney(
                                            "android.platform.test.scenario.calendar.FlingWeekPage"))
                    .addScenarios(
                            Scenario.newBuilder()
                                    .setAt("00:04:00")
                                    .setJourney(
                                            "android.platform.test.scenario.calendar.FlingDayPage"))
                    .addScenarios(
                            Scenario.newBuilder()
                                    .setAt("00:02:00")
                                    .setJourney(
                                            "android.platform.test.scenario.calendar.FlingWeekPage"))
                    .build();
    private static final String CONFIG_WITH_INVALID_JOURNEY_KEY = "config_with_invalid_journey";
    protected static final Configuration CONFIG_WITH_INVALID_JOURNEY =
            Configuration.newBuilder()
                    .setSchedule(Schedule.TIMESTAMPED)
                    .addScenarios(
                            Scenario.newBuilder()
                                    .setAt("00:01:00")
                                    .setJourney(
                                            "android.platform.test.scenario.calendar.FlingWeekPage"))
                    .addScenarios(Scenario.newBuilder().setAt("00:02:00").setJourney("invalid"))
                    .build();
    private static final String CONFIG_WITH_MISSING_TIMESTAMPS_KEY =
            "config_with_missing_timestamps";
    protected static final ImmutableMap<String, Configuration> TEST_CONFIGS= ImmutableMap.of(
            VALID_CONFIG_KEY, VALID_CONFIG,
            CONFIG_WITH_INVALID_JOURNEY_KEY, CONFIG_WITH_INVALID_JOURNEY);
    private static final ImmutableList<String> AVAILABLE_JOURNEYS = ImmutableList.of(
            "android.platform.test.scenario.calendar.FlingWeekPage",
            "android.platform.test.scenario.calendar.FlingDayPage",
            "android.platform.test.scenario.calendar.FlingSchedulePage");

    private ArrayList<Runner> mMockInput;

    @Rule
    public ExpectedException exceptionThrown = ExpectedException.none();

    /**
     * Sets up the input list of mocked runners for test.
     */
    @Before
    public void setUp() {
        mMockInput = new ArrayList<Runner>();
        for (String testJourney : AVAILABLE_JOURNEYS) {
            Runner mockRunner = Mockito.mock(Runner.class);
            Description mockDescription = Mockito.mock(Description.class);
            Mockito.when(mockDescription.getDisplayName()).thenReturn(testJourney);
            Mockito.when(mockRunner.getDescription()).thenReturn(mockDescription);
            mMockInput.add(mockRunner);
        }
    }

    /**
     * Tests that the returned runners are ordered according to their scheduled timestamps.
     */
    @Test
    public void testProfileOrderingRespected() {
        ImmutableList<String> expectedJourneyOrder = ImmutableList.of(
            "android.platform.test.scenario.calendar.FlingWeekPage",
            "android.platform.test.scenario.calendar.FlingWeekPage",
            "android.platform.test.scenario.calendar.FlingDayPage");

        List<Runner> output =
                getProfile(getArguments(VALID_CONFIG_KEY)).getRunnerSequence(mMockInput);
        List<String> outputDescriptions = output.stream().map(r ->
                r.getDescription().getDisplayName()).collect(Collectors.toList());
        boolean respected = outputDescriptions.equals(expectedJourneyOrder);
        assertThat(respected).isTrue();
    }

    /**
     * Tests that an exception is thrown for profiles with invalid scenario names.
     */
    @Test
    public void testProfileWithInvalidScenarioThrows() {
        // An exception about nonexistent user journey should be thrown.
        exceptionThrown.expect(IllegalArgumentException.class);
        exceptionThrown.expectMessage("not found");
        exceptionThrown.expectMessage("invalid");

        // Attempt to apply a profile with invalid CUJ; the above exception should be thrown.
        List<Runner> output =
                getProfile(getArguments(CONFIG_WITH_INVALID_JOURNEY_KEY))
                        .getRunnerSequence(mMockInput);
    }

    protected class TestableProfile extends Profile {
        public TestableProfile(Bundle args) {
            super(args);
        }

        @Override
        protected Configuration getConfigurationArgument(Bundle args) {
            return TEST_CONFIGS.get(args.getString(PROFILE_OPTION_NAME));
        }
    }

    protected Profile getProfile(Bundle args) {
        return new TestableProfile(args);
    }

    protected Bundle getArguments(String configName) {
        Bundle args = new Bundle();
        args.putString(PROFILE_OPTION_NAME, configName);
        return args;
    }
}
