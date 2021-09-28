/*
 * Copyright (C) 2009 The Android Open Source Project
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

package android.webkit.cts;

import android.app.ActivityManager;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Message;
import android.platform.test.annotations.AppModeFull;
import android.test.ActivityInstrumentationTestCase2;
import android.view.KeyEvent;
import android.view.ViewGroup;
import android.webkit.HttpAuthHandler;
import android.webkit.RenderProcessGoneDetail;
import android.webkit.SafeBrowsingResponse;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.cts.WebViewSyncLoader.WaitForLoadedClient;
import android.util.Pair;

import com.android.compatibility.common.util.NullWebViewUtils;
import com.android.compatibility.common.util.PollingCheck;
import com.google.common.util.concurrent.SettableFuture;

import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;
import java.util.List;
import java.util.ArrayList;

@AppModeFull
public class WebViewClientTest extends ActivityInstrumentationTestCase2<WebViewCtsActivity> {
    private static final long TEST_TIMEOUT = 5000;
    private static final String TEST_URL = "http://www.example.com/";

    private WebViewOnUiThread mOnUiThread;
    private CtsTestServer mWebServer;

    private static final String TEST_SAFE_BROWSING_URL_PREFIX =
            "chrome://safe-browsing/match?type=";
    private static final String TEST_SAFE_BROWSING_MALWARE_URL =
            TEST_SAFE_BROWSING_URL_PREFIX + "malware";
    private static final String TEST_SAFE_BROWSING_PHISHING_URL =
            TEST_SAFE_BROWSING_URL_PREFIX + "phishing";
    private static final String TEST_SAFE_BROWSING_UNWANTED_SOFTWARE_URL =
            TEST_SAFE_BROWSING_URL_PREFIX + "unwanted";
    private static final String TEST_SAFE_BROWSING_BILLING_URL =
            TEST_SAFE_BROWSING_URL_PREFIX + "billing";

    public WebViewClientTest() {
        super("android.webkit.cts", WebViewCtsActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        final WebViewCtsActivity activity = getActivity();
        WebView webview = activity.getWebView();
        if (webview != null) {
            new PollingCheck(TEST_TIMEOUT) {
                @Override
                    protected boolean check() {
                    return activity.hasWindowFocus();
                }
            }.run();

            mOnUiThread = new WebViewOnUiThread(webview);
        }
    }

    @Override
    protected void tearDown() throws Exception {
        if (mOnUiThread != null) {
            mOnUiThread.cleanUp();
        }
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
        super.tearDown();
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebViewClientCompatTest#testShouldOverrideUrlLoadingDefault. Modifications
     * to this test should be reflected in that test as necessary. See
     * http://go/modifying-webview-cts.
     */
    // Verify that the shouldoverrideurlloading is false by default
    public void testShouldOverrideUrlLoadingDefault() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final WebViewClient webViewClient = new WebViewClient();
        assertFalse(webViewClient.shouldOverrideUrlLoading(mOnUiThread.getWebView(), new String()));
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebViewClientCompatTest#testShouldOverrideUrlLoading. Modifications to this
     * test should be reflected in that test as necessary. See http://go/modifying-webview-cts.
     */
    // Verify shouldoverrideurlloading called on top level navigation
    public void testShouldOverrideUrlLoading() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);
        mOnUiThread.getSettings().setJavaScriptEnabled(true);
        String data = "<html><body>" +
                "<a href=\"" + TEST_URL + "\" id=\"link\">new page</a>" +
                "</body></html>";
        mOnUiThread.loadDataAndWaitForCompletion(data, "text/html", null);
        clickOnLinkUsingJs("link", mOnUiThread);
        assertEquals(TEST_URL, webViewClient.getLastShouldOverrideUrl());
        assertNotNull(webViewClient.getLastShouldOverrideResourceRequest());
        assertTrue(webViewClient.getLastShouldOverrideResourceRequest().isForMainFrame());
        assertFalse(webViewClient.getLastShouldOverrideResourceRequest().isRedirect());
        assertFalse(webViewClient.getLastShouldOverrideResourceRequest().hasGesture());
    }

    // Verify shouldoverrideurlloading called on webview called via onCreateWindow
    // TODO(sgurun) upstream this test to Aw.
    public void testShouldOverrideUrlLoadingOnCreateWindow() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        mWebServer = new CtsTestServer(getActivity());
        // WebViewClient for main window
        final MockWebViewClient mainWebViewClient = new MockWebViewClient();
        // WebViewClient for child window
        final MockWebViewClient childWebViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(mainWebViewClient);
        mOnUiThread.getSettings().setJavaScriptEnabled(true);
        mOnUiThread.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
        mOnUiThread.getSettings().setSupportMultipleWindows(true);

        final WebView childWebView = mOnUiThread.createWebView();

        WebViewOnUiThread childWebViewOnUiThread = new WebViewOnUiThread(childWebView);
        mOnUiThread.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onCreateWindow(
                WebView view, boolean isDialog, boolean isUserGesture, Message resultMsg) {
                WebView.WebViewTransport transport = (WebView.WebViewTransport) resultMsg.obj;
                childWebView.setWebViewClient(childWebViewClient);
                childWebView.getSettings().setJavaScriptEnabled(true);
                transport.setWebView(childWebView);
                getActivity().addContentView(childWebView, new ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.FILL_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT));
                resultMsg.sendToTarget();
                return true;
            }
        });
        {
          final int childCallCount = childWebViewClient.getShouldOverrideUrlLoadingCallCount();
          mOnUiThread.loadUrl(mWebServer.getAssetUrl(TestHtmlConstants.BLANK_TAG_URL));

          new PollingCheck(TEST_TIMEOUT) {
              @Override
              protected boolean check() {
                  return childWebViewClient.hasOnPageFinishedCalled();
              }
          }.run();
          new PollingCheck(TEST_TIMEOUT) {
              @Override
              protected boolean check() {
                  return childWebViewClient.getShouldOverrideUrlLoadingCallCount() > childCallCount;
              }
          }.run();
          assertEquals(mWebServer.getAssetUrl(TestHtmlConstants.PAGE_WITH_LINK_URL),
                  childWebViewClient.getLastShouldOverrideUrl());
        }

        final int childCallCount = childWebViewClient.getShouldOverrideUrlLoadingCallCount();
        final int mainCallCount = mainWebViewClient.getShouldOverrideUrlLoadingCallCount();
        clickOnLinkUsingJs("link", childWebViewOnUiThread);
        new PollingCheck(TEST_TIMEOUT) {
            @Override
            protected boolean check() {
                return childWebViewClient.getShouldOverrideUrlLoadingCallCount() > childCallCount;
            }
        }.run();
        assertEquals(mainCallCount, mainWebViewClient.getShouldOverrideUrlLoadingCallCount());
        // PAGE_WITH_LINK_URL has a link to BLANK_PAGE_URL (an arbitrary page also controlled by the
        // test server)
        assertEquals(mWebServer.getAssetUrl(TestHtmlConstants.BLANK_PAGE_URL),
                childWebViewClient.getLastShouldOverrideUrl());
    }

    private void clickOnLinkUsingJs(final String linkId, WebViewOnUiThread webViewOnUiThread) {
        assertEquals("null", webViewOnUiThread.evaluateJavascriptSync(
                        "document.getElementById('" + linkId + "').click();" +
                        "console.log('element with id [" + linkId + "] clicked');"));
    }

    public void testLoadPage() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);
        mWebServer = new CtsTestServer(getActivity());
        String url = mWebServer.getAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);

        assertFalse(webViewClient.hasOnPageStartedCalled());
        assertFalse(webViewClient.hasOnLoadResourceCalled());
        assertFalse(webViewClient.hasOnPageFinishedCalled());
        mOnUiThread.loadUrlAndWaitForCompletion(url);

        new PollingCheck(TEST_TIMEOUT) {
            @Override
            protected boolean check() {
                return webViewClient.hasOnPageStartedCalled();
            }
        }.run();

        new PollingCheck(TEST_TIMEOUT) {
            @Override
            protected boolean check() {
                return webViewClient.hasOnLoadResourceCalled();
            }
        }.run();

        new PollingCheck(TEST_TIMEOUT) {
            @Override
            protected boolean check() {
                return webViewClient.hasOnPageFinishedCalled();
            }
        }.run();
    }

    public void testOnReceivedLoginRequest() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);
        TestWebServer testServer = null;
        //set the url and html
        final String path = "/main";
        final String page = "<head></head><body>test onReceivedLoginRequest</body>";
        final String headerName = "x-auto-login";
        final String headerValue = "realm=com.google&account=foo%40bar.com&args=random_string";
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create(headerName, headerValue));

        try {
            testServer = new TestWebServer(false);
            String url = testServer.setResponse(path, page, headers);
            assertFalse(webViewClient.hasOnReceivedLoginRequest());
            mOnUiThread.loadUrlAndWaitForCompletion(url);
            assertTrue(webViewClient.hasOnReceivedLoginRequest());
            new PollingCheck(TEST_TIMEOUT) {
                @Override
                protected boolean check() {
                    return webViewClient.hasOnReceivedLoginRequest();
                }
            }.run();
           assertEquals("com.google", webViewClient.getLoginRequestRealm());
           assertEquals("foo@bar.com", webViewClient.getLoginRequestAccount());
           assertEquals("random_string", webViewClient.getLoginRequestArgs());
        } finally {
            testServer.shutdown();
        }
    }
    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebViewClientCompatTest#testOnReceivedError. Modifications to this test
     * should be reflected in that test as necessary. See http://go/modifying-webview-cts.
     */
    public void testOnReceivedError() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);

        String wrongUri = "invalidscheme://some/resource";
        assertEquals(0, webViewClient.hasOnReceivedErrorCode());
        mOnUiThread.loadUrlAndWaitForCompletion(wrongUri);
        assertEquals(WebViewClient.ERROR_UNSUPPORTED_SCHEME,
                webViewClient.hasOnReceivedErrorCode());
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebViewClientCompatTest#testOnReceivedErrorForSubresource. Modifications to
     * this test should be reflected in that test as necessary. See http://go/modifying-webview-cts.
     */
    public void testOnReceivedErrorForSubresource() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);
        mWebServer = new CtsTestServer(getActivity());

        assertNull(webViewClient.hasOnReceivedResourceError());
        String url = mWebServer.getAssetUrl(TestHtmlConstants.BAD_IMAGE_PAGE_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        assertTrue(webViewClient.hasOnReceivedResourceError() != null);
        assertEquals(WebViewClient.ERROR_UNSUPPORTED_SCHEME,
                webViewClient.hasOnReceivedResourceError().getErrorCode());
    }

    public void testOnReceivedHttpError() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);
        mWebServer = new CtsTestServer(getActivity());

        assertNull(webViewClient.hasOnReceivedHttpError());
        String url = mWebServer.getAssetUrl(TestHtmlConstants.NON_EXISTENT_PAGE_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        assertTrue(webViewClient.hasOnReceivedHttpError() != null);
        assertEquals(404, webViewClient.hasOnReceivedHttpError().getStatusCode());
    }

    public void testOnFormResubmission() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);
        final WebSettings settings = mOnUiThread.getSettings();
        settings.setJavaScriptEnabled(true);
        mWebServer = new CtsTestServer(getActivity());

        assertFalse(webViewClient.hasOnFormResubmissionCalled());
        String url = mWebServer.getAssetUrl(TestHtmlConstants.JS_FORM_URL);
        // this loads a form, which automatically posts itself
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        // wait for JavaScript to post the form
        mOnUiThread.waitForLoadCompletion();
        assertFalse("The URL should have changed when the form was posted",
                url.equals(mOnUiThread.getUrl()));
        // reloading the current URL should trigger the callback
        mOnUiThread.reload();
        new PollingCheck(TEST_TIMEOUT) {
            @Override
            protected boolean check() {
                return webViewClient.hasOnFormResubmissionCalled();
            }
        }.run();
    }

    public void testDoUpdateVisitedHistory() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);
        mWebServer = new CtsTestServer(getActivity());

        assertFalse(webViewClient.hasDoUpdateVisitedHistoryCalled());
        String url1 = mWebServer.getAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);
        String url2 = mWebServer.getAssetUrl(TestHtmlConstants.BR_TAG_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url1);
        mOnUiThread.loadUrlAndWaitForCompletion(url2);
        new PollingCheck(TEST_TIMEOUT) {
            @Override
            protected boolean check() {
                return webViewClient.hasDoUpdateVisitedHistoryCalled();
            }
        }.run();
    }

    public void testOnReceivedHttpAuthRequest() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);
        mWebServer = new CtsTestServer(getActivity());

        assertFalse(webViewClient.hasOnReceivedHttpAuthRequestCalled());
        String url = mWebServer.getAuthAssetUrl(TestHtmlConstants.EMBEDDED_IMG_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        assertTrue(webViewClient.hasOnReceivedHttpAuthRequestCalled());
    }

    public void testShouldOverrideKeyEvent() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);

        assertFalse(webViewClient.shouldOverrideKeyEvent(mOnUiThread.getWebView(), null));
    }

    public void testOnUnhandledKeyEvent() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        requireLoadedPage();
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);

        mOnUiThread.requestFocus();
        getInstrumentation().waitForIdleSync();

        assertFalse(webViewClient.hasOnUnhandledKeyEventCalled());
        sendKeys(KeyEvent.KEYCODE_1);

        new PollingCheck(TEST_TIMEOUT) {
            @Override
            protected boolean check() {
                return webViewClient.hasOnUnhandledKeyEventCalled();
            }
        }.run();
    }

    public void testOnScaleChanged() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);
        mWebServer = new CtsTestServer(getActivity());

        assertFalse(webViewClient.hasOnScaleChangedCalled());
        String url1 = mWebServer.getAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url1);

        new PollingCheck(TEST_TIMEOUT) {
            @Override
            protected boolean check() {
                return mOnUiThread.canZoomIn();
            }
        }.run();

        assertTrue(mOnUiThread.zoomIn());
        new PollingCheck(TEST_TIMEOUT) {
            @Override
            protected boolean check() {
                return webViewClient.hasOnScaleChangedCalled();
            }
        }.run();
    }

    // Test that shouldInterceptRequest is called with the correct parameters
    public void testShouldInterceptRequestParams() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        final String mainPath = "/main";
        final String mainPage = "<head></head><body>test page</body>";
        final String headerName = "x-test-header-name";
        final String headerValue = "testheadervalue";
        HashMap<String, String> headers = new HashMap<String, String>(1);
        headers.put(headerName, headerValue);

        // A client which saves the WebResourceRequest as interceptRequest
        final class TestClient extends WaitForLoadedClient {
            public TestClient() {
                super(mOnUiThread);
            }

            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view,
                    WebResourceRequest request) {
                assertNotNull(view);
                assertNotNull(request);

                assertEquals(view, mOnUiThread.getWebView());

                // Save the main page request; discard any other requests (e.g. for favicon.ico)
                if (request.getUrl().getPath().equals(mainPath)) {
                    assertNull(interceptRequest);
                    interceptRequest = request;
                }

                return null;
            }

            public volatile WebResourceRequest interceptRequest;
        }

        TestClient client = new TestClient();
        mOnUiThread.setWebViewClient(client);

        TestWebServer server = new TestWebServer(false);
        try {
            String mainUrl = server.setResponse(mainPath, mainPage, null);

            mOnUiThread.loadUrlAndWaitForCompletion(mainUrl, headers);

            // Inspect the fields of the saved WebResourceRequest
            assertNotNull(client.interceptRequest);
            assertEquals(mainUrl, client.interceptRequest.getUrl().toString());
            assertTrue(client.interceptRequest.isForMainFrame());
            assertEquals(server.getLastRequest(mainPath).getRequestLine().getMethod(),
                client.interceptRequest.getMethod());

            // Web request headers are case-insensitive. We provided lower-case headerName and
            // headerValue. This will pass implementations which either do not mangle case,
            // convert to lowercase, or convert to uppercase but return a case-insensitive map.
            Map<String, String> interceptHeaders = client.interceptRequest.getRequestHeaders();
            assertTrue(interceptHeaders.containsKey(headerName));
            assertEquals(headerValue, interceptHeaders.get(headerName));
        } finally {
            server.shutdown();
        }
    }

    // Test that the WebResourceResponse returned by shouldInterceptRequest is handled correctly
    public void testShouldInterceptRequestResponse() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        final String mainPath = "/main";
        final String mainPage = "<head></head><body>test page</body>";
        final String interceptPath = "/intercept_me";

        // A client which responds to requests for interceptPath with a saved interceptResponse
        final class TestClient extends WaitForLoadedClient {
            public TestClient() {
                super(mOnUiThread);
            }

            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view,
                    WebResourceRequest request) {
                if (request.getUrl().toString().contains(interceptPath)) {
                    assertNotNull(interceptResponse);
                    return interceptResponse;
                }

                return null;
            }

            volatile public WebResourceResponse interceptResponse;
        }

        mOnUiThread.getSettings().setJavaScriptEnabled(true);

        TestClient client = new TestClient();
        mOnUiThread.setWebViewClient(client);

        TestWebServer server = new TestWebServer(false);
        try {
            String interceptUrl = server.getResponseUrl(interceptPath);

            // JavaScript which makes a synchronous AJAX request and logs and returns the status
            String js =
                "(function() {" +
                "  var xhr = new XMLHttpRequest();" +
                "  xhr.open('GET', '" + interceptUrl + "', false);" +
                "  xhr.send(null);" +
                "  console.info('xhr.status = ' + xhr.status);" +
                "  console.info('xhr.statusText = ' + xhr.statusText);" +
                "  return '[' + xhr.status + '][' + xhr.statusText + ']';" +
                "})();";

            String mainUrl = server.setResponse(mainPath, mainPage, null);
            mOnUiThread.loadUrlAndWaitForCompletion(mainUrl, null);

            // Test a nonexistent page
            client.interceptResponse = new WebResourceResponse("text/html", "UTF-8", null);
            assertEquals("\"[404][Not Found]\"", mOnUiThread.evaluateJavascriptSync(js));

            // Test an empty page
            client.interceptResponse = new WebResourceResponse("text/html", "UTF-8",
                new ByteArrayInputStream(new byte[0]));
            assertEquals("\"[200][OK]\"", mOnUiThread.evaluateJavascriptSync(js));

            // Test a nonempty page with unusual response code/text
            client.interceptResponse =
                new WebResourceResponse("text/html", "UTF-8", 123, "unusual", null,
                    new ByteArrayInputStream("nonempty page".getBytes(StandardCharsets.UTF_8)));
            assertEquals("\"[123][unusual]\"", mOnUiThread.evaluateJavascriptSync(js));
        } finally {
            server.shutdown();
        }
    }

    // Verify that OnRenderProcessGone returns false by default
    public void testOnRenderProcessGoneDefault() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final WebViewClient webViewClient = new WebViewClient();
        assertFalse(webViewClient.onRenderProcessGone(mOnUiThread.getWebView(), null));
    }

    public void testOnRenderProcessGone() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        final MockWebViewClient webViewClient = new MockWebViewClient();
        mOnUiThread.setWebViewClient(webViewClient);
        mOnUiThread.loadUrl("chrome://kill");
        new PollingCheck(TEST_TIMEOUT * 5) {
            @Override
            protected boolean check() {
                return webViewClient.hasRenderProcessGoneCalled();
            }
        }.run();
        assertFalse(webViewClient.didRenderProcessCrash());
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebViewClientCompatTest#testOnSafeBrowsingHitBackToSafety.
     * Modifications to this test should be reflected in that test as necessary. See
     * http://go/modifying-webview-cts.
     */
    public void testOnSafeBrowsingHitBackToSafety() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        mWebServer = new CtsTestServer(getActivity());
        String url = mWebServer.getAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        final String ORIGINAL_URL = mOnUiThread.getUrl();

        final SafeBrowsingBackToSafetyClient backToSafetyWebViewClient =
                new SafeBrowsingBackToSafetyClient();
        mOnUiThread.setWebViewClient(backToSafetyWebViewClient);
        mOnUiThread.getSettings().setSafeBrowsingEnabled(true);

        // Note: Safe Browsing is enabled by default, and will work on chrome://safe-browsing/ URLs
        // regardless of user opt-in or GMS state (because this URL will never be sent to GMS Core).
        assertEquals(0, backToSafetyWebViewClient.hasOnReceivedErrorCode());
        mOnUiThread.loadUrlAndWaitForCompletion(TEST_SAFE_BROWSING_MALWARE_URL);

        assertEquals(TEST_SAFE_BROWSING_MALWARE_URL,
                backToSafetyWebViewClient.getOnSafeBrowsingHitRequest().getUrl().toString());
        assertTrue(backToSafetyWebViewClient.getOnSafeBrowsingHitRequest().isForMainFrame());

        assertEquals("Back to safety should produce a network error",
                WebViewClient.ERROR_UNSAFE_RESOURCE,
                backToSafetyWebViewClient.hasOnReceivedErrorCode());

        assertEquals("Back to safety should navigate backward", ORIGINAL_URL,
                mOnUiThread.getUrl());
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebViewClientCompatTest#testOnSafeBrowsingHitProceed.
     * Modifications to this test should be reflected in that test as necessary. See
     * http://go/modifying-webview-cts.
     */
    public void testOnSafeBrowsingHitProceed() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        mWebServer = new CtsTestServer(getActivity());
        String url = mWebServer.getAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        final String ORIGINAL_URL = mOnUiThread.getUrl();

        final SafeBrowsingProceedClient proceedWebViewClient =
                new SafeBrowsingProceedClient();
        mOnUiThread.setWebViewClient(proceedWebViewClient);

        // Note: Safe Browsing is enabled by default, and will work on chrome://safe-browsing/ URLs
        // regardless of user opt-in or GMS state (because this URL will never be sent to GMS Core).
        assertEquals(0, proceedWebViewClient.hasOnReceivedErrorCode());
        mOnUiThread.loadUrlAndWaitForCompletion(TEST_SAFE_BROWSING_MALWARE_URL);

        assertEquals(TEST_SAFE_BROWSING_MALWARE_URL,
                proceedWebViewClient.getOnSafeBrowsingHitRequest().getUrl().toString());
        assertTrue(proceedWebViewClient.getOnSafeBrowsingHitRequest().isForMainFrame());

        assertEquals("Proceed button should navigate to the page",
                TEST_SAFE_BROWSING_MALWARE_URL, mOnUiThread.getUrl());
    }

    private void testOnSafeBrowsingCode(String expectedUrl, int expectedThreatType)
            throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        mWebServer = new CtsTestServer(getActivity());
        String url = mWebServer.getAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        final String ORIGINAL_URL = mOnUiThread.getUrl();

        final SafeBrowsingBackToSafetyClient backToSafetyWebViewClient =
                new SafeBrowsingBackToSafetyClient();
        mOnUiThread.setWebViewClient(backToSafetyWebViewClient);

        // Note: Safe Browsing is enabled by default, and will work on chrome://safe-browsing/ URLs
        // regardless of user opt-in or GMS state (because this URL will never be sent to GMS Core).
        mOnUiThread.loadUrlAndWaitForCompletion(expectedUrl);

        assertEquals("Safe Browsing hit is for unexpected URL",
                expectedUrl,
                backToSafetyWebViewClient.getOnSafeBrowsingHitRequest().getUrl().toString());

        assertEquals("Safe Browsing hit has unexpected threat type",
                expectedThreatType,
                backToSafetyWebViewClient.getOnSafeBrowsingHitThreatType());
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebViewClientCompatTest#testOnSafeBrowsingMalwareCode.
     * Modifications to this test should be reflected in that test as necessary. See
     * http://go/modifying-webview-cts.
     */
    public void testOnSafeBrowsingMalwareCode() throws Throwable {
        testOnSafeBrowsingCode(TEST_SAFE_BROWSING_MALWARE_URL,
                WebViewClient.SAFE_BROWSING_THREAT_MALWARE);
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebViewClientCompatTest#testOnSafeBrowsingPhishingCode.
     * Modifications to this test should be reflected in that test as necessary. See
     * http://go/modifying-webview-cts.
     */
    public void testOnSafeBrowsingPhishingCode() throws Throwable {
        testOnSafeBrowsingCode(TEST_SAFE_BROWSING_PHISHING_URL,
                WebViewClient.SAFE_BROWSING_THREAT_PHISHING);
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebViewClientCompatTest#testOnSafeBrowsingUnwantedSoftwareCode.
     * Modifications to this test should be reflected in that test as necessary. See
     * http://go/modifying-webview-cts.
     */
    public void testOnSafeBrowsingUnwantedSoftwareCode() throws Throwable {
        testOnSafeBrowsingCode(TEST_SAFE_BROWSING_UNWANTED_SOFTWARE_URL,
                WebViewClient.SAFE_BROWSING_THREAT_UNWANTED_SOFTWARE);
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebViewClientCompatTest#testOnSafeBrowsingBillingCode.
     * Modifications to this test should be reflected in that test as necessary. See
     * http://go/modifying-webview-cts.
     */
    public void testOnSafeBrowsingBillingCode() throws Throwable {
        testOnSafeBrowsingCode(TEST_SAFE_BROWSING_BILLING_URL,
                WebViewClient.SAFE_BROWSING_THREAT_BILLING);
    }

    private void requireLoadedPage() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        mOnUiThread.loadUrlAndWaitForCompletion("about:blank");
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebViewClientCompatTest#testOnPageCommitVisibleCalled.
     * Modifications to this test should be reflected in that test as necessary. See
     * http://go/modifying-webview-cts.
     */
    public void testOnPageCommitVisibleCalled() throws Exception {
        // Check that the onPageCommitVisible callback is called
        // correctly.
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        final SettableFuture<String> pageCommitVisibleFuture = SettableFuture.create();
        mOnUiThread.setWebViewClient(new WebViewClient() {
            public void onPageCommitVisible(WebView view, String url) {
                pageCommitVisibleFuture.set(url);
            }
        });

        final String url = "about:blank";
        mOnUiThread.loadUrl(url);
        assertEquals(url, WebkitUtils.waitForFuture(pageCommitVisibleFuture));
    }

    private class MockWebViewClient extends WaitForLoadedClient {
        private boolean mOnPageStartedCalled;
        private boolean mOnPageFinishedCalled;
        private boolean mOnLoadResourceCalled;
        private int mOnReceivedErrorCode;
        private WebResourceError mOnReceivedResourceError;
        private WebResourceResponse mOnReceivedHttpError;
        private boolean mOnFormResubmissionCalled;
        private boolean mDoUpdateVisitedHistoryCalled;
        private boolean mOnReceivedHttpAuthRequestCalled;
        private boolean mOnReceivedLoginRequest;
        private String mOnReceivedLoginAccount;
        private String mOnReceivedLoginArgs;
        private String mOnReceivedLoginRealm;
        private boolean mOnUnhandledKeyEventCalled;
        private boolean mOnScaleChangedCalled;
        private int mShouldOverrideUrlLoadingCallCount;
        private String mLastShouldOverrideUrl;
        private WebResourceRequest mLastShouldOverrideResourceRequest;
        private boolean mOnRenderProcessGoneCalled;
        private boolean mRenderProcessCrashed;

        public MockWebViewClient() {
            super(mOnUiThread);
        }

        public boolean hasOnPageStartedCalled() {
            return mOnPageStartedCalled;
        }

        public boolean hasOnPageFinishedCalled() {
            return mOnPageFinishedCalled;
        }

        public boolean hasOnLoadResourceCalled() {
            return mOnLoadResourceCalled;
        }

        public int hasOnReceivedErrorCode() {
            return mOnReceivedErrorCode;
        }

        public boolean hasOnReceivedLoginRequest() {
            return mOnReceivedLoginRequest;
        }

        public WebResourceError hasOnReceivedResourceError() {
            return mOnReceivedResourceError;
        }

        public WebResourceResponse hasOnReceivedHttpError() {
            return mOnReceivedHttpError;
        }

        public boolean hasOnFormResubmissionCalled() {
            return mOnFormResubmissionCalled;
        }

        public boolean hasDoUpdateVisitedHistoryCalled() {
            return mDoUpdateVisitedHistoryCalled;
        }

        public boolean hasOnReceivedHttpAuthRequestCalled() {
            return mOnReceivedHttpAuthRequestCalled;
        }

        public boolean hasOnUnhandledKeyEventCalled() {
            return mOnUnhandledKeyEventCalled;
        }

        public boolean hasOnScaleChangedCalled() {
            return mOnScaleChangedCalled;
        }

        public int getShouldOverrideUrlLoadingCallCount() {
            return mShouldOverrideUrlLoadingCallCount;
        }

        public String getLastShouldOverrideUrl() {
            return mLastShouldOverrideUrl;
        }

        public WebResourceRequest getLastShouldOverrideResourceRequest() {
            return mLastShouldOverrideResourceRequest;
        }

        public String getLoginRequestRealm() {
            return mOnReceivedLoginRealm;
        }

        public String getLoginRequestAccount() {
            return mOnReceivedLoginAccount;
        }

        public String getLoginRequestArgs() {
            return mOnReceivedLoginArgs;
        }

        public boolean hasRenderProcessGoneCalled() {
            return mOnRenderProcessGoneCalled;
        }

        public boolean didRenderProcessCrash() {
            return mRenderProcessCrashed;
        }

        @Override
        public void onPageStarted(WebView view, String url, Bitmap favicon) {
            super.onPageStarted(view, url, favicon);
            mOnPageStartedCalled = true;
        }

        @Override
        public void onPageFinished(WebView view, String url) {
            super.onPageFinished(view, url);
            // TODO(ntfschr): propagate these exceptions to the instrumentation thread.
            assertTrue("Expected onPageStarted to be called before onPageFinished",
                    mOnPageStartedCalled);
            assertTrue(
                    "Expected onLoadResource or onReceivedError to be called before onPageFinished",
                    mOnLoadResourceCalled || mOnReceivedResourceError != null);
            mOnPageFinishedCalled = true;
        }

        @Override
        public void onLoadResource(WebView view, String url) {
            super.onLoadResource(view, url);
            mOnLoadResourceCalled = true;
        }

        @Override
        public void onReceivedError(WebView view, int errorCode,
                String description, String failingUrl) {
            super.onReceivedError(view, errorCode, description, failingUrl);
            mOnReceivedErrorCode = errorCode;
        }

        @Override
        public void onReceivedError(WebView view, WebResourceRequest request,
                WebResourceError error) {
            super.onReceivedError(view, request, error);
            mOnReceivedResourceError = error;
        }

        @Override
        public void onReceivedHttpError(WebView view,  WebResourceRequest request,
                WebResourceResponse errorResponse) {
            super.onReceivedHttpError(view, request, errorResponse);
            mOnReceivedHttpError = errorResponse;
        }

        @Override
        public void onReceivedLoginRequest(WebView view, String realm, String account,
                String args) {
            super.onReceivedLoginRequest(view, realm, account, args);
            mOnReceivedLoginRequest = true;
            mOnReceivedLoginRealm = realm;
            mOnReceivedLoginAccount = account;
            mOnReceivedLoginArgs = args;
       }

        @Override
        public void onFormResubmission(WebView view, Message dontResend, Message resend) {
            mOnFormResubmissionCalled = true;
            dontResend.sendToTarget();
        }

        @Override
        public void doUpdateVisitedHistory(WebView view, String url, boolean isReload) {
            super.doUpdateVisitedHistory(view, url, isReload);
            mDoUpdateVisitedHistoryCalled = true;
        }

        @Override
        public void onReceivedHttpAuthRequest(WebView view,
                HttpAuthHandler handler, String host, String realm) {
            super.onReceivedHttpAuthRequest(view, handler, host, realm);
            mOnReceivedHttpAuthRequestCalled = true;
        }

        @Override
        public void onUnhandledKeyEvent(WebView view, KeyEvent event) {
            super.onUnhandledKeyEvent(view, event);
            mOnUnhandledKeyEventCalled = true;
        }

        @Override
        public void onScaleChanged(WebView view, float oldScale, float newScale) {
            super.onScaleChanged(view, oldScale, newScale);
            mOnScaleChangedCalled = true;
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, String url) {
            mLastShouldOverrideUrl = url;
            mLastShouldOverrideResourceRequest = null;
            mShouldOverrideUrlLoadingCallCount++;
            return false;
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
            mLastShouldOverrideUrl = request.getUrl().toString();
            mLastShouldOverrideResourceRequest = request;
            mShouldOverrideUrlLoadingCallCount++;
            return false;
        }

        @Override
        public boolean onRenderProcessGone(WebView view, RenderProcessGoneDetail  detail) {
            mOnRenderProcessGoneCalled = true;
            mRenderProcessCrashed = detail.didCrash();
            return true;
        }
    }

    private class SafeBrowsingBackToSafetyClient extends MockWebViewClient {
        private WebResourceRequest mOnSafeBrowsingHitRequest;
        private int mOnSafeBrowsingHitThreatType;

        public WebResourceRequest getOnSafeBrowsingHitRequest() {
            return mOnSafeBrowsingHitRequest;
        }

        public int getOnSafeBrowsingHitThreatType() {
            return mOnSafeBrowsingHitThreatType;
        }

        @Override
        public void onSafeBrowsingHit(WebView view, WebResourceRequest request,
                int threatType, SafeBrowsingResponse response) {
            // Immediately go back to safety to return the network error code
            mOnSafeBrowsingHitRequest = request;
            mOnSafeBrowsingHitThreatType = threatType;
            response.backToSafety(/* report */ true);
        }
    }

    private class SafeBrowsingProceedClient extends MockWebViewClient {
        private WebResourceRequest mOnSafeBrowsingHitRequest;
        private int mOnSafeBrowsingHitThreatType;

        public WebResourceRequest getOnSafeBrowsingHitRequest() {
            return mOnSafeBrowsingHitRequest;
        }

        public int getOnSafeBrowsingHitThreatType() {
            return mOnSafeBrowsingHitThreatType;
        }

        @Override
        public void onSafeBrowsingHit(WebView view, WebResourceRequest request,
                int threatType, SafeBrowsingResponse response) {
            // Proceed through Safe Browsing warnings
            mOnSafeBrowsingHitRequest = request;
            mOnSafeBrowsingHitThreatType = threatType;
            response.proceed(/* report */ true);
        }
    }
}
