/*
 * Copyright 2019 The Android Open Source Project
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
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.Matchers.lessThan;
import static org.hamcrest.Matchers.lessThanOrEqualTo;

import android.net.http.SslError;
import android.os.StrictMode;
import android.os.StrictMode.ThreadPolicy;
import android.platform.test.annotations.AppModeFull;
import android.test.ActivityInstrumentationTestCase2;
import android.webkit.SslErrorHandler;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.cts.WebViewSyncLoader.WaitForLoadedClient;

import com.android.compatibility.common.util.NullWebViewUtils;
import com.android.compatibility.common.util.PollingCheck;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/**
 *  Test WebView zooming behaviour
 */
@AppModeFull
public class WebViewZoomTest extends ActivityInstrumentationTestCase2<WebViewCtsActivity> {
    private WebView mWebView;
    private WebViewOnUiThread mOnUiThread;
    private ScaleChangedWebViewClient mWebViewClient;
    private CtsTestServer mWebServer;

    /**
     * Epsilon used in page scale value comparisons.
     */
    private static final float PAGE_SCALE_EPSILON = 0.0001f;

    public WebViewZoomTest() {
        super("com.android.cts.webkit", WebViewCtsActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        final WebViewCtsActivity activity = getActivity();
        mWebView = activity.getWebView();
        if (mWebView == null)
            return;
        mOnUiThread = new WebViewOnUiThread(mWebView);
        mOnUiThread.requestFocus();

        new PollingCheck() {
            @Override
                protected boolean check() {
                    return activity.hasWindowFocus();
            }
        }.run();

        mWebViewClient = new ScaleChangedWebViewClient();
        mOnUiThread.setWebViewClient(mWebViewClient);

        // Pinch zoom is not supported in wrap_content layouts.
        mOnUiThread.setLayoutHeightToMatchParent();

    }

    @Override
    protected void tearDown() throws Exception {
        if (mOnUiThread != null) {
            mOnUiThread.cleanUp();
        }
        if (mWebServer != null) {
            stopWebServer();
        }
        super.tearDown();
    }

    private void stopWebServer() throws Exception {
        assertNotNull(mWebServer);
        ThreadPolicy oldPolicy = StrictMode.getThreadPolicy();
        ThreadPolicy tmpPolicy = new ThreadPolicy.Builder(oldPolicy)
                .permitNetwork()
                .build();
        StrictMode.setThreadPolicy(tmpPolicy);
        mWebServer.shutdown();
        mWebServer = null;
        StrictMode.setThreadPolicy(oldPolicy);
    }

    private void setUpPage() throws Exception {
        assertFalse(mWebViewClient.onScaleChangedCalled());
        assertNull(mWebServer);
        // Pass CtsTestserver.SslMode.TRUST_ANY_CLIENT to make the server serve https URLs yet do
        // not ask client for client authentication.
        mWebServer = new CtsTestServer(getActivity(), CtsTestServer.SslMode.TRUST_ANY_CLIENT);
        mOnUiThread.loadUrlAndWaitForCompletion(
                mWebServer.getAssetUrl(TestHtmlConstants.HELLO_WORLD_URL));
        pollingCheckForCanZoomIn();
    }

    public void testZoomIn() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        setUpPage();

        assertTrue(mOnUiThread.zoomIn());
        mWebViewClient.waitForNextScaleChange();
    }

    @SuppressWarnings("deprecation")
    public void testGetZoomControls() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        WebSettings settings = mOnUiThread.getSettings();
        assertTrue(settings.supportZoom());
        assertNotNull(
                "Should be able to get zoom controls when zoom is enabled",
                WebkitUtils.onMainThreadSync(() -> { return mWebView.getZoomControls(); }));

