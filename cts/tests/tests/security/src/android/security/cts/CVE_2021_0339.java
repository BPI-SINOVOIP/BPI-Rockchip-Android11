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

import static org.junit.Assert.assertThat;
import static org.junit.Assume.assumeThat;
import static org.hamcrest.Matchers.*;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.RemoteCallback;
import android.os.SystemClock;
import android.platform.test.annotations.SecurityTest;
import android.test.AndroidTestCase;
import android.util.Log;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

@SecurityTest
@RunWith(AndroidJUnit4.class)
public class CVE_2021_0339 {

    static final String TAG = CVE_2021_0339.class.getSimpleName();
    private static final String SECURITY_CTS_PACKAGE_NAME = "android.security.cts";
    private static final String CALLBACK_KEY = "testactivitycallback";
    private static final String RESULT_KEY = "testactivityresult";
    static final int DURATION_RESULT_CODE = 1;
    static final int MAX_TRANSITION_DURATION_MS = 3000; // internal max
    static final int TIME_MEASUREMENT_DELAY_MS = 5000; // tolerance for lag.

    private void launchActivity(Class<? extends Activity> clazz, RemoteCallback cb) {
        final Context context = InstrumentationRegistry.getInstrumentation().getContext();
        final Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClassName(SECURITY_CTS_PACKAGE_NAME, clazz.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(CALLBACK_KEY, cb);
        context.startActivity(intent);
    }

    /**
     * b/175817167
     * start the first activity and get the result from the remote callback
     */
    @Test
    @SecurityTest
    public void testPocCVE_2021_0339() throws Exception {
        CompletableFuture<Integer> callbackReturn = new CompletableFuture<>();
        RemoteCallback cb = new RemoteCallback((Bundle result) ->
                callbackReturn.complete(result.getInt(RESULT_KEY)));
        launchActivity(FirstActivity.class, cb); // start activity with callback as intent extra

        // blocking while the remotecallback is unset
        int duration = callbackReturn.get(15, TimeUnit.SECONDS);

        // if we couldn't get the duration of secondactivity in firstactivity, the default is -1
        assumeThat(duration, not(equals(-1)));

        // the max duration after the fix is 3 seconds.
        // we check to see that the total duration was less than 8 seconds after accounting for lag
        assertThat(duration,
                lessThan(MAX_TRANSITION_DURATION_MS + TIME_MEASUREMENT_DELAY_MS));
    }

    /**
     * create an activity so that the second activity has something to animate from
     * start the second activity and get the result
     * return the result from the second activity in the remotecallback
     */
    public static class FirstActivity extends Activity {
        private RemoteCallback cb;

        @Override
        public void onCreate(Bundle bundle) {
            super.onCreate(bundle);
            cb = (RemoteCallback) getIntent().getExtras().get(CALLBACK_KEY);
        }

        @Override
        public void onEnterAnimationComplete() {
            super.onEnterAnimationComplete();
            Intent intent = new Intent(this, SecondActivity.class);
            intent.putExtra("STARTED_TIMESTAMP", SystemClock.uptimeMillis());
            startActivityForResult(intent, DURATION_RESULT_CODE);
            overridePendingTransition(R.anim.translate2,R.anim.translate1);
            Log.d(TAG,this.getLocalClassName()+" onEnterAnimationComplete()");
        }

        @Override
        protected void onActivityResult(int requestCode,int resultCode, Intent data) {
            super.onActivityResult(requestCode, resultCode, data);
            if (requestCode == DURATION_RESULT_CODE && resultCode == RESULT_OK) {
                // this is the result that we requested
                int duration = data.getIntExtra("duration", -1); // get result from secondactivity
                Bundle res = new Bundle();
                res.putInt(RESULT_KEY, duration);
                finish();
                cb.sendResult(res); // update callback in test
            }
        }
    }

    /**
     * measure time since secondactivity start to secondactivity animation complete
     * return the duration in the result
     */
    public static class SecondActivity extends Activity{
        @Override
        public void onEnterAnimationComplete() {
            super.onEnterAnimationComplete();
            long completedTs = SystemClock.uptimeMillis();
            long startedTs = getIntent().getLongExtra("STARTED_TIMESTAMP", 0);
            int duration = (int)(completedTs - startedTs);
            Log.d(TAG, this.getLocalClassName()
                      + " onEnterAnimationComplete() duration=" + Long.toString(duration));
            Intent durationIntent = new Intent();
            durationIntent.putExtra("duration", duration);
            setResult(RESULT_OK, durationIntent); // set result for firstactivity
            finish(); // firstactivity only gets the result when we finish
        }
    }
}
