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

package android.platform.test.scenario.sleep;

import android.platform.test.microbenchmark.Microbenchmark;
import android.platform.test.rule.GarbageCollectRule;

import org.junit.Rule;
import org.junit.runner.RunWith;

/** Idle memory microbenchmark for system server. */
@RunWith(Microbenchmark.class)
public class IdleSystemServerMemoryMicrobenchmark extends IdleMemoryMicrobenchmarkBase {
    // Method-level rules
    @Rule public GarbageCollectRule gcRule = new GarbageCollectRule("system_server");
}
