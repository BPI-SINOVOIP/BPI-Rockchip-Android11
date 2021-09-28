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
package android.device.collectors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.matches;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import android.app.Instrumentation;
import android.os.Bundle;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.io.File;

/**
 * Android Unit tests for {@link IncidentReportListener}.
 *
 * To run:
 * atest CollectorDeviceLibTest:android.device.collectors.IncidentReportListenerTest
 */
@RunWith(AndroidJUnit4.class)
public class IncidentReportListenerTest {
    // A {@code File} representing the fixed-location report directory.
    private static final File REPORT_DEST = IncidentReportListener.REPORT_DIRECTORY.toFile();
    // A {@code File} representing the fixed-location full report.
    private static final File REPORT_FILE = IncidentReportListener.FINAL_REPORT_PATH.toFile();
    // A {@code Description} to pass when faking a test run start call.
    private static final Description FAKE_DESCRIPTION = Description.createSuiteDescription("run");
    // A non-empty array that represents an empty incident report.
    private static final byte[] EMPTY_BYTE_ARRAY = new byte[0];
    // A non-empty array that represents a valid incident report.
    private static final byte[] NONEMPTY_BYTE_ARRAY = "full".getBytes();

    private IncidentReportListener mListener;

    @Mock
    private Instrumentation mInstrumentation;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mListener = spy(new IncidentReportListener());
        mListener.setInstrumentation(mInstrumentation);
    }

    @After
    public void tearDown() {
        REPORT_FILE.delete();
        REPORT_DEST.delete();
    }

    /** Tests the collector rewrites the directory at the start of a test run. */
    @Test
    public void testCreateDirectoryAtStart() throws Exception {
        doReturn(NONEMPTY_BYTE_ARRAY).when(mListener)
                .executeCommandBlocking(matches("rm -rf .*"));
        assertFalse(REPORT_DEST.exists());
        assertFalse(REPORT_FILE.exists());
        mListener.testRunStarted(FAKE_DESCRIPTION);
        assertTrue(REPORT_DEST.exists());
    }

    /** Tests the collector writes a new full report at the end of a test run. */
    @Test
    public void testCollectFullReportAtEnd() throws Exception {
        doReturn(NONEMPTY_BYTE_ARRAY)
                .when(mListener)
                .executeCommandBlocking(matches("incident -b -p .*"));
        doReturn(NONEMPTY_BYTE_ARRAY).when(mListener)
                .executeCommandBlocking(matches("rm -rf .*"));
        mListener.testRunStarted(FAKE_DESCRIPTION);
        assertFalse(REPORT_FILE.exists());
        mListener.testRunFinished(new Result());
        assertTrue(REPORT_FILE.exists());
        assertTrue(REPORT_FILE.isFile());
        assertNotEquals(REPORT_FILE.getTotalSpace(), 0);
    }

    /** Tests the collector throws if the output directory is not created or does not exist. */
    @Test
    public void testFailCollectIfNoDestinationDirectory() throws Exception {
        doReturn(NONEMPTY_BYTE_ARRAY)
                .when(mListener)
                .executeCommandBlocking(matches("incident -b -p .*"));
        doReturn(null).when(mListener).createAndEmptyDirectory(anyString());
        // Ensure the test run start throws exception failing to create the folder.
        try {
            mListener.onTestRunStart(mListener.createDataRecord(), FAKE_DESCRIPTION);
            fail("Exception should throw failing to create destination.");
        } catch (RuntimeException e) {
            assertTrue(e.getMessage().contains("Couldn't create"));
            assertTrue(e.getMessage().contains("destination folder"));
        }
        assertFalse(REPORT_DEST.exists());
        // Ensure the test run end throws exception without the available folder.
        try {
            mListener.onTestRunEnd(mListener.createDataRecord(), new Result());
            fail("Exception should throw for missing directory.");
        } catch (RuntimeException e) {
            assertTrue(e.getMessage().contains("destination folder"));
            assertTrue(e.getMessage().contains("does not exist"));
        }
    }

    /** Tests the collector fails to write a new full report if the report data is empty. */
    @Test
    public void testCollectEmptyReportFails() throws Exception {
        doReturn(EMPTY_BYTE_ARRAY)
                .when(mListener)
                .executeCommandBlocking(matches("incident -b -p .*"));
        doReturn(NONEMPTY_BYTE_ARRAY).when(mListener)
                .executeCommandBlocking(matches("rm -rf .*"));
        mListener.testRunStarted(FAKE_DESCRIPTION);
        try {
            mListener.onTestRunEnd(mListener.createDataRecord(), new Result());
            fail("Exception should throw for empty results.");
        } catch (RuntimeException e) {
            assertTrue(e.getMessage().contains("no data"));
        }
    }

    /** Tests the collector includes the full report in the {@code DataRecord}. */
    @Test
    public void testReportMetricsAsData() throws Exception {
        doReturn(NONEMPTY_BYTE_ARRAY)
                .when(mListener)
                .executeCommandBlocking(matches("incident -b -p .*"));
        doReturn(NONEMPTY_BYTE_ARRAY).when(mListener)
                .executeCommandBlocking(matches("rm -rf .*"));
        mListener.testRunStarted(FAKE_DESCRIPTION);
        DataRecord record = mListener.createDataRecord();
        mListener.onTestRunEnd(record, new Result());
        assertTrue(record.hasMetrics());
        Bundle metrics = record.createBundleFromMetrics();
        assertTrue(metrics.containsKey(IncidentReportListener.FINAL_REPORT_KEY));
        assertEquals(metrics.getString(IncidentReportListener.FINAL_REPORT_KEY),
                REPORT_FILE.getAbsolutePath());
    }
}
