/*
 * Copyright (C) 2011 The Android Open Source Project
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

import android.graphics.Bitmap;
import android.graphics.Picture;
import android.os.Looper;
import android.os.SystemClock;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebView.PictureListener;
import android.webkit.WebViewClient;

import androidx.annotation.CallSuper;

import com.android.compatibility.common.util.PollingCheck;

import junit.framework.Assert;

import java.util.Map;
import java.util.concurrent.Callable;

/**
 * Utility class to simplify tests that need to load data into a WebView and wait for completion
 * conditions.
 *
 * May be used from any thread.
 */
public class WebViewSyncLoader {
    /**
     * Set to true after onPageFinished is called.
     */
    private boolean mLoaded;

    /**
     * Set to true after onNewPicture is called. Reset when onPageStarted
     * is called.
     */
    private boolean mNewPicture;

    /**
     * The progress, in percentage, of the page load. Valid values are between
     * 0 and 100.
     */
    private int mProgress;

    /**
     * The WebView that calls will be made on.
     */
    private WebView mWebView;


    public WebViewSyncLoader(WebView webView) {
        init(webView, new WaitForLoadedClient(this), new WaitForProgressClient(this),
                new WaitForNewPicture(this));
    }

    public WebViewSyncLoader(
            WebView webView,
            WaitForLoadedClient waitForLoadedClient,
            WaitForProgressClient waitForProgressClient,
            WaitForNewPicture waitForNewPicture) {
        init(webView, waitForLoadedClient, waitForProgressClient, waitForNewPicture);
    }

    private void init(
            final WebView webView,
            final WaitForLoadedClient waitForLoadedClient,
            final WaitForProgressClient waitForProgressClient,
            final WaitForNewPicture waitForNewPicture) {
        if (!isUiThread()) {
            WebkitUtils.onMainThreadSync(() -> {
                init(webView, waitForLoadedClient, waitForProgressClient, waitForNewPicture);
            });
            return;
        }
        mWebView = webView;
        mWebView.setWebViewClient(waitForLoadedClient);
        mWebView.setWebChromeClient(waitForProgressClient);
        mWebView.setPictureListener(waitForNewPicture);
    }

    /**
     * Detach listeners from this WebView, undoing the changes made to enable sync loading.
     */
    public void detach() {
        if (!isUiThread()) {
            WebkitUtils.onMainThreadSync(this::detach);
            return;
        }
        mWebView.setWebChromeClient(null);
        mWebView.setWebViewClient(null);
        mWebView.setPictureListener(null);
        mWebView = null;
    }

    /**
     * Detach listeners, and destroy this webview.
     */
    public void destroy() {
        if (!isUiThread()) {
            WebkitUtils.onMainThreadSync(this::destroy);
            return;
        }
        WebView webView = mWebView;
        detach();
        webView.clearHistory();
        webView.clearCache(true);
        webView.destroy();
    }

    /**
     * Accessor for underlying WebView.
     * @return The WebView being wrapped by this class.
     */
    public WebView getWebView() {
        return mWebView;
    }

    /**
     * Called from WaitForNewPicture, this is used to indicate that
     * the page has been drawn.
     */
    public synchronized void onNewPicture() {
        mNewPicture = true;
        this.notifyAll();
    }

    /**
     * Called from WaitForLoadedClient, this is used to clear the picture
     * draw state so that draws before the URL begins loading don't count.
     */
    public synchronized void onPageStarted() {
        mNewPicture = false; // Earlier paints won't count.
    }

    /**
     * Called from WaitForLoadedClient, this is used to indicate that
     * the page is loaded, but not drawn yet.
     */
    public synchronized void onPageFinished() {
        mLoaded = true;
        this.notifyAll();
    }

    /**
     * Called from the WebChrome client, this sets the current progress
     * for a page.
     * @param progress The progress made so far between 0 and 100.
     */
    public synchronized void onProgressChanged(int progress) {
        mProgress = progress;
        this.notifyAll();
    }

    /**
     * Calls {@link WebView#loadUrl} on the WebView and then waits for completion.
     *
     * <p>Test fails if the load timeout elapses.
     */
    public void loadUrlAndWaitForCompletion(final String url) {
        callAndWait(() -> mWebView.loadUrl(url));
    }

