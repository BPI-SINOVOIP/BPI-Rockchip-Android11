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

package android.platform.test.scenario.sleep;

import android.os.SystemClock;
import android.platform.test.option.LongOption;
import android.platform.test.rule.NaturalOrientationRule;
import android.platform.test.scenario.annotation.Scenario;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Performs no actions for a specified amount of time.
 */
@Scenario
@RunWith(JUnit4.class)
public class Idle {
    // Class-level rules
    @ClassRule public static NaturalOrientationRule orientationRule = new NaturalOrientationRule();

    @Rule public final LongOption mDurationMs = new LongOption("durationMs").setDefault(1000L);

    @Test
    public void testDoingNothing() {
        SystemClock.sleep(mDurationMs.get());
    }
}
