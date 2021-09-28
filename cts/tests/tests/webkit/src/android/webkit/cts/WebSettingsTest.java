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

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.http.SslError;
import android.os.Build;
import android.os.Message;
import android.platform.test.annotations.AppModeFull;
import android.test.ActivityInstrumentationTestCase2;
import android.util.Base64;
import android.util.Log;
import android.view.ViewGroup;
import android.webkit.ConsoleMessage;
import android.webkit.SslErrorHandler;
import android.webkit.WebChromeClient;
import android.webkit.WebIconDatabase;
import android.webkit.WebResourceResponse;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebSettings.TextSize;
import android.webkit.WebStorage;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.cts.WebViewSyncLoader.WaitForLoadedClient;
import android.webkit.cts.WebViewSyncLoader.WaitForProgressClient;

import com.android.compatibility.common.util.NullWebViewUtils;
import com.android.compatibility.common.util.PollingCheck;
import com.google.common.util.concurrent.SettableFuture;

import java.io.ByteArrayInputStream;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * Tests for {@link android.webkit.WebSettings}
 */
@AppModeFull
public class WebSettingsTest extends ActivityInstrumentationTestCase2<WebViewCtsActivity> {

    private static final int WEBVIEW_TIMEOUT = 5000;
    private static final String LOG_TAG = "WebSettingsTest";

    private final String EMPTY_IMAGE_HEIGHT = "0";
    private final String NETWORK_IMAGE_HEIGHT = "48";  // See getNetworkImageHtml()
    private final String DATA_URL_IMAGE_HTML = "<html>" +
            "<head><script>function updateTitle(){" +
            "document.title=document.getElementById('img').naturalHeight;}</script></head>" +
            "<body onload='updateTitle()'>" +
            "<img id='img' onload='updateTitle()' src='data:image/png;base64,iVBORw0KGgoAAA" +
            "ANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAAAXNSR0IArs4c6QAAAA1JREFUCB0BAgD9/wAAAAIAAc3j" +
            "0SsAAAAASUVORK5CYII=" +
            "'></body></html>";
    private final String DATA_URL_IMAGE_HEIGHT = "1";

    private WebSettings mSettings;
    private CtsTestServer mWebServer;
    private WebViewOnUiThread mOnUiThread;
    private Context mContext;

    public WebSettingsTest() {
        super("android.webkit.cts", WebViewCtsActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        WebView webview = getActivity().getWebView();
        if (webview != null) {
            mOnUiThread = new WebViewOnUiThread(webview);
            mSettings = mOnUiThread.getSettings();
        }
        mContext = getInstrumentation().getTargetContext();
    }

    @Override
    protected void tearDown() throws Exception {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
        if (mOnUiThread != null) {
            mOnUiThread.cleanUp();
        }
        super.tearDown();
    }

    /**
     * Verifies that the default user agent string follows the format defined in Android
     * compatibility definition (tokens in angle brackets are variables, tokens in square
     * brackets are optional):
     * <p/>
     * Mozilla/5.0 (Linux; Android <version>; [<devicemodel>] [Build/<buildID>]; wv)
     * AppleWebKit/<major>.<minor> (KHTML, like Gecko) Version/<major>.<minor>
     * Chrome/<major>.<minor>.<branch>.<build>[ Mobile] Safari/<major>.<minor>
     */
    public void testUserAgentString_default() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        checkUserAgentStringHelper(mSettings.getUserAgentString(), true);
    }

    /**
     * Verifies that the useragent testing regex is actually correct, because it's very complex.
     */
    public void testUserAgentStringTest() {
        // All test UAs share the same prefix and suffix; only the middle part varies.
        final String prefix = "Mozilla/5.0 (Linux; Android " + Build.VERSION.RELEASE + "; ";
        final String suffix = "wv) AppleWebKit/0.0 (KHTML, like Gecko) Version/4.0 Chrome/0.0.0.0 Safari/0.0";

        // Valid cases:
        // Both model and build present
        checkUserAgentStringHelper(prefix + Build.MODEL + " Build/" + Build.ID + "; " + suffix, true);
        // Just model
        checkUserAgentStringHelper(prefix + Build.MODEL + "; " + suffix, true);
        // Just build
        checkUserAgentStringHelper(prefix + "Build/" + Build.ID + "; " + suffix, true);
        // Neither
        checkUserAgentStringHelper(prefix + suffix, true);

        // Invalid cases:
        // No space between model and build
        checkUserAgentStringHelper(prefix + Build.MODEL + "Build/" + Build.ID + "; " + suffix, false);
        // No semicolon after model and/or build
        checkUserAgentStringHelper(prefix + Build.MODEL + " Build/" + Build.ID + suffix, false);
        checkUserAgentStringHelper(prefix + Build.MODEL + suffix, false);
        checkUserAgentStringHelper(prefix + "Build/" + Build.ID + suffix, false);
        // Double semicolon when both omitted
        checkUserAgentStringHelper(prefix + "; " + suffix, false);
    }

