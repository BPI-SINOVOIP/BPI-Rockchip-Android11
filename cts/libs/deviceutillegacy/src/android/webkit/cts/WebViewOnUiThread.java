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
import android.graphics.Canvas;
import android.graphics.Picture;
import android.graphics.Rect;
import android.net.Uri;
import android.net.http.SslCertificate;
import android.os.Message;
import android.print.PrintDocumentAdapter;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.webkit.CookieManager;
import android.webkit.DownloadListener;
import android.webkit.ValueCallback;
import android.webkit.WebBackForwardList;
import android.webkit.WebChromeClient;
import android.webkit.WebMessage;
import android.webkit.WebMessagePort;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebView.HitTestResult;
import android.webkit.WebView.PictureListener;
import android.webkit.WebView.VisualStateCallback;
import android.webkit.WebViewClient;
import android.webkit.WebViewRenderProcessClient;

import com.android.compatibility.common.util.PollingCheck;

import com.google.common.util.concurrent.SettableFuture;

import java.util.concurrent.Executor;

/**
 * Many tests need to run WebView code in the UI thread. This class
 * wraps a WebView so that calls are ensured to arrive on the UI thread.
 *
 * All methods may be run on either the UI thread or test thread.
 *
 * This should remain functionally equivalent to androidx.webkit.WebViewOnUiThread.
 * Modifications to this class should be reflected in that class as necessary. See
 * http://go/modifying-webview-cts.
 */
public class WebViewOnUiThread extends WebViewSyncLoader {
    /**
     * The WebView that calls will be made on.
     */
    private WebView mWebView;

    /**
     * Wraps a WebView to ensure that methods are run on the UI thread.
     *
     * A new WebViewOnUiThread should be called during setUp so as to
     * reinitialize between calls.
     *
     * @param webView The webView that the methods should call.
     */
    public WebViewOnUiThread(WebView webView) {
        super(webView);
        mWebView = webView;
    }

    public void cleanUp() {
        super.destroy();
    }

