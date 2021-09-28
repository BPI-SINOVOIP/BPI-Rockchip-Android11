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

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Instrumentation;
import android.os.Bundle;
import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.BinderCollectionHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/** Unit tests for {@link BinderListener} specific behavior. */
@RunWith(AndroidJUnit4.class)
public final class BinderListenerTest {

    /** A {@code Description} to pass when faking a test run start call. */
    private static final Description RUN_DESCRIPTION = Description.createSuiteDescription("run");

    private static final Description TEST_DESCRIPTION =
            Description.createTestDescription("run", "test");

    @Mock private BinderCollectionHelper mHelper;
    @Mock private Instrumentation mInstrumentation;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
    }

    /** Test only one specific process is set in arguments. */
    @Test
    public void testCollect_singleSpecificProcess() throws Exception {
        Bundle singleProcessBundle = new Bundle();
        singleProcessBundle.putString(
                BinderListener.PROCESS_NAMES_KEY,
                String.join(BinderListener.PROCESS_SEPARATOR, "com.android.systemui"));
        BinderListener collector = new BinderListener(singleProcessBundle, mHelper);
        collector.setInstrumentation(mInstrumentation);

        /** Simulate a test run and verify the "specific process collection" behavior. */
        collector.testRunStarted(RUN_DESCRIPTION);
        verify(mHelper).addTrackedProcesses("com.android.systemui");
        collector.testStarted(TEST_DESCRIPTION);
        collector.testFinished(TEST_DESCRIPTION);
        collector.testRunFinished(new Result());
    }

    /** Test only multiple specific process is set in arguments. */
    @Test
    public void testCollect_multiSpecificProcess() throws Exception {
        Bundle multiProcessBundle = new Bundle();
        multiProcessBundle.putString(
                BinderListener.PROCESS_NAMES_KEY,
                String.join(BinderListener.PROCESS_SEPARATOR, "com.android.systemui", "system"));
        BinderListener collector = new BinderListener(multiProcessBundle, mHelper);
        collector.setInstrumentation(mInstrumentation);

        /** Simulate a test run and verify the "multiple specific processes collection" behavior. */
        collector.testRunStarted(RUN_DESCRIPTION);
        verify(mHelper).addTrackedProcesses("com.android.systemui", "system");
        collector.testStarted(TEST_DESCRIPTION);
        collector.testFinished(TEST_DESCRIPTION);
        collector.testRunFinished(new Result());
    }

    /** Test no specific process is set in arguments. */
    @Test
    public void testCollect_noSpecificProcess() throws Exception {
        BinderListener collector = new BinderListener(new Bundle(), mHelper);
        collector.setInstrumentation(mInstrumentation);

        /** Simulate a test run and verify the "no specific process collection" behavior. */
        collector.testRunStarted(RUN_DESCRIPTION);
        verify(mHelper, never()).addTrackedProcesses(anyString());
        collector.testStarted(TEST_DESCRIPTION);
        collector.testFinished(TEST_DESCRIPTION);
        collector.testRunFinished(new Result());
    }
}
