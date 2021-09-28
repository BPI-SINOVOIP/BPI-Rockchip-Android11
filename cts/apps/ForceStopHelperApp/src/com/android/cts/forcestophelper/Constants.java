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
 *
 */

package com.android.cts.forcestophelper;

public interface Constants {
    String PACKAGE_NAME = "com.android.cts.forcestophelper";
    String ACTIVITY_CLASS_NAME = "com.android.cts.forcestophelper.RecentTaskActivity";
    String ACTION_ALARM = PACKAGE_NAME + ".action.ACTION_ALARM";
    String EXTRA_ON_TASK_REMOVED = PACKAGE_NAME + ".extra.ON_TASK_REMOVED";
    String EXTRA_ON_ALARM = PACKAGE_NAME + ".extra.ON_ALARM";
    long ALARM_DELAY = 5_000;
}
