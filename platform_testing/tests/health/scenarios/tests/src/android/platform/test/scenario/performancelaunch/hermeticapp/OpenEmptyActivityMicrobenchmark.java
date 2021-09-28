/*
 * Copyright (C) 2020 The Android Open Source Project
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
package android.platform.test.scenario.performancelaunch.hermeticapp;

import android.platform.test.microbenchmark.Microbenchmark;
import android.platform.test.rule.CompilationFilterRule;
import android.platform.test.rule.DropCachesRule;
import android.platform.test.rule.KillAppsRule;
import android.platform.test.rule.PressHomeRule;

import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

@RunWith(Microbenchmark.class)
public class OpenEmptyActivityMicrobenchmark extends OpenEmptyActivity {
    private static final String PACKAGE = "com.android.performanceLaunch";

    // Method-level rules
    @Rule
    public RuleChain rules =
            RuleChain.outerRule(new KillAppsRule(PACKAGE))
                    .around(new DropCachesRule())
                    .around(new CompilationFilterRule(PACKAGE))
                    .around(new PressHomeRule());
}
