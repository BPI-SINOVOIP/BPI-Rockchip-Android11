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

package android.view.inputmethod.cts.util;

import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.Timeout;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A custom {@link WebView} class which provides a simple web page for verifying IME behavior.
 */
public class TestWebView {

    private static final class Impl extends WebView {
        private static final String MY_HTML =
                "<html><body>Editor: <input type='text' name='testInput'></body></html>";
        private UiDevice mUiDevice;

        Impl(Context context, UiDevice uiDevice) {
            super(context);
            mUiDevice = uiDevice;
        }

        private void loadEditorPage() {
            super.loadData(MY_HTML, "text/html", "UTF-8");
        }

        private UiObject2 getInput(Timeout timeout) {
            final BySelector selector = By.text("Editor: ");
            UiObject2 label = null;
            try {
                label = timeout.run("waitForObject(" + selector + ")",
                        () -> mUiDevice.findObject(selector));
            } catch (Exception e) {
            }
            assertNotNull("Editor label must exists on WebView", label);

            // Then the input is next.
            final UiObject2 parent = label.getParent();
            UiObject2 previous = null;
            for (UiObject2 child : parent.getChildren()) {
                if (label.equals(previous)) {
                    if (child.getClassName().equals(EditText.class.getName())) {
                        return child;
                    }
                }
                previous = child;
            }
            return null;
        }
    }

    /**
     * Not intended to be instantiated.
     */
    private TestWebView() {
    }

    public static UiObject2 launchTestWebViewActivity(long timeoutMs)
            throws Exception {
        final AtomicReference<UiObject2> inputTextFieldRef = new AtomicReference<>();
        final AtomicReference<TestWebView.Impl> webViewRef = new AtomicReference<>();
        final CountDownLatch latch = new CountDownLatch(1);
        final Timeout timeoutForFindObj = new Timeout("UIOBJECT_FIND_TIMEOUT",
                timeoutMs, 2F, timeoutMs);

        TestActivity.startSync(activity -> {
            final LinearLayout layout = new LinearLayout(activity);
            final TestWebView.Impl webView = new Impl(activity, UiDevice.getInstance(
                    InstrumentationRegistry.getInstrumentation()));
            webView.setWebViewClient(new WebViewClient() {
                @Override
                public void onPageFinished(WebView view, String url) {
                    latch.countDown();
                }
            });
            webViewRef.set(webView);
            layout.setOrientation(LinearLayout.VERTICAL);
            layout.addView(webView);
            webView.loadEditorPage();
            return layout;
        });

        latch.await(timeoutMs, TimeUnit.MILLISECONDS);
        inputTextFieldRef.set(webViewRef.get().getInput(timeoutForFindObj));
        return inputTextFieldRef.get();
    }
}
