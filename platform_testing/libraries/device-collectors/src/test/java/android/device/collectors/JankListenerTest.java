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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Instrumentation;
import android.os.Bundle;
import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.JankCollectionHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/** Unit tests for {@link JankListener} specific behavior. */
@RunWith(AndroidJUnit4.class)
public final class JankListenerTest {

    // A {@code Description} to pass when faking a test run start call.
    private static final Description RUN_DESCRIPTION = Description.createSuiteDescription("run");
    private static final Description TEST_DESCRIPTION =
            Description.createTestDescription("run", "test");

    @Mock private JankCollectionHelper mHelper;
    @Mock private Instrumentation mInstrumentation;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
    }

    /** Test that packages are specified when set in arguments. */
    @Test
    public void testCollect_specificProcess() throws Exception {
        Bundle twoProcBundle = new Bundle();
        twoProcBundle.putString(
                JankListener.PACKAGE_NAMES_KEY,
                String.join(JankListener.PACKAGE_SEPARATOR, "pkg1", "pkg2"));
        JankListener collector = new JankListener(twoProcBundle, mHelper);
        collector.setInstrumentation(mInstrumentation);

        // Simulate a test run and verify the "specific process collection" behavior.
        collector.testRunStarted(RUN_DESCRIPTION);
        verify(mHelper, times(1)).addTrackedPackages("pkg1", "pkg2");
        collector.testStarted(TEST_DESCRIPTION);
        collector.testFinished(TEST_DESCRIPTION);
        collector.testRunFinished(new Result());
    }

    /** Test that no packages are specified when not set in arguments. */
    @Test
    public void testCollect_allProcesses() throws Exception {
        JankListener collector = new JankListener(new Bundle(), mHelper);
        collector.setInstrumentation(mInstrumentation);

        // Simulate a test run and verify the "all process collection" behavior.
        collector.testRunStarted(RUN_DESCRIPTION);
        verify(mHelper, never()).addTrackedPackages(anyString());
        collector.testStarted(TEST_DESCRIPTION);
        collector.testFinished(TEST_DESCRIPTION);
        collector.testRunFinished(new Result());
    }
}
