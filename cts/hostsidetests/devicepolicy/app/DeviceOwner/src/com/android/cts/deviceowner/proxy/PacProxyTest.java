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

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Proxy;
import java.net.Proxy.Type;
import java.net.ProxySelector;
import java.net.Socket;
import java.net.URI;
import java.net.UnknownHostException;
import java.util.List;
import java.util.concurrent.TimeUnit;

import android.net.ProxyInfo;
import android.net.Uri;

public class PacProxyTest extends BaseProxyTest {
  private PacFileServer mPacServer;

  /**
   * PAC file that always returns DIRECT.
   */
  private static final String DIRECT_PAC = "function FindProxyForURL(url, host)" +
      "{" +
      "  return \"DIRECT\";" +
      "}";

  /**
   * PAC file that returns three proxies, the third is DIRECT.
   */
  private static final String LOCAL_PLUS_DIRECT_PAC = "function FindProxyForURL(url, host)" +
      "{" +
      "  return \"PROXY localhost:8080; PROXY localhost:8081; DIRECT \";" +
      "}";

  /**
   * PAC file that returns either proxy or DIRECT depending on the host.
   *
   * The return result is a bogus proxy that demonstrates that the input variables
   * are passed through correctly.
   */
  private static final String HOST_PAC = "function FindProxyForURL(url, host)" +
      "{" +
      "  if (host == \"testhost\") {" +
      "    return \"PROXY localhost:8080\";" +
      "  }" +
      "  return \"DIRECT\";" +
      "}";

  /**
   * PAC file that returns either proxy or DIRECT depending on the URL.
   *
   * The return result is a bogus proxy that demonstrates that the input variables
   * are passed through correctly.
   */
  private static final String URL_PAC = "function FindProxyForURL(url, host)\n" +
      "{" +
      "  if (url == \"http://localhost/my/url/\") {" +
      "    return \"PROXY localhost:8080\";" +
      "  } " +
      "  return \"DIRECT\";" +
      "}";

