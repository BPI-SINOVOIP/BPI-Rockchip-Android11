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

package android.platform.test.scenario.profile;

import android.platform.test.longevity.ProfileSuite;
import android.platform.test.rule.NaturalOrientationRule;

import org.junit.ClassRule;
import org.junit.runner.RunWith;
import org.junit.runners.Suite.SuiteClasses;

@RunWith(ProfileSuite.class)
@SuiteClasses({
    // Referenced package names to avoid overlap.
    android.platform.test.scenario.generic.OpenApp.class,
    android.platform.test.scenario.sleep.Idle.class,
})
public class CommonUserProfileTest {
    // Class-level rules
    @ClassRule public static NaturalOrientationRule orientationRule = new NaturalOrientationRule();
}
