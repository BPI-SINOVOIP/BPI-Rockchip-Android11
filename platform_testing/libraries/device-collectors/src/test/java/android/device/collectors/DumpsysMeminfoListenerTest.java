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
 * limitations under the License
 */

package android.device.collectors;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Instrumentation;
import android.os.Bundle;

import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.DumpsysMeminfoHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.mockito.Mock;

/**
 * Android Unit tests for {@link DumpsysMeminfoListener}.
 *
 * <p>To run: atest CollectorDeviceLibTest:android.device.collectors.DumpsysMeminfoListenerTest
 */
@RunWith(AndroidJUnit4.class)
public class DumpsysMeminfoListenerTest {

    @Mock private DumpsysMeminfoHelper mDumpsysMeminfoHelper;
    @Mock private Instrumentation mInstrumentation;

    private Description mRunDesc;

    @Before
    public void setup() {
        initMocks(this);
        mRunDesc = Description.createSuiteDescription("run");
    }

    @Test
    public void testListener_noProcessNames() throws Exception {
        DumpsysMeminfoListener listener = initListener(new Bundle(), mDumpsysMeminfoHelper);
        listener.testRunStarted(mRunDesc);
        verify(mDumpsysMeminfoHelper, never()).setUp(any());
    }

    @Test
    public void testListener_withProcessNames() throws Exception {
        Bundle bundle = new Bundle();
        bundle.putString(
                DumpsysMeminfoListener.PROCESS_NAMES_KEY,
                String.join(DumpsysMeminfoListener.PROCESS_SEPARATOR, "process1", "process2"));
        DumpsysMeminfoListener listener = initListener(bundle, mDumpsysMeminfoHelper);
        listener.testRunStarted(mRunDesc);
        verify(mDumpsysMeminfoHelper).setUp("process1", "process2");
    }

    private DumpsysMeminfoListener initListener(Bundle bundle, DumpsysMeminfoHelper helper) {
        DumpsysMeminfoListener listener = new DumpsysMeminfoListener(bundle, helper);
        listener.setInstrumentation(mInstrumentation);
        return listener;
    }
}
