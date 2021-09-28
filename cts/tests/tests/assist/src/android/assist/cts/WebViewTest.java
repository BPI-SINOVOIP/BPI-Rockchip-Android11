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

import static org.junit.Assert.fail;

import android.assist.common.AutoResetLatch;
import android.assist.common.Utils;
import android.content.pm.PackageManager;
import android.util.Log;

import org.junit.Test;

import java.util.concurrent.TimeUnit;

/**
 *  Test that the AssistStructure returned is properly formatted.
 */
public class WebViewTest extends AssistTestBase {
    private static final String TAG = "WebViewTest";
    private static final String TEST_CASE_TYPE = Utils.WEBVIEW;

    private final AutoResetLatch mTestWebViewLatch = new AutoResetLatch();

    @Override
    protected void customSetup() throws Exception {
        mActionLatchReceiver = new ActionLatchReceiver(Utils.TEST_ACTIVITY_WEBVIEW_LOADED, mTestWebViewLatch);
        startTestActivity(TEST_CASE_TYPE);
    }

    private void waitForTestActivity() throws Exception {
        Log.i(TAG, "waiting for webview in test activity to load");
        if (!mTestWebViewLatch.await(Utils.ACTIVITY_ONRESUME_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("failed to receive load web view in " + Utils.TIMEOUT_MS + "msec");
        }
    }

    @Test
    public void testWebView() throws Throwable {
        if (mActivityManager.isLowRamDevice()) {
            Log.d(TAG, "Not running assist tests on low-RAM device.");
            return;
        }
        if (!mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WEBVIEW)) {
            return;
        }
        start3pApp(TEST_CASE_TYPE);
        startTest(TEST_CASE_TYPE);
        waitForAssistantToBeReady();
        waitForTestActivity();

        eventuallyWithSessionClose(() -> {
            waitForContext(startSession());
            verifyAssistDataNullness(false, false, false, false);
            verifyAssistStructure(Utils.getTestAppComponent(TEST_CASE_TYPE),
                    false /*FLAG_SECURE set*/);
            verifyAssistStructureHasWebDomain(Utils.WEBVIEW_HTML_DOMAIN);
            verifyAssistStructureHasLocaleList(Utils.WEBVIEW_LOCALE_LIST);
        });
    }
}
