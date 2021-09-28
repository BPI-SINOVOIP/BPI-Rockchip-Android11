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

package com.android.server.wm.flicker;

import static com.android.server.wm.flicker.CommonTransitions.editTextLoseFocusToHome;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.FlakyTest;
import androidx.test.filters.LargeTest;

import com.android.server.wm.flicker.helpers.ImeAppAutoFocusHelper;

import org.junit.Before;
import org.junit.FixMethodOrder;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;
import org.junit.runners.Parameterized;

/**
 * Test IME window closing back to app window transitions.
 * To run this test: {@code atest FlickerTests:CloseImeWindowToAppTest}
 */
@LargeTest
@RunWith(Parameterized.class)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class CloseImeAutoOpenWindowToHomeTest extends CloseImeWindowToHomeTest {

    public CloseImeAutoOpenWindowToHomeTest(String beginRotationName, int beginRotation) {
        super(beginRotationName, beginRotation);

        mTestApp = new ImeAppAutoFocusHelper(InstrumentationRegistry.getInstrumentation());
    }

    @Before
    public void runTransition() {
        run(editTextLoseFocusToHome((ImeAppAutoFocusHelper) mTestApp, mUiDevice, mBeginRotation)
                .includeJankyRuns().build());
    }

    @FlakyTest(bugId = 141458352)
    @Ignore("Waiting bug feedback")
    @Test
    public void checkVisibility_imeWindowBecomesInvisible() {
        super.checkVisibility_imeWindowBecomesInvisible();
    }

    @FlakyTest(bugId = 141458352)
    @Ignore("Waiting bug feedback")
    @Test
    public void checkVisibility_imeLayerBecomesInvisible() {
        super.checkVisibility_imeLayerBecomesInvisible();
    }
}
