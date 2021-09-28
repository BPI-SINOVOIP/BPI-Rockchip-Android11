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

import android.annotation.SuppressLint;
import android.os.Build;
import android.platform.test.annotations.AppModeFull;
import android.test.ActivityInstrumentationTestCase2;
import android.webkit.RenderProcessGoneDetail;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebViewRenderProcess;

import com.google.common.util.concurrent.SettableFuture;

import java.util.concurrent.Future;

@AppModeFull
public class WebViewRenderProcessTest extends ActivityInstrumentationTestCase2<WebViewCtsActivity> {
    private WebViewOnUiThread mOnUiThread;

    public WebViewRenderProcessTest() {
        super("com.android.cts.webkit", WebViewCtsActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        final WebViewCtsActivity activity = getActivity();
        WebView webView = activity.getWebView();
        mOnUiThread = new WebViewOnUiThread(webView);
    }

    @Override
    protected void tearDown() throws Exception {
        if (mOnUiThread != null) {
            mOnUiThread.cleanUp();
        }
        super.tearDown();
    }

    private boolean terminateRenderProcessOnUiThread(
            final WebViewRenderProcess renderer) {
        return WebkitUtils.onMainThreadSync(() -> {
            return renderer.terminate();
        });
    }

    WebViewRenderProcess getRenderProcessOnUiThread(final WebView webView) {
        return WebkitUtils.onMainThreadSync(() -> {
            return webView.getWebViewRenderProcess();
        });
    }

    private Future<WebViewRenderProcess> startAndGetRenderProcess(
            final WebView webView) throws Throwable {
        final SettableFuture<WebViewRenderProcess> future = SettableFuture.create();

        WebkitUtils.onMainThread(() -> {
            webView.setWebViewClient(new WebViewClient() {
                @Override
                public void onPageFinished(WebView view, String url) {
                    WebViewRenderProcess result = webView.getWebViewRenderProcess();
                    future.set(result);
                }
            });
            webView.loadUrl("about:blank");
        });

        return future;
    }

    Future<Boolean> catchRenderProcessTermination(final WebView webView) {
        final SettableFuture<Boolean> future = SettableFuture.create();

        WebkitUtils.onMainThread(() -> {
            webView.setWebViewClient(new WebViewClient() {
                @Override
                public boolean onRenderProcessGone(
                        WebView view,
                        RenderProcessGoneDetail detail) {
                    view.destroy();
                    future.set(true);
                    return true;
                }
            });
        });

        return future;
    }

    public void testGetWebViewRenderProcess() throws Throwable {
        final WebView webView = mOnUiThread.getWebView();
        final WebViewRenderProcess preStartRenderProcess = getRenderProcessOnUiThread(webView);

        assertNotNull(
                "Should be possible to obtain a renderer handle before the renderer has started.",
                preStartRenderProcess);
        assertFalse(
                "Should not be able to terminate an unstarted renderer.",
                terminateRenderProcessOnUiThread(preStartRenderProcess));

        final WebViewRenderProcess renderer = WebkitUtils.waitForFuture(startAndGetRenderProcess(webView));
        assertSame(
                "The pre- and post-start renderer handles should be the same object.",
                renderer, preStartRenderProcess);

        assertSame(
                "When getWebViewRender is called a second time, it should return the same object.",
                renderer, WebkitUtils.waitForFuture(startAndGetRenderProcess(webView)));

        Future<Boolean> terminationFuture = catchRenderProcessTermination(webView);
        assertTrue(
                "A started renderer should be able to be terminated.",
                terminateRenderProcessOnUiThread(renderer));
        assertTrue(
                "Terminating a renderer should result in onRenderProcessGone being called.",
                WebkitUtils.waitForFuture(terminationFuture));

        assertFalse(
                "It should not be possible to terminate a renderer that has already terminated.",
                terminateRenderProcessOnUiThread(renderer));

        WebView webView2 = mOnUiThread.createWebView();
        try {
            assertNotSame(
                    "After a renderer restart, the new renderer handle object should be different.",
                    renderer, WebkitUtils.waitForFuture(startAndGetRenderProcess(webView2)));
        } finally {
            // Ensure that we clean up webView2. webView has been destroyed by the WebViewClient
            // installed by catchRenderProcessTermination
            WebkitUtils.onMainThreadSync(() -> webView2.destroy());
        }
    }
}
