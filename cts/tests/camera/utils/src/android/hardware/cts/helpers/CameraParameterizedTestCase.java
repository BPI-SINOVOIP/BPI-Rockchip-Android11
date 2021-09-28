/*
 * Copyright 2019 The Android Open Source Project
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

package android.hardware.cts.helpers;

import android.app.UiAutomation;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import android.app.Activity;
import androidx.test.rule.ActivityTestRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.util.ArrayList;
import java.util.List;

public class CameraParameterizedTestCase {
    protected UiAutomation mUiAutomation;
    protected Context mContext;
    @Parameter(0)
    public boolean mAdoptShellPerm;

    private static final String CAMERA_ID_INSTR_ARG_KEY = "camera-id";
    private static final String CAMERA_PERF_MEASURE = "perf-measure";
    private static final Bundle mBundle = InstrumentationRegistry.getArguments();
    protected static final String mOverrideCameraId = mBundle.getString(CAMERA_ID_INSTR_ARG_KEY);
    protected static final String mPerfMeasure = mBundle.getString(CAMERA_PERF_MEASURE);

    public boolean isPerfMeasure() {
        return mPerfMeasure != null && mPerfMeasure.equals("on");
    }

    @Parameters
    public static Iterable<? extends Object> data() {
        List<Boolean> adoptShellPerm = new ArrayList<Boolean>();
        // Only add adoptShellPerm(true) of camera id is not overridden.
        if (mPerfMeasure == null || !mPerfMeasure.equals("on")) {
            adoptShellPerm.add(true);
        }
        adoptShellPerm.add(false);
        return adoptShellPerm;
    }

    @Before
    public void setUp() throws Exception {
        mUiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        mContext = InstrumentationRegistry.getTargetContext();
        if (mAdoptShellPerm) {
            mUiAutomation.adoptShellPermissionIdentity();
        }
    }

    @After
    public void tearDown() throws Exception {
        if (mAdoptShellPerm) {
            mUiAutomation.dropShellPermissionIdentity();
        }
    }
}
