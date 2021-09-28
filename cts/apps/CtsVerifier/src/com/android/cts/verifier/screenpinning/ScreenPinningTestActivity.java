/*
 * Copyright (C) 2015 The Android Open Source Project
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
package com.android.cts.verifier.screenpinning;

import android.app.ActivityManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import com.android.cts.verifier.OrderedTestActivity;
import com.android.cts.verifier.R;

public class ScreenPinningTestActivity extends OrderedTestActivity {

    private static final String TAG = "ScreenPinningTestActivity";
    private static final long TASK_MODE_CHECK_DELAY = 200;
    private static final int MAX_TASK_MODE_CHECK_COUNT = 5;

    private ActivityManager mActivityManager;

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mActivityManager = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
    }

    @Override
    protected Test[] getTests() {
        return new Test[]{
                // Verify not already pinned.
                mCheckStartedUnpinned,
                // Enter pinning, verify pinned, try leaving and have the user exit.
                mCheckStartPinning,
                mCheckIsPinned,
                mCheckTryLeave,
                mCheckUnpin,
                // Enter pinning, verify pinned, and use APIs to exit.
                mCheckStartPinning,
                mCheckIsPinned,
                mCheckUnpinFromCode
        };
    }

    @Override
    public void onBackPressed() {
        // Block back button so we can test screen pinning exit functionality.
        // Users can still leave by pressing fail (or when done the pass) button.
    }

    @Override
    protected void error(int errorId) {
        error(errorId, new Throwable());
    }

    private void error(final int errorId, final Throwable cause) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                String error = getString(errorId);
                Log.d(TAG, error, cause);

                ((TextView) findViewById(R.id.txt_instruction)).setText(error);
            }
        });
    }

    // Verify we don't start in screen pinning.
    private final Test mCheckStartedUnpinned = new Test(0) {
        public void run() {
            if (mActivityManager.isInLockTaskMode()) {
                error(R.string.error_screen_already_pinned);
            } else {
                succeed();
            }
        }
    };

    // Start screen pinning by having the user click next then confirm it for us.
    private final Test mCheckStartPinning = new Test(R.string.screen_pin_instructions) {
        protected void onNextClick() {
            startLockTask();
            succeed();
        }
    };

    // Click next and check that we got pinned.
    // Wait for the user to click next to verify that they got back from the prompt
    // successfully.
    private final Test mCheckIsPinned = new Test(R.string.screen_pin_check_pinned) {
        protected void onNextClick() {
            if (mActivityManager.isInLockTaskMode()) {
                succeed();
            } else {
                error(R.string.error_screen_pinning_did_not_start);
            }
        }
    };

    // Tell user to try to leave.
    private final Test mCheckTryLeave = new Test(R.string.screen_pin_no_exit) {
        protected void onNextClick() {
            if (mActivityManager.isInLockTaskMode()) {
                succeed();
            } else {
                error(R.string.error_screen_no_longer_pinned);
            }
        }
    };

    // Verify that the user unpinned and it worked.
    private final Test mCheckUnpin = new Test(R.string.screen_pin_exit) {
        protected void onNextClick() {
            if (!mActivityManager.isInLockTaskMode()) {
                succeed();
            } else {
                error(R.string.error_screen_pinning_did_not_exit);
            }
        }
    };

    // Unpin from code and check that it worked.
    private final Test mCheckUnpinFromCode = new Test(0) {
        protected void run() {
            if (!mActivityManager.isInLockTaskMode()) {
                error(R.string.error_screen_pinning_did_not_start);
                return;
            }
            stopLockTask();
            for (int retry = MAX_TASK_MODE_CHECK_COUNT; retry > 0; retry--) {
                try {
                    Thread.sleep(TASK_MODE_CHECK_DELAY);
                } catch (InterruptedException e) {
                }
                Log.d(TAG, "Check unpin ... " + retry);
                if (!mActivityManager.isInLockTaskMode()) {
                    succeed();
                    break;
                } else if (retry == 1) {
                    error(R.string.error_screen_pinning_couldnt_exit);
                }
            }
        }
    };
}
