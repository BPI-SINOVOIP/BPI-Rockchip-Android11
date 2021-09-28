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
package android.icu.extratest.util;

import android.icu.dev.test.TestFmwk;
import android.icu.testsharding.MainTestShard;
import android.icu.util.JapaneseCalendar;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.Test;

@MainTestShard
@RunWith(JUnit4.class)
public class JapaneseCalendarTest extends TestFmwk {

    /**
     * On Android Q+ device, the current era should be Reiwa or a new era in the future.
     */
    @Test
    public void testCurrentEra_Reiwa() {
        if (JapaneseCalendar.CURRENT_ERA < 236 /* era value of Reiwa */ ) {
            errln("JapaneseCalendar.CURRENT_ERA should have a value of 236 (Reiwa) or greater");
        }
    }
}
