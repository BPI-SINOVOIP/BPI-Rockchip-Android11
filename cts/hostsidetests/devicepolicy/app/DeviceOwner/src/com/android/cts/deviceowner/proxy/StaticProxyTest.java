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

import java.net.InetSocketAddress;
import java.net.Proxy;
import java.net.Proxy.Type;
import java.net.ProxySelector;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.List;

import android.net.ProxyInfo;

public class StaticProxyTest extends BaseProxyTest {
  private static final int PROXY_PORT = 8080;
  private static final String EXCL_LIST = "example.com";
  private final List<String> mExclList = new ArrayList<String>();

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    mProxyHost = "proxyhost.com";
    mProxy = null;
    mExclList.add(EXCL_LIST);
  }

  @Override
  protected void tearDown() throws Exception {
    super.tearDown();
  }

  /**
   * Verify that we get a broadcast with the correct info after
   * we set the global proxy.
   */
  public void testBroadcast() throws Exception {
    ProxyInfo proxy = ProxyInfo.buildDirectProxy(mProxyHost, PROXY_PORT, mExclList);
    assertTrue("No broadcast after setting proxy", setProxyAndWaitForBroadcast(proxy));

    assertTrue(isProxyStatic());
    assertNotNull("Broadcast must contain proxy", mProxy);
    assertEquals(ProxyInfo.buildDirectProxy(mProxyHost, PROXY_PORT, mExclList), mProxy);
  }

  /**
   * Verify that we get a clear broadcast after we clear the global
   * proxy.
   */
  public void testClearBroadcast() throws Exception {
    testBroadcast();
    assertTrue("No broadcast after clearing proxy.", clearProxyAndWaitForBroadcast());
    assertTrue(isProxyEmpty());
  }

  /**
   * Verify that the java proxy selector returns the specified
   * static proxy after the global proxy is set.
   */
  public void testProxySelector() throws Exception {
    testBroadcast();

    waitForSetProxySysProp();
    URI uri = new URI("http://somedomain.com/test/my/url");

    ProxySelector selector = ProxySelector.getDefault();
    List<Proxy> list = selector.select(uri);
    assertEquals("Proxy must be returned", newArrayList(
        new Proxy(Type.HTTP,
            InetSocketAddress.createUnresolved(mProxyHost, PROXY_PORT))),
        list);
  }

  /**
   * Verify that the excluded host gets a direct result from
   * the proxy selector.
   */
  public void testProxySelectorExclude() throws Exception {
    testBroadcast();

    waitForSetProxySysProp();
    URI uri = new URI("http://example.com/test/my/url");

    ProxySelector selector = ProxySelector.getDefault();
    List<Proxy> list = selector.select(uri);
    assertEquals("Direct must be returned", newArrayList(Proxy.NO_PROXY), list);
  }

  /**
   * Verify that without a proxy the proxy selector is returning
   * a direct result.
   */
  public void testClearProxySelector() throws URISyntaxException {
    URI uri = new URI("http://somedomain.com/test/my/url");

    ProxySelector selector = ProxySelector.getDefault();
    List<Proxy> list = selector.select(uri);
    assertEquals("Direct must be returned", newArrayList(Proxy.NO_PROXY), list);
  }

  /**
   * Verify that the java system properties are all full of the
   * correct proxy information.
   */
  public void testProxyJavaProperties() throws Exception {
    testBroadcast();

    waitForSetProxySysProp();
    assertEquals(System.getProperty("http.proxyHost"), mProxyHost);
    assertEquals(System.getProperty("https.proxyHost"), mProxyHost);
    assertEquals(System.getProperty("http.proxyPort"), PROXY_PORT + "");
    assertEquals(System.getProperty("https.proxyPort"), PROXY_PORT + "");
    assertEquals(System.getProperty("http.nonProxyHosts"), EXCL_LIST);
    assertEquals(System.getProperty("https.nonProxyHosts"), EXCL_LIST);
  }
}
