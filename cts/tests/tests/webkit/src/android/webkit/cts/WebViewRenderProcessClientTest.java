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

import android.platform.test.annotations.AppModeFull;
import android.test.ActivityInstrumentationTestCase2;
import android.view.KeyEvent;
import android.webkit.JavascriptInterface;
import android.webkit.WebView;
import android.webkit.WebViewRenderProcess;
import android.webkit.WebViewRenderProcessClient;

import com.google.common.util.concurrent.SettableFuture;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicInteger;

@AppModeFull
public class WebViewRenderProcessClientTest extends ActivityInstrumentationTestCase2<WebViewCtsActivity> {
    private WebViewOnUiThread mOnUiThread;

    public WebViewRenderProcessClientTest() {
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

    private static class JSBlocker {
        // A CoundDownLatch is used here, instead of a Future, because that makes it
        // easier to support requiring variable numbers of releaseBlock() calls
        // to unblock.
        private CountDownLatch mLatch;
        private SettableFuture<Void> mIsBlocked;

        JSBlocker(int requiredReleaseCount) {
            mLatch = new CountDownLatch(requiredReleaseCount);
            mIsBlocked = SettableFuture.create();
        }

        JSBlocker() {
            this(1);
        }

        public void releaseBlock() {
            mLatch.countDown();
        }

        @JavascriptInterface
        public void block() throws Exception {
            // This blocks indefinitely (until signalled) on a background thread.
            // The actual test timeout is not determined by this wait, but by other
            // code waiting for the onRenderProcessUnresponsive() call.
            mIsBlocked.set(null);
            mLatch.await();
        }

        public void waitForBlocked() {
            WebkitUtils.waitForFuture(mIsBlocked);
        }
    }

    private void blockRenderProcess(final JSBlocker blocker) {
        WebkitUtils.onMainThreadSync(new Runnable() {
            @Override
            public void run() {
                WebView webView = mOnUiThread.getWebView();
                webView.evaluateJavascript("blocker.block();", null);
                blocker.waitForBlocked();
                // Sending an input event that does not get acknowledged will cause
                // the unresponsive renderer event to fire.
                webView.dispatchKeyEvent(
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
            }
        });
    }

    private void addJsBlockerInterface(final JSBlocker blocker) {
        WebkitUtils.onMainThreadSync(new Runnable() {
            @Override
            public void run() {
                WebView webView = mOnUiThread.getWebView();
                webView.getSettings().setJavaScriptEnabled(true);
                webView.addJavascriptInterface(blocker, "blocker");
            }
        });
    }

    private void testWebViewRenderProcessClientOnExecutor(Executor executor) throws Throwable {
        final JSBlocker blocker = new JSBlocker();
        final SettableFuture<Void> rendererUnblocked = SettableFuture.create();

        WebViewRenderProcessClient client = new WebViewRenderProcessClient() {
            @Override
            public void onRenderProcessUnresponsive(WebView view, WebViewRenderProcess renderer) {
                // Let the renderer unblock.
                blocker.releaseBlock();
            }

            @Override
            public void onRenderProcessResponsive(WebView view, WebViewRenderProcess renderer) {
                // Notify that the renderer has been unblocked.
                rendererUnblocked.set(null);
            }
        };
        if (executor == null) {
            mOnUiThread.setWebViewRenderProcessClient(client);
        } else {
            mOnUiThread.setWebViewRenderProcessClient(executor, client);
        }

        addJsBlockerInterface(blocker);
        mOnUiThread.loadUrlAndWaitForCompletion("about:blank");
        blockRenderProcess(blocker);
        WebkitUtils.waitForFuture(rendererUnblocked);
    }

    public void testWebViewRenderProcessClientWithoutExecutor() throws Throwable {
        testWebViewRenderProcessClientOnExecutor(null);
    }

    public void testWebViewRenderProcessClientWithExecutor() throws Throwable {
        final AtomicInteger executorCount = new AtomicInteger();
        testWebViewRenderProcessClientOnExecutor(new Executor() {
            @Override
            public void execute(Runnable r) {
                executorCount.incrementAndGet();
                r.run();
            }
        });
        assertEquals(2, executorCount.get());
    }

    public void testSetWebViewRenderProcessClient() throws Throwable {
        assertNull("Initially the renderer client should be null",
                mOnUiThread.getWebViewRenderProcessClient());

        final WebViewRenderProcessClient webViewRenderProcessClient = new WebViewRenderProcessClient() {
            @Override
            public void onRenderProcessUnresponsive(WebView view, WebViewRenderProcess renderer) {}

            @Override
            public void onRenderProcessResponsive(WebView view, WebViewRenderProcess renderer) {}
        };
        mOnUiThread.setWebViewRenderProcessClient(webViewRenderProcessClient);

        assertSame(
                "After the renderer client is set, getting it should return the same object",
                webViewRenderProcessClient, mOnUiThread.getWebViewRenderProcessClient());
    }
}
