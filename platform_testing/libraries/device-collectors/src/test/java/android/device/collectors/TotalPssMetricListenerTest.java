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

import static android.device.collectors.TotalPssMetricListener.PROCESS_NAMES_KEY;
import static android.device.collectors.TotalPssMetricListener.PROCESS_SEPARATOR;
import static android.device.collectors.TotalPssMetricListener.MIN_ITERATIONS_KEY;
import static android.device.collectors.TotalPssMetricListener.MAX_ITERATIONS_KEY;
import static android.device.collectors.TotalPssMetricListener.SLEEP_TIME_KEY;
import static android.device.collectors.TotalPssMetricListener.THRESHOLD_KEY;

import static org.mockito.Mockito.verify;

import android.app.Instrumentation;
import android.os.Bundle;

import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.TotalPssHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Android Unit tests for {@link TotalPssMetricListener}.
 *
 * To run:
 * atest CollectorDeviceLibTest:android.device.collectors.TotalPssMetricListenerTest
 */
@RunWith(AndroidJUnit4.class)
public class TotalPssMetricListenerTest {

    @Mock
    private Instrumentation mInstrumentation;
    @Mock
    private TotalPssHelper mTotalPssMetricHelper;

    private TotalPssMetricListener mListener;
    private Description mRunDesc;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mRunDesc = Description.createSuiteDescription("run");
    }

    private TotalPssMetricListener initListener(Bundle b) {
        TotalPssMetricListener listener = new TotalPssMetricListener(b, mTotalPssMetricHelper);
        listener.setInstrumentation(mInstrumentation);
        return listener;
    }

    @Test
    public void testHelperReceivesProcessNames() throws Exception {
        Bundle b = new Bundle();
        b.putString(PROCESS_NAMES_KEY, "process1" + PROCESS_SEPARATOR + "process2");
        mListener = initListener(b);

        mListener.testRunStarted(mRunDesc);

        verify(mTotalPssMetricHelper).setUp("process1", "process2");
    }

    @Test
    public void testAdditionalPssOptions() throws Exception {
        Bundle b = new Bundle();
        b.putString(PROCESS_NAMES_KEY, "process1");
        b.putString(MIN_ITERATIONS_KEY, "50");
        b.putString(MAX_ITERATIONS_KEY, "102");
        b.putString(SLEEP_TIME_KEY, "2000");
        b.putString(THRESHOLD_KEY, "2048");
        mListener = initListener(b);

        mListener.testRunStarted(mRunDesc);

        verify(mTotalPssMetricHelper).setUp("process1");
        verify(mTotalPssMetricHelper).setMinIterations(50);
        verify(mTotalPssMetricHelper).setMaxIterations(102);
        verify(mTotalPssMetricHelper).setSleepTime(2000);
        verify(mTotalPssMetricHelper).setThreshold(2048);
    }
}
