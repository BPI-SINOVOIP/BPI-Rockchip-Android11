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

import android.assist.common.AutoResetLatch;
import android.assist.common.Utils;
import android.os.Bundle;
import android.util.Log;

import org.junit.Test;

/**
 *  Test that the AssistStructure returned is properly formatted.
 */
public class TextViewTest extends AssistTestBase {
    private static final String TAG = "TextViewTest";
    private static final String TEST_CASE_TYPE = Utils.TEXTVIEW;

    @Override
    protected void customSetup() throws Exception {
        startTestActivity(TEST_CASE_TYPE);
    }

    @Test
    public void testTextView() throws Exception {
        if (mActivityManager.isLowRamDevice()) {
            Log.d(TAG, "Not running assist tests on low-RAM device.");
            return;
        }

        start3pApp(TEST_CASE_TYPE);
        scrollTestApp(0, 0, true, false);

        // Verify that the text view contains the right text
        startTest(TEST_CASE_TYPE);
        waitForAssistantToBeReady();
        final AutoResetLatch latch1 = startSession();
        waitForContext(latch1);
        verifyAssistDataNullness(false, false, false, false);

        verifyAssistStructure(Utils.getTestAppComponent(TEST_CASE_TYPE),
                false /*FLAG_SECURE set*/);

        // Verify that the scroll position of the text view is accurate after scrolling.
        scrollTestApp(10, 50, true /* scrollTextView */, false /* scrollScrollView */);
        final AutoResetLatch latch2 = startSession();
        waitForContext(latch2);
        verifyAssistStructure(Utils.getTestAppComponent(TEST_CASE_TYPE), false);

        scrollTestApp(-1, -1, true, false);
        final AutoResetLatch latch3 = startSession();
        waitForContext(latch3);
        verifyAssistStructure(Utils.getTestAppComponent(TEST_CASE_TYPE), false);

        scrollTestApp(0, 0, true, true);
        final AutoResetLatch latch4 = startSession();
        waitForContext(latch4);
        verifyAssistStructure(Utils.getTestAppComponent(TEST_CASE_TYPE), false);

        scrollTestApp(10, 50, false, true);
        final AutoResetLatch latch5 = startSession();
        waitForContext(latch5);
        verifyAssistStructure(Utils.getTestAppComponent(TEST_CASE_TYPE), false);
    }

    @Override
    protected void scrollTestApp(int scrollX, int scrollY, boolean scrollTextView,
            boolean scrollScrollView) {
        super.scrollTestApp(scrollX, scrollY, scrollTextView, scrollScrollView);
        Bundle bundle = new Bundle();
        if (scrollTextView) {
            bundle.putString(Utils.EXTRA_REMOTE_CALLBACK_ACTION, Utils.SCROLL_TEXTVIEW_ACTION);
        } else if (scrollScrollView) {
            bundle.putString(Utils.EXTRA_REMOTE_CALLBACK_ACTION, Utils.SCROLL_SCROLLVIEW_ACTION);
        }
        bundle.putInt(Utils.SCROLL_X_POSITION, scrollX);
        bundle.putInt(Utils.SCROLL_Y_POSITION, scrollY);
        m3pActivityCallback.sendResult(bundle);
    }
}
