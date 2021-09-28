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

package android.gwpasan;

import android.app.Activity;
import android.content.Intent;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import java.lang.Override;

import android.gwpasan.Utils;

public class GwpAsanTestActivity extends Activity {
    static final String TAG = "GwpAsanTestActivity";

    private int mResult;
    private final Object mFinishEvent = new Object();

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        mResult = resultCode;
        synchronized (mFinishEvent) {
            mFinishEvent.notify();
        }
    }

    protected boolean callActivity(Class<?> cls) throws Exception {
        Thread thread = new Thread() {
            @Override
            public void run() {
                try {
                    Context context = getApplicationContext();
                    Intent intent = new Intent(context, cls);
                    startActivityForResult(intent, 0);

                    synchronized (mFinishEvent) {
                        mFinishEvent.wait();
                    }
                } catch(Exception e) {
                    Log.d(TAG, "callActivity got an exception " + e.toString());
                }
            }
        };
        thread.start();
        thread.join(20000 /* millis */);

        if (mResult == RESULT_OK) {
          throw new Exception();
        }
        return (mResult - RESULT_FIRST_USER) == 1;
    }

    public void testGwpAsanEnabledActivity() throws Exception {
        assertTrue(callActivity(GwpAsanEnabledActivity.class));
    }

    public void testGwpAsanDisabledActivity() throws Exception {
        assertFalse(callActivity(GwpAsanDisabledActivity.class));
    }

    public void testGwpAsanDefaultActivity() throws Exception {
        // GwpAsanDefaultActivity inherits the application attribute.
        assertTrue(callActivity(GwpAsanDefaultActivity.class));
    }
}