  /**
   * Wait for the PacFileServer to tell us it has had a successful
   * HTTP request and responded with the PAC file we set.
   */
  private void waitForPacDownload() throws InterruptedException {
    boolean success = mPacServer.awaitPacSent(30, TimeUnit.SECONDS);
    assertTrue("Took too long to set PAC", success);
  }

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    mPacServer = new PacFileServer(null);
    mPacServer.startServer();
    mProxy = null;
  }

  @Override
  protected void tearDown() throws Exception {
    try {
      mPacServer.stopServer();
    } finally {
      super.tearDown();
    }
  }

  private void setPacURLAndWaitForDownload() throws Exception {
    Uri pacProxyUri = Uri.parse(mPacServer.getPacURL());
    ProxyInfo proxy = ProxyInfo.buildPacProxy(pacProxyUri);
    assertTrue("No broadcast after setting proxy", setProxyAndWaitForBroadcast(proxy));
    assertTrue("Current Proxy " + mProxy, isCorrectPacProxy(pacProxyUri));

    waitForPacDownload();
  }

  /**
   * Make sure that the PAC file can be set and it downloads it
   * from the local HTTP server we are hosting.
   */
  public void testFileLoaded() throws Exception {
    mPacServer.setPacFile(DIRECT_PAC);
    setPacURLAndWaitForDownload();
  }

  /**
   * Make sure when we set the PAC file URL we get a broadcast
   * containing the proxy info.
   */
  public void testBroadcast() throws Exception {
    mPacServer.setPacFile(DIRECT_PAC);
    setPacURLAndWaitForDownload();

    assertNotNull("Broadcast must contain proxy", mProxy);
    assertEquals("Proxy must contain PAC URL", mPacServer.getPacURL(),
        mProxy.getPacFileUrl().toString());
  }

  /**
   * Make sure that we also get a broadcast after we clear the
   * PAC proxy settings.
   */
  public void testClearBroadcast() throws Exception {
    testBroadcast();
    assertTrue("No broadcast after clearing proxy.", clearProxyAndWaitForBroadcast());
    assertTrue(isProxyEmpty());
  }

  /**
   * Verify that the locally-hosted android backup proxy is up and
   * running.
   * Android hosts a proxy server that provides legacy support for apps that
   * don't use the apache HTTP stack, but still read the static proxy
   * settings.  The static settings point to a server that will handle
   * the PAC parsing for them.
   */
  public void testProxyIsUp() throws Exception {
    testBroadcast();

    assertNotNull("Broadcast must contain proxy", mProxy);
    try {
      Socket s = new Socket(mProxy.getHost(), mProxy.getPort());
      assertTrue("Must connect to proxy", s.isConnected());
      s.close();
    } catch (UnknownHostException e) {
      fail("Proxy contained invalid host - " + mProxy.getHost() + ":" + mProxy.getPort());
    } catch (IOException e) {
      fail("Unable to connect to proxy - " + mProxy.getHost() + ":" + mProxy.getPort());
    }
  }

  /**
   * The local legacy proxy described above needs to support HTTP CONNECT
   * requests where data should be passed through without interference.
   * This test hosts a socket and uses this functionality to connect through
   * the proxy and pass data back and forth.  See PassthroughTestHelper for
   * more details.
   */
  public void testProxyPassthrough() throws Exception {
    mPacServer.setPacFile(DIRECT_PAC);
    setPacURLAndWaitForDownload();

    waitForSetProxySysProp();

    PassthroughTestHelper ptt = new PassthroughTestHelper(mProxy);
    ptt.runTest();
  }

  /**
   * Verify that for a simple PAC that returns direct the
   * result is direct.
   */
  public void testDirect() throws Exception {
    testBroadcast();

    waitForSetProxySysProp();
    URI uri = new URI("http://localhost/test/my/url");

    ProxySelector selector = ProxySelector.getDefault();
    List<Proxy> list = selector.select(uri);
    assertEquals("Proxy must be direct", newArrayList(Proxy.NO_PROXY), list);
  }

  /**
   * Test a PAC file that returns a list of proxies, including one direct.
   */
  public void testLocalPlusDirect() throws Exception {
    mPacServer.setPacFile(LOCAL_PLUS_DIRECT_PAC);
    setPacURLAndWaitForDownload();

    waitForSetProxySysProp();

    URI uri = new URI("http://localhost/test/my/url");

    ProxySelector selector = ProxySelector.getDefault();
    List<Proxy> list = selector.select(uri);
    assertEquals("Must return multiple proxies", newArrayList(
        new Proxy(Type.HTTP, InetSocketAddress.createUnresolved("localhost", 8080)),
        new Proxy(Type.HTTP, InetSocketAddress.createUnresolved("localhost", 8081)),
        Proxy.NO_PROXY), list);
  }

  /**
   * Tests a PAC file that returns different proxies depending on
   * the host that is being accessed.
   */
  public void testHostPassthrough() throws Exception {
    mPacServer.setPacFile(HOST_PAC);
    setPacURLAndWaitForDownload();

    waitForSetProxySysProp();
    String host = "testhost";
    URI uri = new URI("http://" + host + "/test/my/url");
    ProxySelector selector = ProxySelector.getDefault();
    List<Proxy> list = selector.select(uri);

    assertEquals("Proxy must have correct port", newArrayList(
        new Proxy(Type.HTTP, InetSocketAddress.createUnresolved("localhost", 8080))),
        list);
  }

  /**
   * Tests a PAC file that returns different proxies depending on
   * the url that is being accessed.
   */
  public void testUrlPassthrough() throws Exception {
    mPacServer.setPacFile(URL_PAC);
    setPacURLAndWaitForDownload();

    waitForSetProxySysProp();

    URI uri = new URI("http://localhost/my/url/");

    ProxySelector selector = ProxySelector.getDefault();
    List<Proxy> list = selector.select(uri);
    assertEquals("Correct URL returns proxy", newArrayList(
        new Proxy(Type.HTTP, InetSocketAddress.createUnresolved("localhost", 8080))),
        list);

    uri = new URI("http://localhost/not/my/url/");
    list = selector.select(uri);
    assertEquals("Incorrect URL should return DIRECT",
        newArrayList(Proxy.NO_PROXY), list);
  }
}
