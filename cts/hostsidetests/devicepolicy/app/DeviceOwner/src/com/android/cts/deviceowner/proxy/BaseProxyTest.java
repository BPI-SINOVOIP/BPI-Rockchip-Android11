/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.cts.deviceowner.proxy;

import java.net.Proxy;
import java.util.ArrayList;
import java.util.List;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ProxyInfo;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;
import com.android.cts.deviceowner.BaseDeviceOwnerTest;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

public class BaseProxyTest extends BaseDeviceOwnerTest {
  static final String TAG = "ProxyCtsTest";
  protected static final int TIME_OUT_SECS = 10;

  protected ProxyInfo mProxy;
  protected String mProxyHost = "localhost";

  private ProxyBroadcastReceiver mReceiver;

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    // Register a broadcast receiver and wait for the first sticky broadcast to come in.
    registerAndWaitForStickyBroadcast();

    if (isProxyStatic() || isProxyPac()) {
      // If there is a proxy set, try to clear it so we can test properly.
      // If we can't clear it then we can't run the test.
      assertTrue("No broadcast after clearing proxy.", clearProxyAndWaitForBroadcast());
      waitForClearProxySysProp();
    }
  }

  private void registerAndWaitForStickyBroadcast() throws Exception {
    mReceiver = new ProxyBroadcastReceiver();
    mContext.registerReceiver(mReceiver,
        new IntentFilter(android.net.Proxy.PROXY_CHANGE_ACTION));
    // don't assert the outcome, as there might not be a sticky broadcast yet, when this is run
    // for the first time after a reboot
    mReceiver.waitForBroadcast();
  }

  @Override
  protected void tearDown() throws Exception {
    Log.d(TAG, "tearDown");
    // Do not assert the outcome, as there will be no broadcast if clear proxy is performed while
    // the system has no proxy configured.
    clearProxyAndWaitForBroadcast();
    waitForClearProxySysProp();
    mContext.unregisterReceiver(mReceiver);
    super.tearDown();
  }

  /**
   * Set the global proxy.
   */
  protected boolean setProxyAndWaitForBroadcast(ProxyInfo proxy) throws Exception {
    Log.d(TAG, "Setting Proxy to " + proxy);
    mDevicePolicyManager.setRecommendedGlobalProxy(getWho(), proxy);
    return mReceiver.waitForBroadcast();
  }

  /**
   * Clear the global proxy.
   */
  protected boolean clearProxyAndWaitForBroadcast() throws Exception {
    return setProxyAndWaitForBroadcast(null);
  }

  protected boolean isProxyEmpty() {
    return (mProxy == null) || (TextUtils.isEmpty(mProxy.getHost())
        && (mProxy.getPacFileUrl() == Uri.EMPTY));
  }

  protected boolean isProxyStatic() {
    return (mProxy != null) && !TextUtils.isEmpty(mProxy.getHost());
  }

  private boolean isProxyPac() {
    return (mProxy != null) && (mProxy.getPacFileUrl() != Uri.EMPTY);
  }

  protected boolean isCorrectPacProxy(Uri pacUri) {
    return isProxyPac() && pacUri.equals(mProxy.getPacFileUrl());
  }

  /**
   * Check if the java proxy property is set correctly.
   */
  private boolean isProxySysPropSet() {
    String proxy = System.getProperty("http.proxyHost");
    return (proxy != null) && proxy.equals(mProxyHost);
  }

  // Since this is a java system property there is no good way to wait for
  // this other than to poll for it to be the correct value.
  protected void waitForSetProxySysProp() {
    int ct = 0;
    while (!isProxySysPropSet() && (++ct < (TIME_OUT_SECS * 10))) {
      try {
        Thread.sleep(100);
      } catch (InterruptedException e) {
      }
    }
    assertTrue("Timed out waiting for proxy to set", isProxySysPropSet());
  }

  /**
   * Check if the java proxy property is clear.
   */
  private boolean isProxySysPropClear() {
    return TextUtils.isEmpty(System.getProperty("http.proxyHost"));
  }

  // Since this is a java system property there is no good way to wait for
  // this other than to poll for it to be the correct value.
  protected void waitForClearProxySysProp() {
    int ct = 0;
    while (!isProxySysPropClear() && (++ct < (TIME_OUT_SECS * 10))) {
      try {
        Thread.sleep(100);
      } catch (InterruptedException e) {
      }
    }
    assertTrue("Timed out waiting for proxy to clear", isProxySysPropClear());
  }

  // Utility function.
  protected List<Proxy> newArrayList(Proxy... proxies) {
    List<Proxy> ret = new ArrayList<Proxy>();
    for (Proxy p : proxies) {
      ret.add(p);
    }
    return ret;
  }

  private class ProxyBroadcastReceiver extends BroadcastReceiver {
    private final Semaphore mSemaphore = new Semaphore(0);

    @Override
    public void onReceive(Context context, Intent intent) {
      mProxy = intent.getParcelableExtra(android.net.Proxy.EXTRA_PROXY_INFO);
      Log.d(TAG, "Proxy received " + mProxy);
      mSemaphore.release();
    }

    public boolean waitForBroadcast() throws Exception {
      if (mSemaphore.tryAcquire(10, TimeUnit.SECONDS)) {
        return true;
      }
      return false;
    }
  }
}
