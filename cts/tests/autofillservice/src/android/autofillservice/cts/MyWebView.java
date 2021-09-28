/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static android.autofillservice.cts.Timeouts.FILL_TIMEOUT;

import static com.google.common.truth.Truth.assertWithMessage;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.autofill.AutofillManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebView;

import com.android.compatibility.common.util.RetryableException;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Custom {@link WebView} used to assert contents were autofilled.
 */
public class MyWebView extends WebView {

    private static final String TAG = "MyWebView";

    private FillExpectation mExpectation;

    public MyWebView(Context context) {
        super(context);
        setJsHandler();
        Log.d(TAG, "isAutofillEnabled() on constructor? " + isAutofillEnabled());
    }

    public MyWebView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setJsHandler();
        Log.d(TAG, "isAutofillEnabled() on constructor? " + isAutofillEnabled());
    }

    public void expectAutofill(String username, String password) {
        mExpectation = new FillExpectation(username, password);
    }

    public void assertAutofilled() throws Exception {
        assertWithMessage("expectAutofill() not called").that(mExpectation).isNotNull();
        mExpectation.assertUsernameCalled();
        mExpectation.assertPasswordCalled();
    }

    private void setJsHandler() {
        getSettings().setJavaScriptEnabled(true);
        addJavascriptInterface(new JavascriptHandler(), "JsHandler");
    }

    boolean isAutofillEnabled() {
        return getContext().getSystemService(AutofillManager.class).isEnabled();
    }

    private class FillExpectation {
        private final CountDownLatch mUsernameLatch = new CountDownLatch(1);
        private final CountDownLatch mPasswordLatch = new CountDownLatch(1);
        private final String mExpectedUsername;
        private final String mExpectedPassword;
        private String mActualUsername;
        private String mActualPassword;

        FillExpectation(String username, String password) {
            this.mExpectedUsername = username;
            this.mExpectedPassword = password;
        }

        void setUsername(String username) {
            mActualUsername = username;
            mUsernameLatch.countDown();
        }

        void setPassword(String password) {
            mActualPassword = password;
            mPasswordLatch.countDown();
        }

        void assertUsernameCalled() throws Exception {
            assertCalled(mUsernameLatch, "username");
            assertWithMessage("Wrong value for username").that(mExpectation.mActualUsername)
                .isEqualTo(mExpectation.mExpectedUsername);
        }

        void assertPasswordCalled() throws Exception {
            assertCalled(mPasswordLatch, "password");
            assertWithMessage("Wrong value for password").that(mExpectation.mActualPassword)
                    .isEqualTo(mExpectation.mExpectedPassword);
        }

        private void assertCalled(CountDownLatch latch, String field) throws Exception {
            if (!latch.await(FILL_TIMEOUT.ms(), TimeUnit.MILLISECONDS)) {
                throw new RetryableException(FILL_TIMEOUT, "%s not called", field);
            }
        }
    }

    private class JavascriptHandler {

        @JavascriptInterface
        public void onUsernameChanged(String username) {
            Log.d(TAG, "onUsernameChanged():" + username);
            if (mExpectation != null) {
                mExpectation.setUsername(username);
            }
        }

        @JavascriptInterface
        public void onPasswordChanged(String password) {
            Log.d(TAG, "onPasswordChanged():" + password);
            if (mExpectation != null) {
                mExpectation.setPassword(password);
            }
        }
    }
}
