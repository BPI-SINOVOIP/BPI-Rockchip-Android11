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

package android.security.cts;

import android.app.ActivityManager;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.platform.test.annotations.SecurityTest;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.*;
import static org.junit.Assume.*;

@SecurityTest
@RunWith(AndroidJUnit4.class)
public class CVE_2020_0294 {
    private static final String TAG = "CVE_2020_0294";

    /**
     * b/170661753
     */
    @Test
    @SecurityTest(minPatchLevel = "2020-12")
    public void testPocCVE_2020_0294() throws Exception {
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        ActivityManager activityManager = (ActivityManager) instrumentation.getContext()
                .getSystemService(Context.ACTIVITY_SERVICE);
        Context targetContext = instrumentation.getTargetContext();
        ComponentName componentName =
                ComponentName.unflattenFromString("com.android.systemui/.ImageWallpaper");
        PendingIntent pi = activityManager.getRunningServiceControlPanel(componentName);
        assumeNotNull(pi);

        Intent spuriousIntent = new Intent();
        spuriousIntent.setPackage(targetContext.getPackageName());
        spuriousIntent.setDataAndType(
                Uri.parse("content://com.android.settings.files/my_cache/NOTICE.html"), "txt/html");
        spuriousIntent.setFlags(
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try {
            pi.send(targetContext, 0, spuriousIntent, null, null);
        } catch (PendingIntent.CanceledException e) {
            //This means PendingIntent has failed as its mutable
            fail("PendingIntent from WallpaperManagerService is mutable");
        }
    }
}
