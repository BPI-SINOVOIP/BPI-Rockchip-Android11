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

import static android.device.collectors.ProcessShowmapListener.PROCESS_NAMES_KEY;
import static android.device.collectors.ProcessShowmapListener.PROCESS_SEPARATOR;

import static org.mockito.Mockito.verify;

import android.app.Instrumentation;
import android.os.Bundle;

import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.ProcessShowmapHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Android Unit tests for {@link ProcessShowmapListener}.
 *
 * To run:
 * atest CollectorDeviceLibTest:android.device.collectors.ProcessShowmapListenerTest
 */
@RunWith(AndroidJUnit4.class)
public class ProcessShowmapListenerTest {

    @Mock
    private Instrumentation mInstrumentation;
    @Mock
    private ProcessShowmapHelper mShowmapHelper;

    private ProcessShowmapListener mListener;
    private Description mRunDesc;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mRunDesc = Description.createSuiteDescription("run");
    }

    private ProcessShowmapListener initListener(Bundle b) {
        ProcessShowmapListener listener = new ProcessShowmapListener(b, mShowmapHelper);
        listener.setInstrumentation(mInstrumentation);
        return listener;
    }

    @Test
    public void testHelperReceivesProcessNames() throws Exception {
        Bundle b = new Bundle();
        b.putString(PROCESS_NAMES_KEY, "process1" + PROCESS_SEPARATOR + "process2");
        mListener = initListener(b);

        mListener.testRunStarted(mRunDesc);

        verify(mShowmapHelper).setUp("process1", "process2");
    }
}
