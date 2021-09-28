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

import static java.util.stream.Collectors.joining;
import static java.util.stream.Collectors.toList;
import static java.util.stream.Collectors.toMap;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.SystemClock;
import android.platform.test.longevity.proto.Configuration;
import android.platform.test.longevity.proto.Configuration.Scenario;
import android.platform.test.longevity.proto.Configuration.Schedule;
import android.util.Log;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.TimeZone;
import java.util.function.Function;

import org.junit.runner.Description;
import org.junit.runner.Runner;
import org.junit.runner.notification.RunListener;

/** A profile composer for device-side testing. */
public class Profile extends RunListener {
    @VisibleForTesting static final String PROFILE_OPTION_NAME = "profile";

    protected static final String PROFILE_EXTENSION = ".pb";

    private static final String LOG_TAG = Profile.class.getSimpleName();

    // Parser for parsing "at" timestamps in profiles.
    private static final SimpleDateFormat TIMESTAMP_FORMATTER = new SimpleDateFormat("HH:mm:ss");

    // Keeps track of the current scenario being run; updated at the end of a scenario.
    private int mScenarioIndex = 0;
    // A list of scenarios in the order that they will be run.
    private List<Scenario> mOrderedScenariosList;
    // Timestamp when the test run starts, defaults to time when the ProfileBase object is
    // constructed. Can be overridden by {@link setTestRunStartTimeMs}.
    private long mRunStartTimeMs = SystemClock.elapsedRealtime();
    // The profile configuration.
    private final Configuration mConfiguration;
    // The timestamp of the first scenario in milliseconds. All scenarios will be scheduled relative
    // to this timestamp.
    private long mFirstScenarioTimestampMs = 0;

    // Comparator for sorting timestamped CUJs.
    private static class ScenarioTimestampComparator implements Comparator<Scenario> {
        @Override
        public int compare(Scenario s1, Scenario s2) {
            if (!(s1.hasAt() && s2.hasAt())) {
                throw new IllegalArgumentException(
                      "Scenarios in scheduled profiles must have timestamps.");
            }
            return s1.getAt().compareTo(s2.getAt());
        }
    }

    // Comparator for sorting indexed CUJs.
    private static class ScenarioIndexedComparator implements Comparator<Scenario> {
        @Override
        public int compare(Scenario s1, Scenario s2) {
            if (!(s1.hasIndex() && s2.hasIndex())) {
                throw new IllegalArgumentException(
                        "Scenarios in indexed profiles must have indexes.");
            }
            return Integer.compare(s1.getIndex(), s2.getIndex());
        }
    }

    public Profile(Bundle args) {
        super();
        // Set the timestamp parser to UTC to get test timestamps as "time elapsed since zero".
        TIMESTAMP_FORMATTER.setTimeZone(TimeZone.getTimeZone("UTC"));

        // Load configuration from arguments and stored the list of scenarios sorted according to
        // their timestamps.
        mConfiguration = getConfigurationArgument(args);
        // When no configuration is supplied, behaves the same way as LongevitySuite but without
        // support for shuffle, iterate etc.
        if (mConfiguration == null) {
            return;
        }
        mOrderedScenariosList = new ArrayList<>(mConfiguration.getScenariosList());
        if (mOrderedScenariosList.isEmpty()) {
            throw new IllegalArgumentException("Profile must have at least one scenario.");
        }
        if (mConfiguration.getSchedule().equals(Schedule.TIMESTAMPED)) {
            Collections.sort(mOrderedScenariosList, new ScenarioTimestampComparator());
            try {
                mFirstScenarioTimestampMs =
                        TIMESTAMP_FORMATTER.parse(mOrderedScenariosList.get(0).getAt()).getTime();
            } catch (ParseException e) {
                throw new IllegalArgumentException(
                        "Cannot parse the timestamp of the first scenario.", e);
            }
        } else if (mConfiguration.getSchedule().equals(Schedule.INDEXED)) {
            Collections.sort(mOrderedScenariosList, new ScenarioIndexedComparator());
        } else {
            throw new UnsupportedOperationException(
                    "Only scheduled profiles are currently supported.");
        }
    }

    public List<Runner> getRunnerSequence(List<Runner> input) {
        if (mConfiguration == null) {
            return input;
        }
        return getTestSequenceFromConfiguration(mConfiguration, input);
    }