    /**
     * Calls {@link WebView#loadUrl(String,Map<String,String>)} on the WebView and waits for
     * completion.
     *
     * <p>Test fails if the load timeout elapses.
     */
    public void loadUrlAndWaitForCompletion(final String url,
            final Map<String, String> extraHeaders) {
        callAndWait(() -> mWebView.loadUrl(url, extraHeaders));
    }

    /**
     * Calls {@link WebView#postUrl(String,byte[])} on the WebView and then waits for completion.
     *
     * <p>Test fails if the load timeout elapses.
     *
     * @param url The URL to load.
     * @param postData the data will be passed to "POST" request.
     */
    public void postUrlAndWaitForCompletion(final String url, final byte[] postData) {
        callAndWait(() -> mWebView.postUrl(url, postData));
    }

    /**
     * Calls {@link WebView#loadData(String,String,String)} on the WebView and then waits for
     * completion.
     *
     * <p>Test fails if the load timeout elapses.
     */
    public void loadDataAndWaitForCompletion(final String data,
            final String mimeType, final String encoding) {
        callAndWait(() -> mWebView.loadData(data, mimeType, encoding));
    }

    /**
     * Calls {@link WebView#loadDataWithBaseUrl(String,String,String,String,String)} on the WebView
     * and then waits for completion.
     *
     * <p>Test fails if the load timeout elapses.
     */
    public void loadDataWithBaseURLAndWaitForCompletion(final String baseUrl,
            final String data, final String mimeType, final String encoding,
            final String historyUrl) {
        callAndWait(
                () -> mWebView.loadDataWithBaseURL(baseUrl, data, mimeType, encoding, historyUrl));
    }

    /**
     * Reloads a page and waits for it to complete reloading. Use reload
     * if it is a form resubmission and the onFormResubmission responds
     * by telling WebView not to resubmit it.
     */
    public void reloadAndWaitForCompletion() {
        callAndWait(() -> mWebView.reload());
    }

