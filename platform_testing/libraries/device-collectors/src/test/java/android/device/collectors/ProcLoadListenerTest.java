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

import static android.device.collectors.ProcLoadListener.PROC_LOAD_THRESHOLD;
import static android.device.collectors.ProcLoadListener.PROC_THRESHOLD_TIMEOUT;
import static android.device.collectors.ProcLoadListener.PROC_LOAD_INTERVAL;


import static org.mockito.Mockito.verify;

import android.app.Instrumentation;
import android.os.Bundle;

import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.ProcLoadHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Android Unit tests for {@link ProcLoadListener}.
 *
 * To run:
 * atest CollectorDeviceLibTest:android.device.collectors.ProcLoadListenerTest
 */
@RunWith(AndroidJUnit4.class)
public class ProcLoadListenerTest {

    @Mock
    private Instrumentation mInstrumentation;
    @Mock
    private ProcLoadHelper mProcLoadHelper;

    private ProcLoadListener mListener;
    private Description mRunDesc;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mRunDesc = Description.createSuiteDescription("run");
    }

    private ProcLoadListener initListener(Bundle b) {
        ProcLoadListener listener = new ProcLoadListener(b, mProcLoadHelper);
        listener.setInstrumentation(mInstrumentation);
        return listener;
    }

    @Test
    public void testLoadProcAdditionalOptions() throws Exception {
        Bundle b = new Bundle();
        b.putString(PROC_LOAD_THRESHOLD, "1");
        b.putString(PROC_THRESHOLD_TIMEOUT, "3");
        b.putString(PROC_LOAD_INTERVAL, "2");
        mListener = initListener(b);
        mListener.testRunStarted(mRunDesc);

        verify(mProcLoadHelper).setProcLoadThreshold(1);
        verify(mProcLoadHelper).setProcLoadWaitTimeInMs(3L);
        verify(mProcLoadHelper).setProcLoadIntervalInMs(2L);
    }
}
