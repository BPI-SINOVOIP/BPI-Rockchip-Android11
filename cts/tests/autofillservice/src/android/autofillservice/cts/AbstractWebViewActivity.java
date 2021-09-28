/*
 * Copyright (C) 2018 The Android Open Source Project
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
package android.autofillservice.cts;

import android.os.SystemClock;
import android.support.test.uiautomator.UiObject2;
import android.view.KeyEvent;
import android.widget.EditText;

abstract class AbstractWebViewActivity extends AbstractAutoFillActivity {

    public static final String FAKE_DOMAIN = "y.u.no.real.server";

    public static final String HTML_NAME_USERNAME = "username";
    public static final String HTML_NAME_PASSWORD = "password";

    protected MyWebView mWebView;

    protected UiObject2 getInput(UiBot uiBot, UiObject2 label) throws Exception {
        // Then the input is next.
        final UiObject2 parent = label.getParent();
        UiObject2 previous = null;
        for (UiObject2 child : parent.getChildren()) {
            if (label.equals(previous)) {
                if (child.getClassName().equals(EditText.class.getName())) {
                    return child;
                }
                uiBot.dumpScreen("getInput() for " + child + "failed");
                throw new IllegalStateException("Invalid class for " + child);
            }
            previous = child;
        }
        uiBot.dumpScreen("getInput() for label " + label + "failed");
        throw new IllegalStateException("could not find username (label=" + label + ")");
    }

    public void dispatchKeyPress(int keyCode) {
        runOnUiThread(() -> {
            KeyEvent keyEvent = new KeyEvent(KeyEvent.ACTION_DOWN, keyCode);
            mWebView.dispatchKeyEvent(keyEvent);
            keyEvent = new KeyEvent(KeyEvent.ACTION_UP, keyCode);
            mWebView.dispatchKeyEvent(keyEvent);
        });
        // wait webview to process the key event.
        SystemClock.sleep(300);
    }
}