    protected List<Runner> getTestSequenceFromConfiguration(
            Configuration config, List<Runner> input) {
        Map<String, Runner> nameToRunner =
                input.stream()
                        .collect(
                                toMap(
                                        r -> r.getDescription().getDisplayName(),
                                        Function.identity()));
        Log.i(
                LOG_TAG,
                String.format(
                        "Available journeys: %s",
                        nameToRunner.keySet().stream().collect(joining(", "))));
        List<Runner> result =
                mOrderedScenariosList
                        .stream()
                        .map(Configuration.Scenario::getJourney)
                        .map(
                                journeyName -> {
                                    if (nameToRunner.containsKey(journeyName)) {
                                        return nameToRunner.get(journeyName);
                                    } else {
                                        // Write error message here to trick the auto-formatter.
                                        String errorFmtMessage =
                                                "Journey %s in profile not found. "
                                                + "Check logcat to see available journeys.";
                                        throw new IllegalArgumentException(
                                                String.format(errorFmtMessage, journeyName));
                                    }
                                })
                        .collect(toList());
        Log.i(
                LOG_TAG,
                String.format(
                        "Returned runners: %s",
                        result.stream()
                                .map(Runner::getDescription)
                                .map(Description::getDisplayName)
                                .collect(toList())));
        return result;
    }

    @Override
    public void testRunStarted(Description description) {
        mRunStartTimeMs = SystemClock.elapsedRealtime();
    }

    @Override
    public void testFinished(Description description) {
        // Increments the index to move onto the next scenario.
        mScenarioIndex += 1;
    }

    @Override
    public void testIgnored(Description description) {
        // Increments the index to move onto the next scenario.
        mScenarioIndex += 1;
    }

    /**
     * Returns true if there is a next scheduled scenario to run. If no profile is supplied, returns
     * false.
     */
    public boolean hasNextScheduledScenario() {
        return (mOrderedScenariosList != null)
                && (mScenarioIndex < mOrderedScenariosList.size() - 1);
    }

    /** Returns time in milliseconds until the next scenario. */
    public long getTimeUntilNextScenarioMs() {
        Scenario nextScenario = mOrderedScenariosList.get(mScenarioIndex + 1);
        if (nextScenario.hasAt()) {
            try {
                // Calibrate the start time against the first scenario's timestamp.
                long startTimeMs =
                        TIMESTAMP_FORMATTER.parse(nextScenario.getAt()).getTime()
                                - mFirstScenarioTimestampMs;
                // Time in milliseconds from the start of the test run to the current point in time.
                long currentTimeMs = getTimeSinceRunStartedMs();
                // If the next test should not start yet, sleep until its start time. Otherwise,
                // start it immediately.
                if (startTimeMs > currentTimeMs) {
                    return startTimeMs - currentTimeMs;
                }
            } catch (ParseException e) {
                throw new IllegalArgumentException(
                        String.format(
                                "Timestamp %s from scenario %s could not be parsed",
                                nextScenario.getAt(), nextScenario.getJourney()));
            }
        }
        // For non-scheduled profiles (not a priority at this point), simply return 0.
        return 0L;
    }

    /** Return time in milliseconds since the test run started. */
    public long getTimeSinceRunStartedMs() {
        return SystemClock.elapsedRealtime() - mRunStartTimeMs;
    }

    /** Returns the Scenario object for the current scenario. */
    public Scenario getCurrentScenario() {
        return mOrderedScenariosList.get(mScenarioIndex);
    }

    /** Returns the profile configuration. */
    public Configuration getConfiguration() {
        return mConfiguration;
    }

    /*
     * Parses the arguments, reads the configuration file and returns the Configuration object.
     *
     * If no profile option is found in the arguments, function should return null, in which case
     * the input sequence is returned without modification. Otherwise, function should parse the
     * profile according to the supplied argument and return the Configuration object or throw an
     * exception if the file is not available or cannot be parsed.
     *
     * The configuration should be passed as either the name of a configuration bundled into the APK
     * or a path to the configuration file.
     *
     * TODO(harrytczhang@): Write tests for this logic.
     */
    protected Configuration getConfigurationArgument(Bundle args) {
        // profileValue is either the name of a profile bundled with an APK or a path to a
        // profile configuration file.
        String profileValue = args.getString(PROFILE_OPTION_NAME, "");
        if (profileValue.isEmpty()) {
            return null;
        }
        // Look inside the APK assets for the profile; if this fails, try
        // using the profile argument as a path to a configuration file.
        InputStream configStream;
        try {
            AssetManager manager = InstrumentationRegistry.getContext().getAssets();
            String profileName = profileValue + PROFILE_EXTENSION;
            configStream = manager.open(profileName);
        } catch (IOException e) {
            // Try using the profile argument it as a path to a configuration file.
            try {
                File configFile = new File(profileValue);
                if (!configFile.exists()) {
                    throw new IllegalArgumentException(String.format(
                            "Profile %s does not exist.", profileValue));
                }
                configStream = new FileInputStream(configFile);
            } catch (IOException f) {
                throw new IllegalArgumentException(String.format(
                        "Profile %s cannot be opened.", profileValue));
            }
        }
        try {
            // Parse the configuration from its input stream and return it.
            return Configuration.parseFrom(configStream);
        } catch (IOException e) {
            throw new IllegalArgumentException(String.format(
                    "Cannot parse profile %s.", profileValue));
        } finally {
            try {
                configStream.close();
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
    }
}
