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

package android.webkit.cts;

import android.content.pm.PackageManager;
import android.os.Process;
import android.test.ActivityInstrumentationTestCase2;
import android.webkit.ServiceWorkerController;
import android.webkit.ServiceWorkerWebSettings;
import android.webkit.WebSettings;
import android.webkit.WebView;

import com.android.compatibility.common.util.NullWebViewUtils;


public class ServiceWorkerWebSettingsTest extends
        ActivityInstrumentationTestCase2<WebViewCtsActivity> {

    private ServiceWorkerWebSettings mSettings;
    private WebViewOnUiThread mOnUiThread;

    public ServiceWorkerWebSettingsTest() {
        super("android.webkit.cts", WebViewCtsActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        WebView webview = getActivity().getWebView();
        if (webview != null) {
            mOnUiThread = new WebViewOnUiThread(webview);
            mSettings = ServiceWorkerController.getInstance().getServiceWorkerWebSettings();
        }
    }

    @Override
    protected void tearDown() throws Exception {
        if (mOnUiThread != null) {
            mOnUiThread.cleanUp();
        }
        super.tearDown();
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.ServiceWorkerWebSettingsCompatTest#testCacheMode. Modifications to this test
     * should be reflected in that test as necessary. See http://go/modifying-webview-cts.
     */
    public void testCacheMode() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        int i = WebSettings.LOAD_DEFAULT;
        assertEquals(i, mSettings.getCacheMode());
        for (; i <= WebSettings.LOAD_CACHE_ONLY; i++) {
            mSettings.setCacheMode(i);
            assertEquals(i, mSettings.getCacheMode());
        }
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.ServiceWorkerWebSettingsCompatTest#testAllowContentAccess. Modifications to
     * this test should be reflected in that test as necessary. See http://go/modifying-webview-cts.
     */
    public void testAllowContentAccess() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        assertEquals(mSettings.getAllowContentAccess(), true);
        for (boolean b : new boolean[]{false, true}) {
            mSettings.setAllowContentAccess(b);
            assertEquals(b, mSettings.getAllowContentAccess());
        }
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.ServiceWorkerWebSettingsCompatTest#testAllowFileAccess. Modifications to
     * this test should be reflected in that test as necessary. See http://go/modifying-webview-cts.
     */
    public void testAllowFileAccess() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        assertEquals(mSettings.getAllowFileAccess(), true);
        for (boolean b : new boolean[]{false, true}) {
            mSettings.setAllowFileAccess(b);
            assertEquals(b, mSettings.getAllowFileAccess());
        }
    }

    /**
     * This should remain functionally equivalent to
     * androidx.webkit.ServiceWorkerWebSettingsCompatTest#testBlockNetworkLoads. Modifications to
     * this test should be reflected in that test as necessary. See http://go/modifying-webview-cts.
     */
    public void testBlockNetworkLoads() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        // Note: we cannot test this setter unless we provide the INTERNET permission, otherwise we
        // get a SecurityException when we pass 'false'.
        final boolean hasInternetPermission = true;

        assertEquals(mSettings.getBlockNetworkLoads(), !hasInternetPermission);
        for (boolean b : new boolean[]{false, true}) {
            mSettings.setBlockNetworkLoads(b);
            assertEquals(b, mSettings.getBlockNetworkLoads());
        }
    }
}
