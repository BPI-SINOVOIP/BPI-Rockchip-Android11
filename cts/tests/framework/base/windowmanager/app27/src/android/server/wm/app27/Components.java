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

package android.server.wm.app27;

import static android.server.wm.app.Components.LAUNCHING_ACTIVITY;
import static android.server.wm.app.Components.TEST_ACTIVITY;

import android.content.ComponentName;
import android.server.wm.component.ComponentsBase;

public class Components extends ComponentsBase {

    public static final ComponentName SDK_27_LAUNCHING_ACTIVITY =
            component(Components.class, LAUNCHING_ACTIVITY.getClassName());

    public static final ComponentName SDK_27_TEST_ACTIVITY =
            component(Components.class, TEST_ACTIVITY.getClassName());

    public static final ComponentName SDK_27_SEPARATE_PROCESS_ACTIVITY =
            component(Components.class, "SeparateProcessActivity");

    public static final ComponentName SDK_27_LAUNCH_ENTER_PIP_ACTIVITY =
            component(Components.class, "LaunchEnterPipActivity");

    public static final ComponentName SDK_27_PIP_ACTIVITY =
            component(Components.class, "PipActivity");

    public static final ComponentName SDK_27_HOME_ACTIVITY =
            component(Components.class, "HomeActivity");
}