    /**
     * Use this only when JavaScript causes a page load to wait for the
     * page load to complete. Otherwise use loadUrlAndWaitForCompletion or
     * similar functions.
     */
    public void waitForLoadCompletion() {
        waitForCriteria(WebkitUtils.TEST_TIMEOUT_MS,
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return isLoaded();
                    }
                });
        clearLoad();
    }

    private void waitForCriteria(long timeout, Callable<Boolean> doneCriteria) {
        if (isUiThread()) {
            waitOnUiThread(timeout, doneCriteria);
        } else {
            waitOnTestThread(timeout, doneCriteria);
        }
    }

    /**
     * @return Whether or not the load has finished.
     */
    private synchronized boolean isLoaded() {
        return mLoaded && mNewPicture && mProgress == 100;
    }

    /**
     * Makes a WebView call, waits for completion and then resets the
     * load state in preparation for the next load call.
     *
     * <p>This method may be called on the UI thread.
     *
     * @param call The call to make on the UI thread prior to waiting.
     */
    private void callAndWait(Runnable call) {
        Assert.assertTrue("WebViewSyncLoader.load*AndWaitForCompletion calls "
                + "may not be mixed with load* calls directly on WebView "
                + "without calling waitForLoadCompletion after the load",
                !isLoaded());
        clearLoad(); // clear any extraneous signals from a previous load.
        if (isUiThread()) {
            call.run();
        } else {
            WebkitUtils.onMainThread(call);
        }
        waitForLoadCompletion();
    }

    /**
     * Called whenever a load has been completed so that a subsequent call to
     * waitForLoadCompletion doesn't return immediately.
     */
    private synchronized void clearLoad() {
        mLoaded = false;
        mNewPicture = false;
        mProgress = 0;
    }

    /**
     * Uses a polling mechanism, while pumping messages to check when the
     * criteria is met.
     */
    private void waitOnUiThread(long timeout, final Callable<Boolean> doneCriteria) {
        new PollingCheck(timeout) {
            @Override
            protected boolean check() {
                pumpMessages();
                try {
                    return doneCriteria.call();
                } catch (Exception e) {
                    Assert.fail("Unexpected error while checking the criteria: "
                            + e.getMessage());
                    return true;
                }
            }
        }.run();
    }

    /**
     * Uses a wait/notify to check when the criteria is met.
     */
    private synchronized void waitOnTestThread(long timeout, Callable<Boolean> doneCriteria) {
        try {
            long waitEnd = SystemClock.uptimeMillis() + timeout;
            long timeRemaining = timeout;
            while (!doneCriteria.call() && timeRemaining > 0) {
                this.wait(timeRemaining);
                timeRemaining = waitEnd - SystemClock.uptimeMillis();
            }
            Assert.assertTrue("Action failed to complete before timeout", doneCriteria.call());
        } catch (InterruptedException e) {
            // We'll just drop out of the loop and fail
        } catch (Exception e) {
            Assert.fail("Unexpected error while checking the criteria: "
                    + e.getMessage());
        }
    }

    /**
     * Pumps all currently-queued messages in the UI thread and then exits.
     * This is useful to force processing while running tests in the UI thread.
     */
    private void pumpMessages() {
        class ExitLoopException extends RuntimeException {
        }

        // Force loop to exit when processing this. Loop.quit() doesn't
        // work because this is the main Loop.
        mWebView.getHandler().post(new Runnable() {
            @Override
            public void run() {
                throw new ExitLoopException(); // exit loop!
            }
        });
        try {
            // Pump messages until our message gets through.
            Looper.loop();
        } catch (ExitLoopException e) {
        }
    }

    /**
     * Returns true if the current thread is the UI thread based on the
     * Looper.
     */
    private static boolean isUiThread() {
        return (Looper.myLooper() == Looper.getMainLooper());
    }

    /**
     * A WebChromeClient used to capture the onProgressChanged for use
     * in waitFor functions. If a test must override the WebChromeClient,
     * it can derive from this class or call onProgressChanged
     * directly.
     */
    public static class WaitForProgressClient extends WebChromeClient {
        private WebViewSyncLoader mWebViewSyncLoader;

        public WaitForProgressClient(WebViewSyncLoader webViewSyncLoader) {
            mWebViewSyncLoader = webViewSyncLoader;
        }

        @Override
        @CallSuper
        public void onProgressChanged(WebView view, int newProgress) {
            super.onProgressChanged(view, newProgress);
            mWebViewSyncLoader.onProgressChanged(newProgress);
        }
    }

    /**
     * A WebViewClient that captures the onPageFinished for use in
     * waitFor functions. Using initializeWebView sets the WaitForLoadedClient
     * into the WebView. If a test needs to set a specific WebViewClient and
     * needs the waitForCompletion capability then it should derive from
     * WaitForLoadedClient or call WebViewSyncLoader.onPageFinished.
     */
    public static class WaitForLoadedClient extends WebViewClient {
        private WebViewSyncLoader mWebViewSyncLoader;

        public WaitForLoadedClient(WebViewSyncLoader webViewSyncLoader) {
            mWebViewSyncLoader = webViewSyncLoader;
        }

        @Override
        @CallSuper
        public void onPageFinished(WebView view, String url) {
            super.onPageFinished(view, url);
            mWebViewSyncLoader.onPageFinished();
        }

        @Override
        @CallSuper
        public void onPageStarted(WebView view, String url, Bitmap favicon) {
            super.onPageStarted(view, url, favicon);
            mWebViewSyncLoader.onPageStarted();
        }
    }

    /**
     * A PictureListener that captures the onNewPicture for use in
     * waitForLoadCompletion. Using initializeWebView sets the PictureListener
     * into the WebView. If a test needs to set a specific PictureListener and
     * needs the waitForCompletion capability then it should call
     * WebViewSyncLoader.onNewPicture.
     */
    public static class WaitForNewPicture implements PictureListener {
        private WebViewSyncLoader mWebViewSyncLoader;

        public WaitForNewPicture(WebViewSyncLoader webViewSyncLoader) {
            mWebViewSyncLoader = webViewSyncLoader;
        }

        @Override
        @CallSuper
        public void onNewPicture(WebView view, Picture picture) {
            mWebViewSyncLoader.onNewPicture();
        }
    }
}
