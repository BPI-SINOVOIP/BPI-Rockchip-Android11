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

package android.server.wm.intent;

import android.app.Activity;

/**
 * A collection of activities with various launch modes used in the intent tests.
 * Each activity is named after the LaunchMode it has enabled.
 *
 * They reflect the activities present in the IntentPlayground app. The IntentPlayground app
 * resides in development/samples/IntentPlayground
 */
public class Activities {

    public static class TrackerActivity extends Activity {
    }

    public static class RegularActivity extends Activity {
    }

    public static class SingleTopActivity extends Activity {
    }

    public static class SingleInstanceActivity extends Activity {
    }

    public static class SingleInstanceActivity2 extends Activity {
    }

    public static class SingleTaskActivity extends Activity {
    }

    public static class SingleTaskActivity2 extends Activity {
    }

    public static class TaskAffinity1Activity extends Activity {
    }

    public static class TaskAffinity1Activity2 extends Activity {
    }

    public static class TaskAffinity2Activity extends Activity {
    }

    public static class TaskAffinity3Activity extends Activity {
    }

    public static class ClearTaskOnLaunchActivity extends Activity {
    }

    public static class DocumentLaunchIntoActivity extends Activity {
    }

    public static class DocumentLaunchAlwaysActivity extends Activity {
    }

    public static class DocumentLaunchNeverActivity extends Activity {
    }

    public static class NoHistoryActivity extends Activity {
    }

    public static class LauncherActivity extends Activity {
    }

    public static class RelinquishTaskIdentityActivity extends Activity {
    }

    public static class TaskAffinity1RelinquishTaskIdentityActivity extends Activity {
    }
}