        // disable zoom support
        settings.setSupportZoom(false);
        assertFalse(settings.supportZoom());
        assertNull(
                "Should not be able to get zoom controls when zoom is disabled",
                WebkitUtils.onMainThreadSync(() -> { return mWebView.getZoomControls(); }));
    }

    public void testInvokeZoomPicker() throws Exception {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        WebSettings settings = mOnUiThread.getSettings();
        assertTrue(settings.supportZoom());
        setUpPage();
        WebkitUtils.onMainThreadSync(() -> mWebView.invokeZoomPicker());
    }

    public void testZoom_canNotZoomInPastMaximum() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        float currScale = mOnUiThread.getScale();
        // Zoom in until maximum scale, in default increments.
        while (mOnUiThread.zoomIn()) {
            currScale = mWebViewClient.expectZoomIn(currScale);
        }

        assertFalse(mOnUiThread.zoomIn());
        assertNoScaleChange(currScale);
    }

    public void testZoom_canNotZoomOutPastMinimum() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        float currScale = mOnUiThread.getScale();
        // Zoom in until maximum scale, in default increments.
        while (mOnUiThread.zoomOut()) {
            currScale = mWebViewClient.expectZoomOut(currScale);
        }

        assertFalse(mOnUiThread.zoomOut());
        assertNoScaleChange(currScale);
    }

    public void testCanZoomWhileZoomSupportedFalse() throws Throwable {
        // setZoomSupported disables user controls, but not zooming via API
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        setUpPage();

        WebSettings settings = mOnUiThread.getSettings();
        settings.setSupportZoom(false);
        assertFalse(settings.supportZoom());

        float currScale = mOnUiThread.getScale();

        assertTrue("Zoom out should succeed although zoom support is disabled in web settings",
                mOnUiThread.zoomIn());
        currScale = mWebViewClient.expectZoomIn(currScale);

        assertTrue("Zoom out should succeed although zoom support is disabled in web settings",
                mOnUiThread.zoomOut());
        currScale = mWebViewClient.expectZoomOut(currScale);
    }

    public void testZoomByPowerOfTwoIncrements() throws Throwable {
        // setZoomSupported disables user controls, but not zooming via API
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        setUpPage();
        float currScale = mOnUiThread.getScale();

        mOnUiThread.zoomBy(1.25f); // zoom in
        currScale = mWebViewClient.expectZoomBy(currScale, 1.25f);

        mOnUiThread.zoomBy(0.75f); // zoom out
        currScale = mWebViewClient.expectZoomBy(currScale, 0.75f);
    }

    public void testZoomByNonPowerOfTwoIncrements() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        setUpPage();

        float currScale = mOnUiThread.getScale();

        // Zoom in until maximum scale, in specified increments designed so that the last zoom will
        // be less than expected.
        while (mOnUiThread.canZoomIn()) {
            mOnUiThread.zoomBy(1.7f);
            currScale = mWebViewClient.expectZoomBy(currScale, 1.7f);
        }

        // At this point, zooming in should do nothing.
        mOnUiThread.zoomBy(1.7f);
        assertNoScaleChange(currScale);

        // Zoom out until minimum scale, in specified increments designed so that the last zoom will
        // be less than requested.
        while (mOnUiThread.canZoomOut()) {
            mOnUiThread.zoomBy(0.7f);
            currScale = mWebViewClient.expectZoomBy(currScale, 0.7f);
        }

        // At this point, zooming out should do nothing.
        mOnUiThread.zoomBy(0.7f);
        assertNoScaleChange(currScale);
    }

    public void testScaleChangeCallbackMatchesGetScale() throws Throwable {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }
        assertFalse(mWebViewClient.onScaleChangedCalled());

        setUpPage();

        assertFalse(mWebViewClient.onScaleChangedCalled());
        assertTrue(mOnUiThread.zoomIn());
        ScaleChangedState state = mWebViewClient.waitForNextScaleChange();
        assertEquals(
                "Expected onScaleChanged arg 2 (new scale) to equal view.getScale()",
                state.mNewScale, mOnUiThread.getScale());
    }

    private void assertNoScaleChange(float currScale) {
        // We sleep to assert to the best of our ability
        // that a scale change does *not* happen.
        try {
            Thread.sleep(500);
            assertFalse(mWebViewClient.onScaleChangedCalled());
            assertEquals(currScale, mOnUiThread.getScale());
        } catch (InterruptedException e) {
            fail("Interrupted");
        }
    }

    private static final class ScaleChangedState {
        public float mOldScale;
        public float mNewScale;
        public boolean mCanZoomIn;
        public boolean mCanZoomOut;

        ScaleChangedState(WebView view, float oldScale, float newScale) {
            mOldScale = oldScale;
            mNewScale = newScale;
            mCanZoomIn = view.canZoomIn();
            mCanZoomOut = view.canZoomOut();
        }
    }

    private void pollingCheckForCanZoomIn() {
        new PollingCheck(WebkitUtils.TEST_TIMEOUT_MS) {
            @Override
            protected boolean check() {
                return mOnUiThread.canZoomIn();
            }
        }.run();
    }

    private final class ScaleChangedWebViewClient extends WaitForLoadedClient {
        private BlockingQueue<ScaleChangedState> mCallQueue;

        public ScaleChangedWebViewClient() {
            super(mOnUiThread);
            mCallQueue = new LinkedBlockingQueue<>();
        }

        @Override
        public void onScaleChanged(WebView view, float oldScale, float newScale) {
            super.onScaleChanged(view, oldScale, newScale);
            mCallQueue.add(new ScaleChangedState(view, oldScale, newScale));
        }

        public float expectZoomBy(float currentScale, float scaleAmount) {
            assertTrue(scaleAmount != 1.0f);

            float nextScale = currentScale * scaleAmount;
            ScaleChangedState state = waitForNextScaleChange();
            assertEquals(currentScale, state.mOldScale);


            // Zoom scale changes can come in multiple steps and the initial scale may have
            // conversion errors. Wait for the first significant scale change.
            while (Math.abs(state.mNewScale - state.mOldScale) < PAGE_SCALE_EPSILON) {
                state = waitForNextScaleChange();
            }

            // Check that we zoomed in the expected direction wrt. the current scale.
            if (scaleAmount > 1.0f) {
                assertThat(
                        "Expected new scale > current scale when zooming in",
                        state.mNewScale, greaterThan(currentScale));
            } else {
                assertThat(
                        "Expected new scale < current scale when zooming out",
                        state.mNewScale, lessThan(currentScale));
            }

            // If we hit the zoom limit, then the new scale should be between the old scale
            // and the expected new scale. Otherwise it should equal the expected new scale.
            if (Math.abs(nextScale - state.mNewScale) > PAGE_SCALE_EPSILON) {
                if (scaleAmount > 1.0f) {
                    assertFalse(state.mCanZoomIn);
                    assertThat(
                            "Expected new scale <= requested scale when zooming in past limit",
                            state.mNewScale, lessThanOrEqualTo(nextScale));
                } else {
                    assertFalse(state.mCanZoomOut);
                    assertThat(
                            "Expected new scale >= requested scale when zooming out past limit",
                            state.mNewScale, greaterThanOrEqualTo(nextScale));
                }
            }

            return state.mNewScale;
        }

        public float expectZoomOut(float currentScale) {
            ScaleChangedState state = waitForNextScaleChange();
            assertEquals(currentScale, state.mOldScale);
            assertThat(state.mNewScale, lessThan(currentScale));
            return state.mNewScale;
        }

        public float expectZoomIn(float currentScale) {
            ScaleChangedState state = waitForNextScaleChange();
            assertEquals(currentScale, state.mOldScale);
            assertThat(state.mNewScale, greaterThan(currentScale));
            return state.mNewScale;
        }

        public ScaleChangedState waitForNextScaleChange() {
            return WebkitUtils.waitForNextQueueElement(mCallQueue);
        }

        public boolean onScaleChangedCalled() {
            return mCallQueue.size() > 0;
        }

        public void onReceivedSslError(WebView view, SslErrorHandler handler, SslError error) {
            // We know the CtsTestServer gave us fake credential, so we ignore the SSL error.
            handler.proceed();
        }
    }
}