    /**
     * Helper function to validate that a given useragent string is or is not valid.
     */
    private void checkUserAgentStringHelper(final String useragent, boolean shouldMatch) {
        String expectedRelease;
        if ("REL".equals(Build.VERSION.CODENAME)) {
            expectedRelease = Pattern.quote(Build.VERSION.RELEASE);
        } else {
            // Non-release builds don't include real release version, be lenient.
            expectedRelease = "[^;]+";
        }

        // Build expected regex inserting the appropriate variables, as this is easier to
        // understand and get right than matching any possible useragent and comparing the
        // variables afterward.
        final String patternString =
                // Release version always has a semicolon after it:
                Pattern.quote("Mozilla/5.0 (Linux; Android ") + expectedRelease + ";" +
                // Model is optional, but if present must have a space first:
                "( " + Pattern.quote(Build.MODEL) + ")?" +
                // Build is optional, but if present must have a space first:
                "( Build/" + Pattern.quote(Build.ID) + ")?" +
                // We want a semicolon before the wv token, but we don't want to have two in a row
                // if both model and build are omitted. Lookbehind assertions ensure either:
                // - the previous character is a semicolon
                // - or the previous character is NOT a semicolon AND a semicolon is added here.
                "((?<=;)|(?<!;);)" +
                // After that we can just check for " wv)" to finish the platform section:
                Pattern.quote(" wv) ") +
                // The rest of the expression is browser tokens and is fairly simple:
                "AppleWebKit/\\d+\\.\\d+ " +
                Pattern.quote("(KHTML, like Gecko) Version/4.0 ") +
                "Chrome/\\d+\\.\\d+\\.\\d+\\.\\d+ " +
                "(Mobile )?Safari/\\d+\\.\\d+";
        final Pattern userAgentExpr = Pattern.compile(patternString);
        Matcher patternMatcher = userAgentExpr.matcher(useragent);
        if (shouldMatch) {
            assertTrue(String.format("CDD(3.4.1/C-1-3) User agent string did not match expected pattern. \n" +
                            "Expected pattern:\n%s\nActual:\n%s", patternString, useragent),
                    patternMatcher.find());
        } else {
            assertFalse(String.format("Known-bad user agent string incorrectly matched. \n" +
                            "Expected pattern:\n%s\nActual:\n%s", patternString, useragent),
                    patternMatcher.find());
        }
    }

    public void testAccessUserAgentString() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        startWebServer();
        String url = mWebServer.getUserAgentUrl();

        String defaultUserAgent = mSettings.getUserAgentString();
        assertNotNull(defaultUserAgent);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        assertEquals(defaultUserAgent, mOnUiThread.getTitle());

        // attempting to set a null string has no effect
        mSettings.setUserAgentString(null);
        assertEquals(defaultUserAgent, mSettings.getUserAgentString());
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        assertEquals(defaultUserAgent, mOnUiThread.getTitle());

        // attempting to set an empty string has no effect
        mSettings.setUserAgentString("");
        assertEquals(defaultUserAgent, mSettings.getUserAgentString());
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        assertEquals(defaultUserAgent, mOnUiThread.getTitle());

