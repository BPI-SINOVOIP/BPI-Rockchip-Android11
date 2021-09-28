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

package android.hardware.input.cts.tests;

import android.hardware.cts.R;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class MicrosoftXboxOneSTest extends InputTestCase {

    // Exercises the Bluetooth behavior of the Xbox One S controller
    public MicrosoftXboxOneSTest() {
        super(R.raw.microsoft_xboxones_register);
    }

    @Test
    public void testAllKeys() {
        testInputEvents(R.raw.microsoft_xboxones_keyeventtests);
    }

    @Test
    public void testAllMotions() {
        testInputEvents(R.raw.microsoft_xboxones_motioneventtests);
    }
}
