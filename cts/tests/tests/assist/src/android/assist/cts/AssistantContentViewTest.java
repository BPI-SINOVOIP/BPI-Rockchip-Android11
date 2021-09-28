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

package android.assist.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.assist.common.AutoResetLatch;
import android.assist.common.Utils;
import android.graphics.Point;
import android.os.Bundle;
import android.util.Log;

import org.junit.Test;

import java.util.concurrent.TimeUnit;

/** Test verifying the Content View of the Assistant */
public class AssistantContentViewTest extends AssistTestBase {
    private static final String TAG = "ContentViewTest";
    private AutoResetLatch mContentViewLatch = new AutoResetLatch(1);
    private Bundle mBundle;

    @Override
    protected void customSetup() throws Exception {
        mActionLatchReceiver = new AssistantReceiver();
        startTestActivity(Utils.VERIFY_CONTENT_VIEW);
    }

    @Override
    protected void customTearDown() throws Exception {
        mBundle = null;
    }

    private void waitForContentView() throws Exception {
        Log.i(TAG, "waiting for the Assistant's Content View  before continuing");
        if (!mContentViewLatch.await(Utils.TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("failed to receive content view in " + Utils.TIMEOUT_MS + "msec");
        }
    }

    @Test
    public void testAssistantContentViewDimens() throws Exception {
        if (mActivityManager.isLowRamDevice()) {
          Log.d(TAG, "Not running assist tests on low-RAM device.");
          return;
        }
        startTest(Utils.VERIFY_CONTENT_VIEW);
        waitForAssistantToBeReady();
        startSession();
        waitForContentView();
        int height = mBundle.getInt(Utils.EXTRA_CONTENT_VIEW_HEIGHT, 0);
        int width = mBundle.getInt(Utils.EXTRA_CONTENT_VIEW_WIDTH, 0);
        Point displayPoint = mBundle.getParcelable(Utils.EXTRA_DISPLAY_POINT);
        assertThat(height).isEqualTo(displayPoint.y);
        assertThat(width).isEqualTo(displayPoint.x);
    }

    private class AssistantReceiver extends ActionLatchReceiver {

        AssistantReceiver() {
            super(Utils.BROADCAST_CONTENT_VIEW_HEIGHT, mContentViewLatch);
        }

        @Override
        protected void onAction(Bundle bundle, String action) {
            mBundle = bundle;
            super.onAction(bundle, action);
        }
    }
}
