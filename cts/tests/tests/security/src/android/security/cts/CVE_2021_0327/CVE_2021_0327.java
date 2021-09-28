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

package android.security.cts.CVE_2021_0327;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.platform.test.annotations.SecurityTest;
import android.test.AndroidTestCase;
import android.util.Log;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.InstrumentationRegistry;
import org.junit.Test;
import org.junit.runner.RunWith;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

@SecurityTest
@RunWith(AndroidJUnit4.class)
public class CVE_2021_0327 {

    private static final String SECURITY_CTS_PACKAGE_NAME = "android.security.cts";
    private static final String TAG = "CVE_2021_0327";
    public static boolean testActivityRequested;
    public static boolean testActivityCreated;
    public static String errorMessage = null;

    private void launchActivity(Class<? extends Activity> clazz) {
        final Context context = InstrumentationRegistry.getInstrumentation().getContext();
        final Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClassName(SECURITY_CTS_PACKAGE_NAME, clazz.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    /**
     * b/175817081
     */
    @Test
    @SecurityTest
    public void testPocCVE_2021_0327() throws Exception {
        Log.d(TAG, "test start");
        testActivityCreated=false;
        testActivityRequested=false;
        launchActivity(IntroActivity.class);
        synchronized(CVE_2021_0327.class){
          if(!testActivityRequested) {
            Log.d(TAG, "test is waiting for TestActivity to be requested to run");
            CVE_2021_0327.class.wait();
            Log.d(TAG, "got signal");
          }
        }
        synchronized(CVE_2021_0327.class){
          if(!testActivityCreated) {
            Log.d(TAG, "test is waiting for TestActivity to run");
            CVE_2021_0327.class.wait(10000);
            Log.d(TAG, "got signal");
          }
        }
        Log.d(TAG, "test completed. testActivityCreated=" + testActivityCreated);
        if(errorMessage != null){
          Log.d(TAG, "errorMessage=" + errorMessage);
        }
        assertTrue(errorMessage==null);
        assertFalse(testActivityCreated);
    }
}
