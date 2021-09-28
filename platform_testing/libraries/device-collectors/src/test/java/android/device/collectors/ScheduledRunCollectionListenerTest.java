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
package android.device.collectors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Instrumentation;
import android.device.collectors.util.SendToInstrumentation;
import android.os.Bundle;
import android.os.Environment;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.ICollectorHelper;

import java.io.File;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/** Android Unit tests for {@link ScheduledRunCollectionListener}. */
@RunWith(AndroidJUnit4.class)
public class ScheduledRunCollectionListenerTest {

    private static final String TEST_METRIC_KEY = "test_metric_key";
    private static final Integer[] TEST_METRIC_VALUES = {0, 1, 2, 3, 4};
    private static final long TEST_INTERVAL = 100L;
    private static final long TEST_DURATION = 450L;
    // Collects at 0ms, 100ms, 200ms, and so on.
    private static final long NUMBER_OF_COLLECTIONS = TEST_DURATION / TEST_INTERVAL + 1;
    private static final String DATA_REGEX =
            "(?<timestamp>[0-9]+)\\s+," + TEST_METRIC_KEY + "\\s+,(?<value>[0-9])\\s+";

    @Mock private ICollectorHelper mHelper;

    @Mock private Instrumentation mInstrumentation;

    private ScheduledRunCollectionListener mListener;

    private ScheduledRunCollectionListener initListener() {
        Bundle b = new Bundle();
        b.putString(ScheduledRunCollectionListener.INTERVAL_ARG_KEY, Long.toString(TEST_INTERVAL));
        doReturn(true).when(mHelper).startCollecting();
        Map<String, Integer> first = new HashMap<>();
        first.put(TEST_METRIC_KEY, TEST_METRIC_VALUES[0]);
        Map<String, Integer>[] rest =
                Arrays.stream(TEST_METRIC_VALUES)
                        .skip(1)
                        .map(
                                testMetricValue -> {
                                    Map<String, Integer> m = new HashMap<>();
                                    m.put(TEST_METRIC_KEY, testMetricValue);
                                    return m;
                                })
                        .toArray(Map[]::new);
        // <code>thenReturn</code> call signature requires thenReturn(T value, T... values).
        when(mHelper.getMetrics()).thenReturn(first, rest);
        doReturn(true).when(mHelper).stopCollecting();
        ScheduledRunCollectionListener listener =
                new ScheduledRunCollectionListener<Integer>(b, mHelper);
        // Mock getUiAutomation method for the purpose of enabling createAndEmptyDirectory method
        // from BaseMetricListener.
        doReturn(InstrumentationRegistry.getInstrumentation().getUiAutomation())
                .when(mInstrumentation)
                .getUiAutomation();
        listener.setInstrumentation(mInstrumentation);
        return listener;
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mListener = initListener();
    }

    @After
    public void tearDown() {
        // Remove files/directories that have been created.
        Path outputFilePath =
                Paths.get(
                        Environment.getExternalStorageDirectory().toString(),
                        ScheduledRunCollectionListener.OUTPUT_ROOT,
                        ScheduledRunCollectionListener.class.getSimpleName());
        mListener.executeCommandBlocking("rm -rf " + outputFilePath.toString());
    }

    @Test
    public void testCompleteRun() throws Exception {
        testRun(true);
    }

    @Test
    public void testIncompleteRun() throws Exception {
        testRun(false);
    }

    @Test
    public void testInstrumentationResult() throws Exception {
        Description runDescription = Description.createSuiteDescription("run");
        mListener.testRunStarted(runDescription);

        Thread.sleep(TEST_DURATION);
        mListener.testRunFinished(new Result());
        // AJUR runner is then gonna call instrumentationRunFinished.
        Bundle result = new Bundle();
        mListener.instrumentationRunFinished(System.out, result, new Result());
        int expectedMin = Arrays.stream(TEST_METRIC_VALUES).min(Integer::compare).get();
        assertEquals(
                expectedMin,
                Integer.parseInt(
                        result.getString(
                                TEST_METRIC_KEY + ScheduledRunCollectionListener.MIN_SUFFIX)));
        int expectedMax = Arrays.stream(TEST_METRIC_VALUES).max(Integer::compare).get();
        assertEquals(
                expectedMax,
                Integer.parseInt(
                        result.getString(
                                TEST_METRIC_KEY + ScheduledRunCollectionListener.MAX_SUFFIX)));
        double expectedMean =
                Arrays.stream(TEST_METRIC_VALUES).mapToDouble(i -> i.doubleValue()).sum()
                        / NUMBER_OF_COLLECTIONS;
        assertEquals(
                expectedMean,
                Double.parseDouble(
                        result.getString(
                                TEST_METRIC_KEY + ScheduledRunCollectionListener.MEAN_SUFFIX)),
                0.1);
    }

    private void testRun(boolean isComplete) throws Exception {
        Description runDescription = Description.createSuiteDescription("run");
        mListener.testRunStarted(runDescription);

        Thread.sleep(TEST_DURATION);
        // If incomplete run happens, for example, when a system crash happens half way through the
        // run, <code>testRunFinished</code> method will be skipped, but the output file should
        // still be present, and the time-series up to the point when the crash happens should still
        // be recorded.
        if (isComplete) {
            mListener.testRunFinished(new Result());
        }

        ArgumentCaptor<Bundle> bundle = ArgumentCaptor.forClass(Bundle.class);

        // Verify that the path of the time-series output file has been sent to instrumentation.
        verify(mInstrumentation, atLeast(1))
                .sendStatus(eq(SendToInstrumentation.INST_STATUS_IN_PROGRESS), bundle.capture());
        Bundle pathBundle = bundle.getAllValues().get(0);
        String pathKey =
                String.format(
                        ScheduledRunCollectionListener.OUTPUT_FILE_PATH,
                        ScheduledRunCollectionListener.class.getSimpleName());
        String path = pathBundle.getString(pathKey);
        assertNotNull(path);

        // Check the output file exists.
        File outputFile = new File(path);
        assertTrue(outputFile.exists());

        // Check that output file contains some of the periodic run results, sample results are
        // like:
        //
        // time  ,metric_key       ,value
        // 2     ,test_metric_key  ,0
        // 102   ,test_metric_key  ,0
        // 203   ,test_metric_key  ,0
        // ...
        List<String> lines = Files.readAllLines(outputFile.toPath(), Charset.defaultCharset());
        assertEquals(NUMBER_OF_COLLECTIONS, lines.size() - 1);
        assertEquals(lines.get(0), ScheduledRunCollectionListener.TIME_SERIES_HEADER);
        for (int i = 1; i != lines.size(); ++i) {
            Pattern p = Pattern.compile(DATA_REGEX);
            Matcher m = p.matcher(lines.get(i));
            assertTrue(m.matches());
            long timestamp = Long.parseLong(m.group("timestamp"));
            long delta = TEST_INTERVAL / 2;
            assertEquals((i - 1) * TEST_INTERVAL, timestamp, delta);
            Integer value = Integer.valueOf(m.group("value"));
            assertEquals(TEST_METRIC_VALUES[i - 1], value);
        }

        // For incomplete run, invoke testRunFinished in the end to prevent resource leak.
        if (!isComplete) {
            mListener.testRunFinished(new Result());
        }
    }
}
