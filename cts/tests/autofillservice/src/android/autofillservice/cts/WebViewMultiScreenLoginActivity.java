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

import static android.autofillservice.cts.Timeouts.WEBVIEW_TIMEOUT;

import static com.google.common.truth.Truth.assertThat;

import android.os.Bundle;
import android.support.test.uiautomator.UiObject2;
import android.util.Log;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import com.android.compatibility.common.util.RetryableException;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class WebViewMultiScreenLoginActivity extends AbstractWebViewActivity {

    private static final String TAG = "WebViewMultiScreenLoginActivity";
    private static final String FAKE_USERNAME_URL = "https://" + FAKE_DOMAIN + ":666/username.html";
    private static final String FAKE_PASSWORD_URL = "https://" + FAKE_DOMAIN + ":666/password.html";

    private UiObject2 mUsernameLabel;
    private UiObject2 mUsernameInput;
    private UiObject2 mNextButton;

    private UiObject2 mPasswordLabel;
    private UiObject2 mPasswordInput;
    private UiObject2 mLoginButton;

    private final Map<String, CountDownLatch> mLatches = new HashMap<>();

    public WebViewMultiScreenLoginActivity() {
        mLatches.put(FAKE_USERNAME_URL, new CountDownLatch(1));
        mLatches.put(FAKE_PASSWORD_URL, new CountDownLatch(1));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.webview_only_activity);
        mWebView = findViewById(R.id.my_webview);
    }

    public MyWebView loadWebView(UiBot uiBot) throws Exception {
        syncRunOnUiThread(() -> {
            mWebView.setWebViewClient(new WebViewClient() {
                // WebView does not set the WebDomain on file:// requests, so we need to use an
                // https:// request and intercept it to provide the real data.
                @Override
                public WebResourceResponse shouldInterceptRequest(WebView view,
                        WebResourceRequest request) {
                    final String url = request.getUrl().toString();
                    if (!url.equals(FAKE_USERNAME_URL) && !url.equals(FAKE_PASSWORD_URL)) {
                        Log.d(TAG, "Ignoring " + url);
                        return super.shouldInterceptRequest(view, request);
                    }

                    final String rawPath = request.getUrl().getPath()
                            .substring(1); // Remove leading /
                    Log.d(TAG, "Converting " + url + " to " + rawPath);
                    // NOTE: cannot use try-with-resources because it would close the stream before
                    // WebView uses it.
                    try {
                        return new WebResourceResponse("text/html", "utf-8",
                                getAssets().open(rawPath));
                    } catch (IOException e) {
                        throw new IllegalArgumentException("Error opening " + rawPath, e);
                    }
                }

                @Override
                public void onPageFinished(WebView view, String url) {
                    final CountDownLatch latch = mLatches.get(url);
                    Log.v(TAG, "onPageFinished(): " + url + " latch: " + latch);
                    if (latch != null) {
                        latch.countDown();
                    }
                }
            });
            mWebView.loadUrl(FAKE_USERNAME_URL);
        });

        // Wait until it's loaded.
        if (!mLatches.get(FAKE_USERNAME_URL).await(WEBVIEW_TIMEOUT.ms(), TimeUnit.MILLISECONDS)) {
            throw new RetryableException(WEBVIEW_TIMEOUT, "WebView not loaded");
        }

        // Sanity check to make sure autofill was enabled when the WebView was created
        assertThat(mWebView.isAutofillEnabled()).isTrue();

        // WebView builds its accessibility tree asynchronously and only after being queried the
        // first time, so we should first find the WebView and query some of its properties,
        // wait for its accessibility tree to be populated (by blocking until a known element
        // appears), then cache the objects for further use.

        // NOTE: we cannot search by resourceId because WebView does not set them...

        // Wait for known element...
        mUsernameLabel = uiBot.assertShownByText("Username: ", WEBVIEW_TIMEOUT);
        // ...then cache the others
        mUsernameInput = getInput(uiBot, mUsernameLabel);
        mNextButton = uiBot.findRightAwayByText("Next");

        return mWebView;
    }

    void waitForPasswordScreen(UiBot uiBot) throws Exception {
        // Wait until it's loaded.
        if (!mLatches.get(FAKE_PASSWORD_URL).await(WEBVIEW_TIMEOUT.ms(), TimeUnit.MILLISECONDS)) {
            throw new RetryableException(WEBVIEW_TIMEOUT, "Password page not loaded");
        }

        // WebView builds its accessibility tree asynchronously and only after being queried the
        // first time, so we should first find the WebView and query some of its properties,
        // wait for its accessibility tree to be populated (by blocking until a known element
        // appears), then cache the objects for further use.

        // NOTE: we cannot search by resourceId because WebView does not set them...

        // Wait for known element...
        mPasswordLabel = uiBot.assertShownByText("Password: ", WEBVIEW_TIMEOUT);
        // ...then cache the others
        mPasswordInput = getInput(uiBot, mPasswordLabel);
        mLoginButton = uiBot.findRightAwayByText("Login");
    }

    public UiObject2 getUsernameLabel() throws Exception {
        return mUsernameLabel;
    }

    public UiObject2 getUsernameInput() throws Exception {
        return mUsernameInput;
    }

    public UiObject2 getNextButton() throws Exception {
        return mNextButton;
    }

    public UiObject2 getPasswordLabel() throws Exception {
        return mPasswordLabel;
    }

    public UiObject2 getPasswordInput() throws Exception {
        return mPasswordInput;
    }

    public UiObject2 getLoginButton() throws Exception {
        return mLoginButton;
    }
}
