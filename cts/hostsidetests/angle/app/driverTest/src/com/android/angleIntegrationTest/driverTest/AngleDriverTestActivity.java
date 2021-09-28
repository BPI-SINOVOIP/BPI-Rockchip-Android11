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

package com.android.angleIntegrationTest.driverTest;

import static org.junit.Assert.fail;

import androidx.test.runner.AndroidJUnit4;

import com.android.angleIntegrationTest.common.GlesView;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class AngleDriverTestActivity {

    private final String TAG = this.getClass().getSimpleName();

    private void validateDeveloperOption(boolean angleEnabled) throws Exception {
        GlesView glesView = new GlesView();

        if (!glesView.validateDeveloperOption(angleEnabled)) {
            if (angleEnabled) {
                String renderer = glesView.getRenderer();
                fail("Failure - ANGLE was not loaded: '" + renderer + "'");
            } else {
                String renderer = glesView.getRenderer();
                fail("Failure - ANGLE was loaded: '" + renderer + "'");
            }
        }
    }

    @Test
    public void testUseDefaultDriver() throws Exception {
        // The rules file does not enable ANGLE for this app
        validateDeveloperOption(false);
    }

    @Test
    public void testUseAngleDriver() throws Exception {
        validateDeveloperOption(true);
    }

    @Test
    public void testUseNativeDriver() throws Exception {
        validateDeveloperOption(false);
    }
}