        String customUserAgent = "Cts/test";
        mSettings.setUserAgentString(customUserAgent);
        assertEquals(customUserAgent, mSettings.getUserAgentString());
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        assertEquals(customUserAgent, mOnUiThread.getTitle());
    }

    public void testAccessAllowFileAccess() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        // prepare an HTML file in the data directory we can access to test the setting.
        final String dataDirTitle = "Loaded from data dir";
        final String dataDirFile = "datadir.html";
        final String dataDirPath = mContext.getFileStreamPath(dataDirFile).getAbsolutePath();
        final String dataDirUrl = "file://" + dataDirPath;
        writeFile(dataDirFile, "<html><title>" + dataDirTitle + "</title></html>");

        assertFalse("File access should be off by default", mSettings.getAllowFileAccess());

        mSettings.setAllowFileAccess(true);
        assertTrue("Explicitly setting file access to true should work",
                mSettings.getAllowFileAccess());

        mOnUiThread.loadUrlAndWaitForCompletion(dataDirUrl);
        assertEquals("Loading files on the file system should work with file access enabled",
                dataDirTitle, mOnUiThread.getTitle());

        mSettings.setAllowFileAccess(false);
        assertFalse("Explicitly setting file access to false should work",
                mSettings.getAllowFileAccess());

        String assetUrl = TestHtmlConstants.getFileUrl(TestHtmlConstants.BR_TAG_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(assetUrl);
        assertEquals(
                "android_asset URLs should still be loaded when even with file access disabled",
                TestHtmlConstants.BR_TAG_TITLE, mOnUiThread.getTitle());

        mOnUiThread.loadUrlAndWaitForCompletion(dataDirUrl);
        assertFalse("Files on the file system should not be loaded with file access disabled",
                dataDirTitle.equals(mOnUiThread.getTitle()));
    }

    public void testAccessCacheMode_defaultValue() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertEquals(WebSettings.LOAD_DEFAULT, mSettings.getCacheMode());
    }

    private void openIconDatabase() throws InterruptedException {
        WebkitUtils.onMainThreadSync(() -> {
            // getInstance must run on the UI thread
            WebIconDatabase iconDb = WebIconDatabase.getInstance();
            String dbPath = getActivity().getFilesDir().toString() + "/icons";
            iconDb.open(dbPath);
        });
        getInstrumentation().waitForIdleSync();
        Thread.sleep(100); // Wait for open to be received on the icon db thread.
    }

    public void testAccessCacheMode_cacheElseNetwork() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        openIconDatabase();
        final IconListenerClient iconListener = new IconListenerClient();
        mOnUiThread.setWebChromeClient(iconListener);
        startWebServer();

        mSettings.setCacheMode(WebSettings.LOAD_CACHE_ELSE_NETWORK);
        assertEquals(WebSettings.LOAD_CACHE_ELSE_NETWORK, mSettings.getCacheMode());
        int initialRequestCount = mWebServer.getRequestCount();
        loadAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);
        iconListener.waitForNextIcon();
        int requestCountAfterFirstLoad = mWebServer.getRequestCount();
        assertTrue("Must fetch non-cached resource from server",
                requestCountAfterFirstLoad > initialRequestCount);
        iconListener.drainIconQueue();
        loadAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);
        iconListener.waitForNextIcon();
        int requestCountAfterSecondLoad = mWebServer.getRequestCount();
        assertEquals("Expected to use cache instead of re-fetching resource",
                requestCountAfterSecondLoad, requestCountAfterFirstLoad);
    }

    public void testAccessCacheMode_noCache() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        openIconDatabase();
        final IconListenerClient iconListener = new IconListenerClient();
        mOnUiThread.setWebChromeClient(iconListener);
        startWebServer();

        mSettings.setCacheMode(WebSettings.LOAD_NO_CACHE);
        assertEquals(WebSettings.LOAD_NO_CACHE, mSettings.getCacheMode());
        int initialRequestCount = mWebServer.getRequestCount();
        loadAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);
        iconListener.waitForNextIcon();
        int requestCountAfterFirstLoad = mWebServer.getRequestCount();
        assertTrue("Must fetch non-cached resource from server",
                requestCountAfterFirstLoad > initialRequestCount);
        iconListener.drainIconQueue();
        loadAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);
        iconListener.waitForNextIcon();
        int requestCountAfterSecondLoad = mWebServer.getRequestCount();
        assertTrue("Expected to re-fetch resource instead of caching",
                requestCountAfterSecondLoad > requestCountAfterFirstLoad);
    }

    public void testAccessCacheMode_cacheOnly() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        openIconDatabase();
        final IconListenerClient iconListener = new IconListenerClient();
        mOnUiThread.setWebChromeClient(iconListener);
        startWebServer();

        // As a precondition, get the icon in the cache.
        mSettings.setCacheMode(WebSettings.LOAD_DEFAULT);
        loadAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);
        iconListener.waitForNextIcon();

        mSettings.setCacheMode(WebSettings.LOAD_CACHE_ONLY);
        assertEquals(WebSettings.LOAD_CACHE_ONLY, mSettings.getCacheMode());
        iconListener.drainIconQueue();
        int initialRequestCount = mWebServer.getRequestCount();
        loadAssetUrl(TestHtmlConstants.HELLO_WORLD_URL);
        iconListener.waitForNextIcon();
        int requestCountAfterFirstLoad = mWebServer.getRequestCount();
        assertEquals("Expected to use cache instead of fetching resource",
                requestCountAfterFirstLoad, initialRequestCount);
    }

    public void testAccessCursiveFontFamily() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertNotNull(mSettings.getCursiveFontFamily());

        String newCusiveFamily = "Apple Chancery";
        mSettings.setCursiveFontFamily(newCusiveFamily);
        assertEquals(newCusiveFamily, mSettings.getCursiveFontFamily());
    }

    public void testAccessFantasyFontFamily() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertNotNull(mSettings.getFantasyFontFamily());

        String newFantasyFamily = "Papyrus";
        mSettings.setFantasyFontFamily(newFantasyFamily);
        assertEquals(newFantasyFamily, mSettings.getFantasyFontFamily());
    }

    public void testAccessFixedFontFamily() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertNotNull(mSettings.getFixedFontFamily());

        String newFixedFamily = "Courier";
        mSettings.setFixedFontFamily(newFixedFamily);
        assertEquals(newFixedFamily, mSettings.getFixedFontFamily());
    }

    public void testAccessSansSerifFontFamily() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertNotNull(mSettings.getSansSerifFontFamily());

        String newFixedFamily = "Verdana";
        mSettings.setSansSerifFontFamily(newFixedFamily);
        assertEquals(newFixedFamily, mSettings.getSansSerifFontFamily());
    }

    public void testAccessSerifFontFamily() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertNotNull(mSettings.getSerifFontFamily());

        String newSerifFamily = "Times";
        mSettings.setSerifFontFamily(newSerifFamily);
        assertEquals(newSerifFamily, mSettings.getSerifFontFamily());
    }

    public void testAccessStandardFontFamily() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertNotNull(mSettings.getStandardFontFamily());

        String newStandardFamily = "Times";
        mSettings.setStandardFontFamily(newStandardFamily);
        assertEquals(newStandardFamily, mSettings.getStandardFontFamily());
    }

    public void testAccessDefaultFontSize() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        int defaultSize = mSettings.getDefaultFontSize();
        assertThat(defaultSize, greaterThan(0));

        mSettings.setDefaultFontSize(1000);
        int maxSize = mSettings.getDefaultFontSize();
        // cannot check exact size set, since the implementation caps it at an arbitrary limit
        assertThat("max size should be greater than default size",
                maxSize,
                greaterThan(defaultSize));

        mSettings.setDefaultFontSize(-10);
        int minSize = mSettings.getDefaultFontSize();
        assertThat(minSize, greaterThan(0));
        assertThat(minSize, lessThan(maxSize));

        mSettings.setDefaultFontSize(10);
        assertEquals(10, mSettings.getDefaultFontSize());
    }

    public void testAccessDefaultFixedFontSize() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        int defaultSize = mSettings.getDefaultFixedFontSize();
        assertThat(defaultSize, greaterThan(0));

        mSettings.setDefaultFixedFontSize(1000);
        int maxSize = mSettings.getDefaultFixedFontSize();
        // cannot check exact size set, since the implementation caps it at an arbitrary limit
        assertThat("max size should be greater than default size",
                maxSize,
                greaterThan(defaultSize));

        mSettings.setDefaultFixedFontSize(-10);
        int minSize = mSettings.getDefaultFixedFontSize();
        assertThat(minSize, greaterThan(0));
        assertThat(minSize, lessThan(maxSize));

        mSettings.setDefaultFixedFontSize(10);
        assertEquals(10, mSettings.getDefaultFixedFontSize());
    }

    public void testAccessDefaultTextEncodingName() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertNotNull(mSettings.getDefaultTextEncodingName());

        String newEncodingName = "iso-8859-1";
        mSettings.setDefaultTextEncodingName(newEncodingName);
        assertEquals(newEncodingName, mSettings.getDefaultTextEncodingName());
    }

    public void testAccessJavaScriptCanOpenWindowsAutomatically() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        mSettings.setJavaScriptEnabled(true);
        mSettings.setSupportMultipleWindows(true);
        startWebServer();

        final WebView childWebView = mOnUiThread.createWebView();
        final SettableFuture<Void> createWindowFuture = SettableFuture.create();
        mOnUiThread.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onCreateWindow(
                    WebView view, boolean isDialog, boolean isUserGesture, Message resultMsg) {
                WebView.WebViewTransport transport = (WebView.WebViewTransport) resultMsg.obj;
                transport.setWebView(childWebView);
                resultMsg.sendToTarget();
                createWindowFuture.set(null);
                return true;
            }
        });

        mSettings.setJavaScriptCanOpenWindowsAutomatically(false);
        assertFalse(mSettings.getJavaScriptCanOpenWindowsAutomatically());
        mOnUiThread.loadUrl(mWebServer.getAssetUrl(TestHtmlConstants.POPUP_URL));
        new PollingCheck(WEBVIEW_TIMEOUT) {
            @Override
            protected boolean check() {
                return "Popup blocked".equals(mOnUiThread.getTitle());
            }
        }.run();
        assertFalse("onCreateWindow should not have been called yet", createWindowFuture.isDone());

        mSettings.setJavaScriptCanOpenWindowsAutomatically(true);
        assertTrue(mSettings.getJavaScriptCanOpenWindowsAutomatically());
        mOnUiThread.loadUrl(mWebServer.getAssetUrl(TestHtmlConstants.POPUP_URL));
        WebkitUtils.waitForFuture(createWindowFuture);
    }

    public void testAccessJavaScriptEnabled() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        mSettings.setJavaScriptEnabled(true);
        assertTrue(mSettings.getJavaScriptEnabled());
        loadAssetUrl(TestHtmlConstants.JAVASCRIPT_URL);
        new PollingCheck(WEBVIEW_TIMEOUT) {
            @Override
            protected boolean check() {
                return "javascript on".equals(mOnUiThread.getTitle());
            }
        }.run();

        mSettings.setJavaScriptEnabled(false);
        assertFalse(mSettings.getJavaScriptEnabled());
        loadAssetUrl(TestHtmlConstants.JAVASCRIPT_URL);
        new PollingCheck(WEBVIEW_TIMEOUT) {
            @Override
            protected boolean check() {
                return "javascript off".equals(mOnUiThread.getTitle());
            }
        }.run();

    }

    public void testAccessLayoutAlgorithm() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertEquals(WebSettings.LayoutAlgorithm.NARROW_COLUMNS, mSettings.getLayoutAlgorithm());

        mSettings.setLayoutAlgorithm(WebSettings.LayoutAlgorithm.NORMAL);
        assertEquals(WebSettings.LayoutAlgorithm.NORMAL, mSettings.getLayoutAlgorithm());

        mSettings.setLayoutAlgorithm(WebSettings.LayoutAlgorithm.SINGLE_COLUMN);
        assertEquals(WebSettings.LayoutAlgorithm.SINGLE_COLUMN, mSettings.getLayoutAlgorithm());
    }

    public void testAccessMinimumFontSize() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertEquals(8, mSettings.getMinimumFontSize());

        mSettings.setMinimumFontSize(100);
        assertEquals(72, mSettings.getMinimumFontSize());

        mSettings.setMinimumFontSize(-10);
        assertEquals(1, mSettings.getMinimumFontSize());

        mSettings.setMinimumFontSize(10);
        assertEquals(10, mSettings.getMinimumFontSize());
    }

    public void testAccessMinimumLogicalFontSize() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertEquals(8, mSettings.getMinimumLogicalFontSize());

        mSettings.setMinimumLogicalFontSize(100);
        assertEquals(72, mSettings.getMinimumLogicalFontSize());

        mSettings.setMinimumLogicalFontSize(-10);
        assertEquals(1, mSettings.getMinimumLogicalFontSize());

        mSettings.setMinimumLogicalFontSize(10);
        assertEquals(10, mSettings.getMinimumLogicalFontSize());
    }

    public void testAccessPluginsEnabled() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertFalse(mSettings.getPluginsEnabled());

        mSettings.setPluginsEnabled(true);
        assertTrue(mSettings.getPluginsEnabled());
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebSettingsCompatTest#testOffscreenPreRaster. Modifications to this test
     * should be reflected in that test as necessary. See http://go/modifying-webview-cts.
     */
    public void testOffscreenPreRaster() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertFalse(mSettings.getOffscreenPreRaster());

        mSettings.setOffscreenPreRaster(true);
        assertTrue(mSettings.getOffscreenPreRaster());
    }

    public void testAccessPluginsPath() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertNotNull(mSettings.getPluginsPath());

        String pluginPath = "pluginPath";
        mSettings.setPluginsPath(pluginPath);
        assertEquals("Plugin path always empty", "", mSettings.getPluginsPath());
    }

    public void testAccessTextSize() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        mSettings.setTextSize(TextSize.NORMAL);
        assertEquals(TextSize.NORMAL, mSettings.getTextSize());

        mSettings.setTextSize(TextSize.LARGER);
        assertEquals(TextSize.LARGER, mSettings.getTextSize());

        mSettings.setTextSize(TextSize.LARGEST);
        assertEquals(TextSize.LARGEST, mSettings.getTextSize());

        mSettings.setTextSize(TextSize.SMALLER);
        assertEquals(TextSize.SMALLER, mSettings.getTextSize());

        mSettings.setTextSize(TextSize.SMALLEST);
        assertEquals(TextSize.SMALLEST, mSettings.getTextSize());
    }

    public void testAccessUseDoubleTree() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertFalse(mSettings.getUseDoubleTree());

        mSettings.setUseDoubleTree(true);
        assertFalse("setUseDoubleTree should be a no-op", mSettings.getUseDoubleTree());
    }

    public void testAccessUseWideViewPort() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertFalse(mSettings.getUseWideViewPort());

        mSettings.setUseWideViewPort(true);
        assertTrue(mSettings.getUseWideViewPort());
    }

    public void testSetNeedInitialFocus() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        mSettings.setNeedInitialFocus(false);

        mSettings.setNeedInitialFocus(true);
    }

    public void testSetRenderPriority() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        mSettings.setRenderPriority(WebSettings.RenderPriority.HIGH);

        mSettings.setRenderPriority(WebSettings.RenderPriority.LOW);

        mSettings.setRenderPriority(WebSettings.RenderPriority.NORMAL);
    }

    public void testAccessSupportMultipleWindows() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertFalse(mSettings.supportMultipleWindows());

        mSettings.setSupportMultipleWindows(true);
        assertTrue(mSettings.supportMultipleWindows());
    }

    public void testAccessSupportZoom() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertTrue(mSettings.supportZoom());

        mSettings.setSupportZoom(false);
        assertFalse(mSettings.supportZoom());
    }

    public void testAccessBuiltInZoomControls() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertFalse(mSettings.getBuiltInZoomControls());

        mSettings.setBuiltInZoomControls(true);
        assertTrue(mSettings.getBuiltInZoomControls());
    }

    public void testAppCacheDisabled() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        // Test that when AppCache is disabled, we don't get any AppCache
        // callbacks.
        startWebServer();
        final String url = mWebServer.getAppCacheUrl();
        mSettings.setJavaScriptEnabled(true);

        mOnUiThread.loadUrlAndWaitForCompletion(url);
        new PollingCheck(WEBVIEW_TIMEOUT) {
            protected boolean check() {
                return "Loaded".equals(mOnUiThread.getTitle());
            }
        }.run();
        // The page is now loaded. Wait for a further 1s to check no AppCache
        // callbacks occur.
        Thread.sleep(1000);
        assertEquals("Loaded", mOnUiThread.getTitle());
    }

    public void testAppCacheEnabled() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        // Note that the AppCache path can only be set once. This limits the
        // amount of testing we can do, and means that we must test all aspects
        // of setting the AppCache path in a single test to guarantee ordering.

        // Test that when AppCache is enabled but no valid path is provided,
        // we don't get any AppCache callbacks.
        startWebServer();
        final String url = mWebServer.getAppCacheUrl();
        mSettings.setAppCacheEnabled(true);
        mSettings.setJavaScriptEnabled(true);

        mOnUiThread.loadUrlAndWaitForCompletion(url);
        new PollingCheck(WEBVIEW_TIMEOUT) {
            @Override
            protected boolean check() {
                return "Loaded".equals(mOnUiThread.getTitle());
            }
        }.run();
        // The page is now loaded. Wait for a further 1s to check no AppCache
        // callbacks occur.
        Thread.sleep(1000);
        assertEquals("Loaded", mOnUiThread.getTitle());

        // We used to test that when AppCache is enabled and a valid path is
        // provided, we got an AppCache callback of some kind, but AppCache is
        // deprecated on the web and will be removed from Chromium in the
        // future, so this test has been removed.
    }

    // Ideally, we need a test case for the enabled case. However, it seems that
    // enabling the database should happen prior to navigating the first url due to
    // some internal limitations of webview. For this reason, we only provide a
    // test case for "disabled" behavior.
    // Also loading as data rather than using URL should work, but it causes a
    // security exception in JS, most likely due to cross domain access. So we load
    // using a URL. Finally, it looks like enabling database requires creating a
    // webChromeClient and listening to Quota callbacks, which is not documented.
    public void testDatabaseDisabled() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        // Verify that websql database does not work when disabled.
        startWebServer();

        mOnUiThread.setWebChromeClient(new WebViewSyncLoader.WaitForProgressClient(mOnUiThread) {
            @Override
            public void onExceededDatabaseQuota(String url, String databaseId, long quota,
                long estimatedSize, long total, WebStorage.QuotaUpdater updater) {
                updater.updateQuota(estimatedSize);
            }
        });
        mSettings.setJavaScriptEnabled(true);
        mSettings.setDatabaseEnabled(false);
        final String url = mWebServer.getAssetUrl(TestHtmlConstants.DATABASE_ACCESS_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        assertEquals("No database", mOnUiThread.getTitle());
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebSettingsCompatTest#testDisabledActionModeMenuItems. Modifications to this
     * test should be reflected in that test as necessary. See http://go/modifying-webview-cts.
     */
    public void testDisabledActionModeMenuItems() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        assertEquals(WebSettings.MENU_ITEM_NONE, mSettings.getDisabledActionModeMenuItems());

        int allDisabledFlags = WebSettings.MENU_ITEM_NONE | WebSettings.MENU_ITEM_SHARE |
                WebSettings.MENU_ITEM_WEB_SEARCH | WebSettings.MENU_ITEM_PROCESS_TEXT;
        for (int i = WebSettings.MENU_ITEM_NONE; i <= allDisabledFlags; i++) {
            mSettings.setDisabledActionModeMenuItems(i);
            assertEquals(i, mSettings.getDisabledActionModeMenuItems());
        }
    }

    public void testLoadsImagesAutomatically_default() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertTrue(mSettings.getLoadsImagesAutomatically());
    }

    public void testLoadsImagesAutomatically_httpImagesLoaded() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        startWebServer();
        mSettings.setJavaScriptEnabled(true);
        mSettings.setLoadsImagesAutomatically(true);

        mOnUiThread.loadDataAndWaitForCompletion(getNetworkImageHtml(), "text/html", null);
        assertEquals(NETWORK_IMAGE_HEIGHT, mOnUiThread.getTitle());
    }

    public void testLoadsImagesAutomatically_dataUriImagesLoaded() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        startWebServer();
        mSettings.setJavaScriptEnabled(true);
        mSettings.setLoadsImagesAutomatically(true);

        mOnUiThread.loadDataAndWaitForCompletion(DATA_URL_IMAGE_HTML, "text/html", null);
        assertEquals(DATA_URL_IMAGE_HEIGHT, mOnUiThread.getTitle());
    }

    public void testLoadsImagesAutomatically_blockLoadingImages() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        startWebServer();
        mSettings.setJavaScriptEnabled(true);
        mSettings.setLoadsImagesAutomatically(false);

        mOnUiThread.clearCache(true); // in case of side-effects from other tests
        mOnUiThread.loadDataAndWaitForCompletion(getNetworkImageHtml(), "text/html", null);
        assertEquals(EMPTY_IMAGE_HEIGHT, mOnUiThread.getTitle());

        mOnUiThread.loadDataAndWaitForCompletion(DATA_URL_IMAGE_HTML, "text/html", null);
        assertEquals(EMPTY_IMAGE_HEIGHT, mOnUiThread.getTitle());
    }

    public void testLoadsImagesAutomatically_loadImagesWithoutReload() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        startWebServer();
        mSettings.setJavaScriptEnabled(true);
        mSettings.setLoadsImagesAutomatically(false);

        mOnUiThread.clearCache(true); // in case of side-effects from other tests
        mOnUiThread.loadDataAndWaitForCompletion(getNetworkImageHtml(), "text/html", null);
        assertEquals(EMPTY_IMAGE_HEIGHT, mOnUiThread.getTitle());
        mSettings.setLoadsImagesAutomatically(true); // load images, without calling #reload()
        waitForNonEmptyImage();
        assertEquals(NETWORK_IMAGE_HEIGHT, mOnUiThread.getTitle());

        mSettings.setLoadsImagesAutomatically(false);
        mOnUiThread.clearCache(true);
        mOnUiThread.loadDataAndWaitForCompletion(DATA_URL_IMAGE_HTML, "text/html", null);
        assertEquals(EMPTY_IMAGE_HEIGHT, mOnUiThread.getTitle());
        mSettings.setLoadsImagesAutomatically(true); // load images, without calling #reload()
        waitForNonEmptyImage();
        assertEquals(DATA_URL_IMAGE_HEIGHT, mOnUiThread.getTitle());
    }

    public void testBlockNetworkImage() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertFalse(mSettings.getBlockNetworkImage());

        startWebServer();
        mSettings.setJavaScriptEnabled(true);

        // Check that by default network and data url images are loaded.
        mOnUiThread.loadDataAndWaitForCompletion(getNetworkImageHtml(), "text/html", null);
        assertEquals(NETWORK_IMAGE_HEIGHT, mOnUiThread.getTitle());
        mOnUiThread.loadDataAndWaitForCompletion(DATA_URL_IMAGE_HTML, "text/html", null);
        assertEquals(DATA_URL_IMAGE_HEIGHT, mOnUiThread.getTitle());

        // Check that only network images are blocked, data url images are still loaded.
        // Also check that network images are loaded automatically once we disable the setting,
        // without reloading the page.
        mSettings.setBlockNetworkImage(true);
        mOnUiThread.clearCache(true);
        mOnUiThread.loadDataAndWaitForCompletion(getNetworkImageHtml(), "text/html", null);
        assertEquals(EMPTY_IMAGE_HEIGHT, mOnUiThread.getTitle());
        mSettings.setBlockNetworkImage(false);
        waitForNonEmptyImage();
        assertEquals(NETWORK_IMAGE_HEIGHT, mOnUiThread.getTitle());

        mSettings.setBlockNetworkImage(true);
        mOnUiThread.clearCache(true);
        mOnUiThread.loadDataAndWaitForCompletion(DATA_URL_IMAGE_HTML, "text/html", null);
        assertEquals(DATA_URL_IMAGE_HEIGHT, mOnUiThread.getTitle());
    }

    public void testBlockNetworkLoads() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertFalse(mSettings.getBlockNetworkLoads());

        startWebServer();
        mSettings.setJavaScriptEnabled(true);

        // Check that by default network resources and data url images are loaded.
        mOnUiThread.loadUrlAndWaitForCompletion(
            mWebServer.getAssetUrl(TestHtmlConstants.HELLO_WORLD_URL));
        assertEquals(TestHtmlConstants.HELLO_WORLD_TITLE, mOnUiThread.getTitle());
        mOnUiThread.loadDataAndWaitForCompletion(getNetworkImageHtml(), "text/html", null);
        assertEquals(NETWORK_IMAGE_HEIGHT, mOnUiThread.getTitle());
        mOnUiThread.loadDataAndWaitForCompletion(DATA_URL_IMAGE_HTML, "text/html", null);
        assertEquals(DATA_URL_IMAGE_HEIGHT, mOnUiThread.getTitle());

        // Check that only network resources are blocked, data url images are still loaded.
        mSettings.setBlockNetworkLoads(true);
        mOnUiThread.clearCache(true);
        mOnUiThread.loadUrlAndWaitForCompletion(
            mWebServer.getAssetUrl(TestHtmlConstants.HELLO_WORLD_URL));
        assertFalse(TestHtmlConstants.HELLO_WORLD_TITLE.equals(mOnUiThread.getTitle()));
        mOnUiThread.loadDataAndWaitForCompletion(getNetworkImageHtml(), "text/html", null);
        assertEquals(EMPTY_IMAGE_HEIGHT, mOnUiThread.getTitle());
        mOnUiThread.loadDataAndWaitForCompletion(DATA_URL_IMAGE_HTML, "text/html", null);
        assertEquals(DATA_URL_IMAGE_HEIGHT, mOnUiThread.getTitle());

        // Check that network resources are loaded once we disable the setting and reload the page.
        mSettings.setBlockNetworkLoads(false);
        mOnUiThread.loadUrlAndWaitForCompletion(
            mWebServer.getAssetUrl(TestHtmlConstants.HELLO_WORLD_URL));
        assertEquals(TestHtmlConstants.HELLO_WORLD_TITLE, mOnUiThread.getTitle());
        mOnUiThread.loadDataAndWaitForCompletion(getNetworkImageHtml(), "text/html", null);
        assertEquals(NETWORK_IMAGE_HEIGHT, mOnUiThread.getTitle());
    }

    // Verify that an image in local file system can be loaded by an asset
    public void testLocalImageLoads() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        mSettings.setJavaScriptEnabled(true);
        // Check that local images are loaded without issues regardless of domain checkings
        mSettings.setAllowUniversalAccessFromFileURLs(false);
        mSettings.setAllowFileAccessFromFileURLs(false);
        String url = TestHtmlConstants.getFileUrl(TestHtmlConstants.IMAGE_ACCESS_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        waitForNonEmptyImage();
        assertEquals(NETWORK_IMAGE_HEIGHT, mOnUiThread.getTitle());
    }

    // Verify that javascript cross-domain request permissions matches file domain settings
    // for iframes
    public void testIframesWhenAccessFromFileURLsEnabled() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        mSettings.setJavaScriptEnabled(true);
        // disable universal access from files
        mSettings.setAllowUniversalAccessFromFileURLs(false);
        mSettings.setAllowFileAccessFromFileURLs(true);

        // when cross file scripting is enabled, make sure cross domain requests succeed
        String url = TestHtmlConstants.getFileUrl(TestHtmlConstants.IFRAME_ACCESS_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        String iframeUrl = TestHtmlConstants.getFileUrl(TestHtmlConstants.HELLO_WORLD_URL);
        assertEquals(iframeUrl, mOnUiThread.getTitle());
    }

    // Verify that javascript cross-domain request permissions matches file domain settings
    // for iframes
    public void testIframesWhenAccessFromFileURLsDisabled() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        mSettings.setJavaScriptEnabled(true);
        // disable universal access from files
        mSettings.setAllowUniversalAccessFromFileURLs(false);
        mSettings.setAllowFileAccessFromFileURLs(false);

        // when cross file scripting is disabled, make sure cross domain requests fail
        String url = TestHtmlConstants.getFileUrl(TestHtmlConstants.IFRAME_ACCESS_URL);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
        String iframeUrl = TestHtmlConstants.getFileUrl(TestHtmlConstants.HELLO_WORLD_URL);
        assertFalse("Title should not have changed, because file URL access is disabled",
                iframeUrl.equals(mOnUiThread.getTitle()));
    }

    // Verify that enabling file access from file URLs enable XmlHttpRequest (XHR) across files
    public void testXHRWhenAccessFromFileURLsEnabled() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        verifyFileXHR(true);
    }

    // Verify that disabling file access from file URLs disable XmlHttpRequest (XHR) accross files
    public void testXHRWhenAccessFromFileURLsDisabled() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        verifyFileXHR(false);
    }

    // verify XHR across files matches the allowFileAccessFromFileURLs setting
    private void verifyFileXHR(boolean enableXHR) throws Throwable {
        // target file content
        String target ="<html><body>target</body><html>";

        String targetPath = mContext.getFileStreamPath("target.html").getAbsolutePath();
        // local file content that use XHR to read the target file
        String local ="" +
            "<html><body><script>" +
            "var client = new XMLHttpRequest();" +
            "client.open('GET', 'file://" + targetPath + "',false);" +
            "client.send();" +
            "document.title = client.responseText;" +
            "</script></body></html>";

        // create files in internal storage
        writeFile("local.html", local);
        writeFile("target.html", target);

        mSettings.setJavaScriptEnabled(true);
        mSettings.setAllowFileAccess(true);
        // disable universal access from files
        mSettings.setAllowUniversalAccessFromFileURLs(false);
        mSettings.setAllowFileAccessFromFileURLs(enableXHR);
        String localPath = mContext.getFileStreamPath("local.html").getAbsolutePath();
        // when cross file scripting is enabled, make sure cross domain requests succeed
        mOnUiThread.loadUrlAndWaitForCompletion("file://" + localPath);
        if (enableXHR) {
            assertEquals("Expected title to change, because XHR should be enabled", target,
                    mOnUiThread.getTitle());
        } else {
            assertFalse("Title should not have changed, because XHR should be disabled",
                    target.equals(mOnUiThread.getTitle()));
        }
    }

    // Create a private file on internal storage from the given string
    private void writeFile(String filename, String content) throws Exception {

        FileOutputStream fos = mContext.openFileOutput(filename, Context.MODE_PRIVATE);
        fos.write(content.getBytes());
        fos.close();
    }

    public void testAllowMixedMode() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        final String INSECURE_BASE_URL = "http://www.example.com/";
        final String INSECURE_JS_URL = INSECURE_BASE_URL + "insecure.js";
        final String INSECURE_IMG_URL = INSECURE_BASE_URL + "insecure.png";
        final String SECURE_URL = "/secure.html";
        final String JS_HTML = "<script src=\"" + INSECURE_JS_URL + "\"></script>";
        final String IMG_HTML = "<img src=\"" + INSECURE_IMG_URL + "\" />";
        final String SECURE_HTML = "<body>" + IMG_HTML + " " + JS_HTML + "</body>";
        final String JS_CONTENT = "window.loaded_js = 42;";
        final String IMG_CONTENT = "R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";

        final class InterceptClient extends WaitForLoadedClient {
            public int mInsecureJsCounter;
            public int mInsecureImgCounter;

            public InterceptClient() {
                super(mOnUiThread);
            }

            @Override
            public void onReceivedSslError(WebView view, SslErrorHandler handler, SslError error) {
                handler.proceed();
            }

            @Override
            public WebResourceResponse shouldInterceptRequest(
                    WebView view, WebResourceRequest request) {
                if (request.getUrl().toString().equals(INSECURE_JS_URL)) {
                    mInsecureJsCounter++;
                    return new WebResourceResponse("text/javascript", "utf-8",
                        new ByteArrayInputStream(JS_CONTENT.getBytes(StandardCharsets.UTF_8)));
                } else if (request.getUrl().toString().equals(INSECURE_IMG_URL)) {
                    mInsecureImgCounter++;
                    return new WebResourceResponse("image/gif", "utf-8",
                        new ByteArrayInputStream(Base64.decode(IMG_CONTENT, Base64.DEFAULT)));
                }

                if (request.getUrl().toString().startsWith(INSECURE_BASE_URL)) {
                    return new WebResourceResponse("text/html", "UTF-8", null);
                }
                return null;
            }
        }

        InterceptClient interceptClient = new InterceptClient();
        mOnUiThread.setWebViewClient(interceptClient);
        mSettings.setJavaScriptEnabled(true);
        TestWebServer httpsServer = null;
        try {
            httpsServer = new TestWebServer(true);
            String secureUrl = httpsServer.setResponse(SECURE_URL, SECURE_HTML, null);
            mOnUiThread.clearSslPreferences();

            mSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
            mOnUiThread.loadUrlAndWaitForCompletion(secureUrl);
            assertEquals(1, httpsServer.getRequestCount(SECURE_URL));
            assertEquals(0, interceptClient.mInsecureJsCounter);
            assertEquals(0, interceptClient.mInsecureImgCounter);

            mSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
            mOnUiThread.loadUrlAndWaitForCompletion(secureUrl);
            assertEquals(2, httpsServer.getRequestCount(SECURE_URL));
            assertEquals(1, interceptClient.mInsecureJsCounter);
            assertEquals(1, interceptClient.mInsecureImgCounter);

            mSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_COMPATIBILITY_MODE);
            mOnUiThread.loadUrlAndWaitForCompletion(secureUrl);
            assertEquals(3, httpsServer.getRequestCount(SECURE_URL));
            assertEquals(1, interceptClient.mInsecureJsCounter);
            assertEquals(2, interceptClient.mInsecureImgCounter);
        } finally {
            if (httpsServer != null) {
                httpsServer.shutdown();
            }
        }
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.WebSettingsCompatTest#testEnableSafeBrowsing. Modifications to this test
     * should be reflected in that test as necessary. See http://go/modifying-webview-cts.
     */
    public void testEnableSafeBrowsing() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertTrue("Safe Browsing should be enabled by default",
                mSettings.getSafeBrowsingEnabled());
        mSettings.setSafeBrowsingEnabled(false);
        assertFalse("Can disable Safe Browsing", mSettings.getSafeBrowsingEnabled());
        mSettings.setSafeBrowsingEnabled(true);
        assertTrue("Can enable Safe Browsing", mSettings.getSafeBrowsingEnabled());
    }

    private  int[] getBitmapPixels(Bitmap bitmap, int x, int y, int width, int height) {
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, x, y, width, height);
        return pixels;
    }

    private Map<Integer,Integer> getBitmapHistogram(Bitmap bitmap, int x, int y, int width, int height) {
        HashMap<Integer, Integer> histogram = new HashMap();
        for (int pixel : getBitmapPixels(bitmap, x, y, width, height)) {
            histogram.put(pixel, histogram.getOrDefault(pixel, 0) + 1);
        }
        return histogram;
    }

    public void testForceDark_default() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        assertEquals("The default force dark state should be AUTO",
                mSettings.getForceDark(), WebSettings.FORCE_DARK_AUTO);
    }

    private void setWebViewSize(int width, int height) {
        // Set the webview size to 64x64
        WebkitUtils.onMainThreadSync(() -> {
            WebView webView = mOnUiThread.getWebView();
            ViewGroup.LayoutParams params = webView.getLayoutParams();
            params.height = height;
            params.width = width;
            webView.setLayoutParams(params);
        });

    }

    public void testForceDark_rendersDark() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        setWebViewSize(64, 64);

        Map<Integer, Integer> histogram;
        Integer[] colourValues;

        // Loading about:blank into a force-dark-on webview should result in a dark background
        mSettings.setForceDark(WebSettings.FORCE_DARK_ON);
        assertEquals("Force dark should have been set to ON",
                mSettings.getForceDark(), WebSettings.FORCE_DARK_ON);

        mOnUiThread.loadUrlAndWaitForCompletion("about:blank");
        histogram = getBitmapHistogram(mOnUiThread.captureBitmap(), 0, 0, 64, 64);
        assertEquals("Bitmap should have a single colour", histogram.size(), 1);
        colourValues = histogram.keySet().toArray(new Integer[0]);
        assertThat("Bitmap colour should be dark",
                Color.luminance(colourValues[0]), lessThan(0.5f));

        // Loading about:blank into a force-dark-off webview should result in a light background
        mSettings.setForceDark(WebSettings.FORCE_DARK_OFF);
        assertEquals("Force dark should have been set to OFF",
                mSettings.getForceDark(), WebSettings.FORCE_DARK_OFF);

        mOnUiThread.loadUrlAndWaitForCompletion("about:blank");
        histogram = getBitmapHistogram(mOnUiThread.captureBitmap(), 0, 0, 64, 64);
        assertEquals("Bitmap should have a single colour", histogram.size(), 1);
        colourValues = histogram.keySet().toArray(new Integer[0]);
        assertThat("Bitmap colour should be light",
                Color.luminance(colourValues[0]), greaterThan(0.5f));
    }

    /**
     * Starts the internal web server. The server will be shut down automatically
     * during tearDown().
     *
     * @throws Exception
     */
    private void startWebServer() throws Exception {
        assertNull(mWebServer);
        mWebServer = new CtsTestServer(getActivity(), false);
    }

    /**
     * Load the given asset from the internal web server. Starts the server if
     * it is not already running.
     *
     * @param asset The name of the asset to load.
     * @throws Exception
     */
    private void loadAssetUrl(String asset) throws Exception {
        if (mWebServer == null) {
            startWebServer();
        }
        String url = mWebServer.getAssetUrl(asset);
        mOnUiThread.loadUrlAndWaitForCompletion(url);
    }

    private String getNetworkImageHtml() {
        return "<html>" +
                "<head><script>function updateTitle(){" +
                "document.title=document.getElementById('img').naturalHeight;}</script></head>" +
                "<body onload='updateTitle()'>" +
                "<img id='img' onload='updateTitle()' src='" +
                mWebServer.getAssetUrl(TestHtmlConstants.SMALL_IMG_URL) +
                "'></body></html>";
    }

    private void waitForNonEmptyImage() {
        new PollingCheck(WEBVIEW_TIMEOUT) {
            @Override
            protected boolean check() {
                return !EMPTY_IMAGE_HEIGHT.equals(mOnUiThread.getTitle());
            }
        }.run();
    }

    private class IconListenerClient extends WaitForProgressClient {
        private final BlockingQueue<Bitmap> mReceivedIconQueue = new LinkedBlockingQueue<>();

        public IconListenerClient() {
            super(mOnUiThread);
        }

        @Override
        public void onReceivedIcon(WebView view, Bitmap icon) {
            mReceivedIconQueue.add(icon);
        }

        /**
         * Exposed as a precaution, in case for some reason we get multiple calls to
         * {@link #onReceivedIcon}.
         */
        public void drainIconQueue() {
            while (mReceivedIconQueue.poll() != null) {}
        }

        public void waitForNextIcon() {
            // TODO(ntfschr): consider exposing the Bitmap, if we want to make assertions.
            WebkitUtils.waitForNextQueueElement(mReceivedIconQueue);
        }
    }
}