    public void setWebViewClient(final WebViewClient webViewClient) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.setWebViewClient(webViewClient);
        });
    }

    public void setWebChromeClient(final WebChromeClient webChromeClient) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.setWebChromeClient(webChromeClient);
        });
    }

    /**
     * Set the webview renderer client for {@code mWebView}, on the UI thread.
     */
    public void setWebViewRenderProcessClient(
            final WebViewRenderProcessClient webViewRenderProcessClient) {
        setWebViewRenderProcessClient(mWebView, webViewRenderProcessClient);
    }

    /**
     * Set the webview renderer client for {@code webView}, on the UI thread.
     */
    public static void setWebViewRenderProcessClient(
            final WebView webView,
            final WebViewRenderProcessClient webViewRenderProcessClient) {
        WebkitUtils.onMainThreadSync(() ->
                webView.setWebViewRenderProcessClient(webViewRenderProcessClient)
        );
    }

    /**
     * Set the webview renderer client for {@code mWebView}, on the UI thread, with callbacks
     * executed by {@code executor}
     */
    public void setWebViewRenderProcessClient(
            final Executor executor, final WebViewRenderProcessClient webViewRenderProcessClient) {
        setWebViewRenderProcessClient(mWebView, executor, webViewRenderProcessClient);
    }

    /**
     * Set the webview renderer client for {@code webView}, on the UI thread, with callbacks
     * executed by {@code executor}
     */
    public static void setWebViewRenderProcessClient(
            final WebView webView,
            final Executor executor,
            final WebViewRenderProcessClient webViewRenderProcessClient) {
        WebkitUtils.onMainThreadSync(() ->
                webView.setWebViewRenderProcessClient(executor, webViewRenderProcessClient)
        );
    }

    /**
     * Get the webview renderer client currently set on {@code mWebView}, on the UI thread.
     */
    public WebViewRenderProcessClient getWebViewRenderProcessClient() {
        return getWebViewRenderProcessClient(mWebView);
    }

    /**
     * Get the webview renderer client currently set on {@code webView}, on the UI thread.
     */
    public static WebViewRenderProcessClient getWebViewRenderProcessClient(
            final WebView webView) {
        return WebkitUtils.onMainThreadSync(() -> {
            return webView.getWebViewRenderProcessClient();
        });
    }

    public void setPictureListener(final PictureListener pictureListener) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.setPictureListener(pictureListener);
        });
    }

    public void setNetworkAvailable(final boolean available) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.setNetworkAvailable(available);
        });
    }

    public void setDownloadListener(final DownloadListener listener) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.setDownloadListener(listener);
        });
    }

    public void setBackgroundColor(final int color) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.setBackgroundColor(color);
        });
    }

    public void clearCache(final boolean includeDiskFiles) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.clearCache(includeDiskFiles);
        });
    }

    public void clearHistory() {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.clearHistory();
        });
    }

    public void requestFocus() {
        new PollingCheck(WebkitUtils.TEST_TIMEOUT_MS) {
            @Override
            protected boolean check() {
                requestFocusOnUiThread();
                return hasFocus();
            }
        }.run();
    }

    private void requestFocusOnUiThread() {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.requestFocus();
        });
    }

    private boolean hasFocus() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.hasFocus();
        });
    }

    public boolean canZoomIn() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.canZoomIn();
        });
    }

    public boolean canZoomOut() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.canZoomOut();
        });
    }

    public boolean zoomIn() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.zoomIn();
        });
    }

    public boolean zoomOut() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.zoomOut();
        });
    }

    public void zoomBy(final float zoomFactor) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.zoomBy(zoomFactor);
        });
    }

    public void setFindListener(final WebView.FindListener listener) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.setFindListener(listener);
        });
    }

    public void removeJavascriptInterface(final String interfaceName) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.removeJavascriptInterface(interfaceName);
        });
    }

    public WebMessagePort[] createWebMessageChannel() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.createWebMessageChannel();
        });
    }

    public void postWebMessage(final WebMessage message, final Uri targetOrigin) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.postWebMessage(message, targetOrigin);
        });
    }

    public void addJavascriptInterface(final Object object, final String name) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.addJavascriptInterface(object, name);
        });
    }

    public void flingScroll(final int vx, final int vy) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.flingScroll(vx, vy);
        });
    }

    public void requestFocusNodeHref(final Message hrefMsg) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.requestFocusNodeHref(hrefMsg);
        });
    }

    public void requestImageRef(final Message msg) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.requestImageRef(msg);
        });
    }

    public void setInitialScale(final int scaleInPercent) {
        WebkitUtils.onMainThreadSync(() -> {
                mWebView.setInitialScale(scaleInPercent);
        });
    }

    public void clearSslPreferences() {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.clearSslPreferences();
        });
    }

    public void clearClientCertPreferences(final Runnable onCleared) {
        WebkitUtils.onMainThreadSync(() -> {
            WebView.clearClientCertPreferences(onCleared);
        });
    }

    public void resumeTimers() {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.resumeTimers();
        });
    }

    public void findNext(final boolean forward) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.findNext(forward);
        });
    }

    public void clearMatches() {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.clearMatches();
        });
    }

    public void loadUrl(final String url) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.loadUrl(url);
        });
    }

    public void stopLoading() {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.stopLoading();
        });
    }

    /**
     * Reload the previous URL.
     */
    public void reload() {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.reload();
        });
    }

    public String getTitle() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getTitle();
        });
    }

    public WebSettings getSettings() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getSettings();
        });
    }

    public WebBackForwardList copyBackForwardList() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.copyBackForwardList();
        });
    }

    public Bitmap getFavicon() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getFavicon();
        });
    }

    public String getUrl() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getUrl();
        });
    }

    public int getProgress() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getProgress();
        });
    }

    public int getHeight() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getHeight();
        });
    }

    public int getContentHeight() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getContentHeight();
        });
    }

    public boolean pageUp(final boolean top) {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.pageUp(top);
        });
    }

    public boolean pageDown(final boolean bottom) {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.pageDown(bottom);
        });
    }

    /**
     * Post a visual state listener callback for mWebView on the UI thread.
     */
    public void postVisualStateCallback(final long requestId, final VisualStateCallback callback) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.postVisualStateCallback(requestId, callback);
        });
    }

    public int[] getLocationOnScreen() {
        final int[] location = new int[2];
        return WebkitUtils.onMainThreadSync(() -> {
            mWebView.getLocationOnScreen(location);
            return location;
        });
    }

    public float getScale() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getScale();
        });
    }

    public boolean requestFocus(final int direction,
            final Rect previouslyFocusedRect) {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.requestFocus(direction, previouslyFocusedRect);
        });
    }

    public HitTestResult getHitTestResult() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getHitTestResult();
        });
    }

    public int getScrollX() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getScrollX();
        });
    }

    public int getScrollY() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getScrollY();
        });
    }

    public final DisplayMetrics getDisplayMetrics() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getContext().getResources().getDisplayMetrics();
        });
    }

    public boolean requestChildRectangleOnScreen(final View child,
            final Rect rect,
            final boolean immediate) {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.requestChildRectangleOnScreen(child, rect,
                    immediate);
        });
    }

    public int findAll(final String find) {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.findAll(find);
        });
    }

    public Picture capturePicture() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.capturePicture();
        });
    }

    /**
     * Execute javascript synchronously, returning the result.
     */
    public String evaluateJavascriptSync(final String script) {
        final SettableFuture<String> future = SettableFuture.create();
        evaluateJavascript(script, result -> future.set(result));
        return WebkitUtils.waitForFuture(future);
    }

    public void evaluateJavascript(final String script, final ValueCallback<String> result) {
        WebkitUtils.onMainThread(() -> mWebView.evaluateJavascript(script, result));
    }

    public void saveWebArchive(final String basename, final boolean autoname,
                               final ValueCallback<String> callback) {
        WebkitUtils.onMainThreadSync(() -> {
            mWebView.saveWebArchive(basename, autoname, callback);
        });
    }

    public SslCertificate getCertificate() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.getCertificate();
        });
    }

    public WebView createWebView() {
        return WebkitUtils.onMainThreadSync(() -> {
            return new WebView(mWebView.getContext());
        });
    }

    public PrintDocumentAdapter createPrintDocumentAdapter() {
        return WebkitUtils.onMainThreadSync(() -> {
            return mWebView.createPrintDocumentAdapter();
        });
    }

    public void setLayoutHeightToMatchParent() {
        WebkitUtils.onMainThreadSync(() -> {
            ViewParent parent = mWebView.getParent();
            if (parent instanceof ViewGroup) {
                ((ViewGroup) parent).getLayoutParams().height =
                    ViewGroup.LayoutParams.MATCH_PARENT;
            }
            mWebView.getLayoutParams().height = ViewGroup.LayoutParams.MATCH_PARENT;
            mWebView.requestLayout();
        });
    }

    public void setLayoutToMatchParent() {
        WebkitUtils.onMainThreadSync(() -> {
            setMatchParent((View) mWebView.getParent());
            setMatchParent(mWebView);
            mWebView.requestLayout();
        });
    }

    public void setAcceptThirdPartyCookies(final boolean accept) {
        WebkitUtils.onMainThreadSync(() -> {
            CookieManager.getInstance().setAcceptThirdPartyCookies(mWebView, accept);
        });
    }

    public boolean acceptThirdPartyCookies() {
        return WebkitUtils.onMainThreadSync(() -> {
            return CookieManager.getInstance().acceptThirdPartyCookies(mWebView);
        });
    }

    /**
     * Accessor for underlying WebView.
     * @return The WebView being wrapped by this class.
     */
    public WebView getWebView() {
        return mWebView;
    }

    /**
     * Wait for the current state of the DOM to be ready to render on the next draw.
     */
    public void waitForDOMReadyToRender() {
        final SettableFuture<Void> future = SettableFuture.create();
        postVisualStateCallback(0, new VisualStateCallback() {
            @Override
            public void onComplete(long requestId) {
                future.set(null);
            }
        });
        WebkitUtils.waitForFuture(future);
    }

    /**
     * Capture a bitmap representation of the current WebView state.
     *
     * This synchronises so that the bitmap contents reflects the current DOM state, rather than
     * potentially capturing a previously generated frame.
     */
    public Bitmap captureBitmap() {
        getSettings().setOffscreenPreRaster(true);
        waitForDOMReadyToRender();
        return WebkitUtils.onMainThreadSync(() -> {
            Bitmap bitmap = Bitmap.createBitmap(mWebView.getWidth(), mWebView.getHeight(),
                    Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            mWebView.draw(canvas);
            return bitmap;
        });
    }

    /**
     * Set LayoutParams to MATCH_PARENT.
     *
     * @param view Target view
     */
    private void setMatchParent(View view) {
        ViewGroup.LayoutParams params = view.getLayoutParams();
        params.height = ViewGroup.LayoutParams.MATCH_PARENT;
        params.width = ViewGroup.LayoutParams.MATCH_PARENT;
        view.setLayoutParams(params);
    }
}
