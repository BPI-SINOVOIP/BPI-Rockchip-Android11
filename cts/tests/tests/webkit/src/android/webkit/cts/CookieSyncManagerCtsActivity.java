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

import android.app.Activity;
import android.os.Bundle;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.webkit.CookieSyncManager;
import android.webkit.WebView;

import com.android.compatibility.common.util.NullWebViewUtils;

public class CookieSyncManagerCtsActivity extends Activity {
    private WebView mWebView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Only do the rest of setup if the device is supposed to have a WebView implementation.
        if (!NullWebViewUtils.isWebViewAvailable()) return;

        CookieSyncManager.createInstance(this);
        mWebView = new WebView(this);
        setContentView(mWebView);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (NullWebViewUtils.isWebViewAvailable()) {
            CookieSyncManager.getInstance().startSync();
        }
    }

    @Override
    public void onDestroy() {
        if (mWebView != null) {
            ViewParent parent =  mWebView.getParent();
            if (parent instanceof ViewGroup) {
                ((ViewGroup) parent).removeView(mWebView);
            }
            mWebView.destroy();
        }
        super.onDestroy();
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (NullWebViewUtils.isWebViewAvailable()) {
            CookieSyncManager.getInstance().stopSync();
        }
    }

    public WebView getWebView() {
        return mWebView;
    }
}
