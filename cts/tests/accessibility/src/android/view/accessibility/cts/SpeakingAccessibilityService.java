/*
 * Copyright (C) 2012 The Android Open Source Project
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

package android.view.accessibility.cts;

import android.accessibility.cts.common.InstrumentedAccessibilityService;
import android.content.ComponentName;

/**
 * Stub accessibility service that reports itself as providing spoken feedback.
 */
public class SpeakingAccessibilityService extends InstrumentedAccessibilityService {
    public static final ComponentName COMPONENT_NAME = new ComponentName(
            "android.view.accessibility.cts",
            "android.view.accessibility.cts.SpeakingAccessibilityService");
}
