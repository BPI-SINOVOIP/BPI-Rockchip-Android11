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

package com.android.textclassifier.common.statsd;

import static com.google.common.truth.Truth.assertThat;

import android.app.Instrumentation;
import android.app.UiAutomation;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import androidx.test.platform.app.InstrumentationRegistry;
import com.android.internal.os.StatsdConfigProto.AtomMatcher;
import com.android.internal.os.StatsdConfigProto.EventMetric;
import com.android.internal.os.StatsdConfigProto.SimpleAtomMatcher;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.ConfigMetricsReport;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.os.StatsLog.EventMetricData;
import com.android.os.StatsLog.StatsLogReport;
import com.google.common.collect.ImmutableList;
import com.google.common.io.ByteStreams;
import java.io.ByteArrayInputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.lang.reflect.Method;
import java.util.Comparator;
import java.util.List;
import java.util.stream.Collectors;
import javax.annotation.Nullable;

/** Util functions to make statsd testing easier by using adb shell cmd stats commands. */
public class StatsdTestUtils {
  private static final String TAG = "StatsdTestUtils";
  private static final long SHORT_WAIT_MS = 1000;

  private StatsdTestUtils() {}

  /** Push a config which specifies what loggings we are interested in. */
  public static void pushConfig(StatsdConfig config) throws Exception {
    String command = String.format("cmd stats config update %s", config.getId());
    Log.v(TAG, "pushConfig: " + config);
    String output = new String(runShellCommand(command, config.toByteArray()));
    assertThat(output).isEmpty();
  }

  /** Adds a atom matcher to capture logs with given atom tag. */
  public static void addAtomMatcher(StatsdConfig.Builder builder, int atomTag) {
    final String atomName = "Atom" + atomTag;
    final String eventName = "Event" + atomTag;
    SimpleAtomMatcher simpleAtomMatcher = SimpleAtomMatcher.newBuilder().setAtomId(atomTag).build();
    builder.addAtomMatcher(
        AtomMatcher.newBuilder()
            .setId(atomName.hashCode())
            .setSimpleAtomMatcher(simpleAtomMatcher));
    builder.addEventMetric(
        EventMetric.newBuilder().setId(eventName.hashCode()).setWhat(atomName.hashCode()));
  }

  /**
   * Extracts logged atoms from the report, sorted by logging time, and deletes the saved report.
   */
  public static ImmutableList<Atom> getLoggedAtoms(long configId) throws Exception {
    // There is no callback to notify us the log is collected. So we do a short wait here.
    Thread.sleep(SHORT_WAIT_MS);

    ConfigMetricsReportList reportList = getAndRemoveReportList(configId);
    assertThat(reportList.getReportsCount()).isEqualTo(1);
    ConfigMetricsReport report = reportList.getReports(0);
    List<StatsLogReport> metricsList = report.getMetricsList();
    return ImmutableList.copyOf(
        metricsList.stream()
            .flatMap(statsLogReport -> statsLogReport.getEventMetrics().getDataList().stream())
            .sorted(Comparator.comparing(EventMetricData::getElapsedTimestampNanos))
            .map(EventMetricData::getAtom)
            .collect(Collectors.toList()));
  }

  /** Removes the pushed config file and existing reports. */
  public static void cleanup(long configId) throws Exception {
    runShellCommand(String.format("cmd stats config remove %d", configId), /* input= */ null);
    // Remove existing reports.
    getAndRemoveReportList(configId);
  }

  /**
   * Runs an adb shell command with the provided input and returns the command line output.
   *
   * @param cmd the shell command
   * @param input the content that will be piped to the command stdin.
   * @return the command output
   */
  private static byte[] runShellCommand(String cmd, @Nullable byte[] input) throws Exception {
    Log.v(TAG, "run shell command: " + cmd);
    Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
    UiAutomation uiAutomation = instrumentation.getUiAutomation();
    Method method =
        uiAutomation.getClass().getDeclaredMethod("executeShellCommandRw", String.class);
    ParcelFileDescriptor[] pipes = (ParcelFileDescriptor[]) method.invoke(uiAutomation, cmd);
    // Write to the input pipe.
    try (FileOutputStream fos = new ParcelFileDescriptor.AutoCloseOutputStream(pipes[1])) {
      if (input != null) {
        fos.write(input);
      }
    }
    // Read from the output pipe.
    try (FileInputStream inputStream = new ParcelFileDescriptor.AutoCloseInputStream(pipes[0])) {
      return ByteStreams.toByteArray(inputStream);
    }
  }

  /** Gets the statsd report. Note that this also deletes that report from statsd. */
  private static ConfigMetricsReportList getAndRemoveReportList(long configId) throws Exception {
    byte[] output =
        runShellCommand(
            String.format("cmd stats dump-report %d --include_current_bucket --proto", configId),
            /*input=*/ null);
    return ConfigMetricsReportList.parser().parseFrom(new ByteArrayInputStream(output));
  }
}
