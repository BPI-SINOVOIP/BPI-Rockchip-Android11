/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.net.cts;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.net.SSLCertificateSocketFactory;
import android.platform.test.annotations.AppModeFull;
import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketAddress;
import java.net.UnknownHostException;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;
import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLPeerUnverifiedException;
import javax.net.ssl.SSLSession;
import libcore.javax.net.ssl.SSLConfigurationAsserts;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class SSLCertificateSocketFactoryTest {
    // TEST_HOST should point to a web server with a valid TLS certificate.
    private static final String TEST_HOST = "www.google.com";
    private static final int HTTPS_PORT = 443;
    private HostnameVerifier mDefaultVerifier;
    private SSLCertificateSocketFactory mSocketFactory;
    private InetAddress mLocalAddress;
    // InetAddress obtained by resolving TEST_HOST.
    private InetAddress mTestHostAddress;
    // SocketAddress combining mTestHostAddress and HTTPS_PORT.
    private List<SocketAddress> mTestSocketAddresses;

    @Before
    public void setUp() {
        // Expected state before each test method is that
        // HttpsURLConnection.getDefaultHostnameVerifier() will return the system default.
        mDefaultVerifier = HttpsURLConnection.getDefaultHostnameVerifier();
        mSocketFactory = (SSLCertificateSocketFactory)
            SSLCertificateSocketFactory.getDefault(1000 /* handshakeTimeoutMillis */);
        assertNotNull(mSocketFactory);
        InetAddress[] addresses;
        try {
            addresses = InetAddress.getAllByName(TEST_HOST);
            mTestHostAddress = addresses[0];
        } catch (UnknownHostException uhe) {
            throw new AssertionError(
                "Unable to test SSLCertificateSocketFactory: cannot resolve " + TEST_HOST, uhe);
        }

        mTestSocketAddresses = Arrays.stream(addresses)
            .map(addr -> new InetSocketAddress(addr, HTTPS_PORT))
            .collect(Collectors.toList());

        // Find the local IP address which will be used to connect to TEST_HOST.
        try {
            Socket testSocket = new Socket(TEST_HOST, HTTPS_PORT);
            mLocalAddress = testSocket.getLocalAddress();
            testSocket.close();
        } catch (IOException ioe) {
            throw new AssertionError(""
                + "Unable to test SSLCertificateSocketFactory: cannot connect to "
                + TEST_HOST, ioe);
        }
    }

    // Restore the system default hostname verifier after each test.
    @After
    public void restoreDefaultHostnameVerifier() {
        HttpsURLConnection.setDefaultHostnameVerifier(mDefaultVerifier);
    }

    @Test
    public void testDefaultConfiguration() throws Exception {
        SSLConfigurationAsserts.assertSSLSocketFactoryDefaultConfiguration(mSocketFactory);
    }

    @Test
    public void testAccessProperties() {
        mSocketFactory.getSupportedCipherSuites();
        mSocketFactory.getDefaultCipherSuites();
    }

    /**
     * Tests the {@code createSocket()} cases which are expected to fail with {@code IOException}.
     */
    @Test
    @AppModeFull(reason = "Socket cannot bind in instant app mode")
    public void createSocket_io_error_expected() {
        // Connect to the localhost HTTPS port. Should result in connection refused IOException
        // because no service should be listening on that port.
        InetAddress localhostAddress = InetAddress.getLoopbackAddress();
        try {
            mSocketFactory.createSocket(localhostAddress, HTTPS_PORT);
            fail();
        } catch (IOException e) {
            // expected
        }

        // Same, but also binding to a local address.
        try {
            mSocketFactory.createSocket(localhostAddress, HTTPS_PORT, localhostAddress, 0);
            fail();
        } catch (IOException e) {
            // expected
        }

        // Same, wrapping an existing plain socket which is in an unconnected state.
        try {
            Socket socket = new Socket();
            mSocketFactory.createSocket(socket, "localhost", HTTPS_PORT, true);
            fail();
        } catch (IOException e) {
            // expected
        }
    }

    /**
     * Tests hostname verification for
     * {@link SSLCertificateSocketFactory#createSocket(String, int)}.
     *
     * <p>This method should return a socket which is fully connected (i.e. TLS handshake complete)
     * and whose peer TLS certificate has been verified to have the correct hostname.
     *
     * <p>{@link SSLCertificateSocketFactory} is documented to verify hostnames using
     * the {@link HostnameVerifier} returned by
     * {@link HttpsURLConnection#getDefaultHostnameVerifier}, so this test connects twice,
     * once with the system default {@link HostnameVerifier} which is expected to succeed,
     * and once after installing a {@link NegativeHostnameVerifier} which will cause
     * {@link SSLCertificateSocketFactory#verifyHostname} to throw a
     * {@link SSLPeerUnverifiedException}.
     *
     * <p>These tests only test the hostname verification logic in SSLCertificateSocketFactory,
     * other TLS failure modes and the default HostnameVerifier are tested elsewhere, see
     * {@link com.squareup.okhttp.internal.tls.HostnameVerifierTest} and
     * https://android.googlesource.com/platform/external/boringssl/+/refs/heads/master/src/ssl/test
     *
     * <p>Tests the following behaviour:-
     * <ul>
     * <li>TEST_SERVER is available and has a valid TLS certificate
     * <li>{@code createSocket()} verifies the remote hostname is correct using
     *     {@link HttpsURLConnection#getDefaultHostnameVerifier}
     * <li>{@link SSLPeerUnverifiedException} is thrown when the remote hostname is invalid
     * </ul>
     *
     * <p>See also http://b/2807618.
     */
    @Test
    public void createSocket_simple_with_hostname_verification() throws Exception {
        Socket socket = mSocketFactory.createSocket(TEST_HOST, HTTPS_PORT);
        assertConnectedSocket(socket);
        socket.close();

        HttpsURLConnection.setDefaultHostnameVerifier(new NegativeHostnameVerifier());
        try {
            mSocketFactory.createSocket(TEST_HOST, HTTPS_PORT);
            fail();
        } catch (SSLPeerUnverifiedException expected) {
            // expected
        }
    }

    /**
     * Tests hostname verification for
     * {@link SSLCertificateSocketFactory#createSocket(Socket, String, int, boolean)}.
     *
     * <p>This method should return a socket which is fully connected (i.e. TLS handshake complete)
     * and whose peer TLS certificate has been verified to have the correct hostname.
     *
     * <p>The TLS socket returned is wrapped around the plain socket passed into
     * {@code createSocket()}.
     *
     * <p>See {@link #createSocket_simple_with_hostname_verification()} for test methodology.
     */
    @Test
    public void createSocket_wrapped_with_hostname_verification() throws Exception {
        Socket underlying = new Socket(TEST_HOST, HTTPS_PORT);
        Socket socket = mSocketFactory.createSocket(underlying, TEST_HOST, HTTPS_PORT, true);
        assertConnectedSocket(socket);
        socket.close();

        HttpsURLConnection.setDefaultHostnameVerifier(new NegativeHostnameVerifier());
        try {
            underlying = new Socket(TEST_HOST, HTTPS_PORT);
            mSocketFactory.createSocket(underlying, TEST_HOST, HTTPS_PORT, true);
            fail();
        } catch (SSLPeerUnverifiedException expected) {
            // expected
        }
    }

    /**
     * Tests hostname verification for
     * {@link SSLCertificateSocketFactory#createSocket(String, int, InetAddress, int)}.
     *
     * <p>This method should return a socket which is fully connected (i.e. TLS handshake complete)
     * and whose peer TLS certificate has been verified to have the correct hostname.
     *
     * <p>The TLS socket returned is also bound to the local address determined in {@link #setUp} to
     * be used for connections to TEST_HOST, and a wildcard port.
     *
     * <p>See {@link #createSocket_simple_with_hostname_verification()} for test methodology.
     */
    @Test
    @AppModeFull(reason = "Socket cannot bind in instant app mode")
    public void createSocket_bound_with_hostname_verification() throws Exception {
        Socket socket = mSocketFactory.createSocket(TEST_HOST, HTTPS_PORT, mLocalAddress, 0);
        assertConnectedSocket(socket);
        socket.close();

        HttpsURLConnection.setDefaultHostnameVerifier(new NegativeHostnameVerifier());
        try {
            mSocketFactory.createSocket(TEST_HOST, HTTPS_PORT, mLocalAddress, 0);
            fail();
        } catch (SSLPeerUnverifiedException expected) {
            // expected
        }
    }

    /**
     * Tests hostname verification for
     * {@link SSLCertificateSocketFactory#createSocket(InetAddress, int)}.
     *
     * <p>This method should return a socket which the documentation describes as "unconnected",
     * which actually means that the socket is fully connected at the TCP layer but TLS handshaking
     * and hostname verification have not yet taken place.
     *
     * <p>Behaviour is tested by installing a {@link NegativeHostnameVerifier} and by calling
     * {@link #assertConnectedSocket} to ensure TLS handshaking but no hostname verification takes
     * place.  Next, {@link SSLCertificateSocketFactory#verifyHostname} is called to ensure
     * that hostname verification is using the {@link HostnameVerifier} returned by
     * {@link HttpsURLConnection#getDefaultHostnameVerifier} as documented.
     *
     * <p>Tests the following behaviour:-
     * <ul>
     * <li>TEST_SERVER is available and has a valid TLS certificate
     * <li>{@code createSocket()} does not verify the remote hostname
     * <li>Calling {@link SSLCertificateSocketFactory#verifyHostname} on the returned socket
     *     throws {@link SSLPeerUnverifiedException} if the remote hostname is invalid
     * </ul>
     */
    @Test
    public void createSocket_simple_no_hostname_verification() throws Exception{
        HttpsURLConnection.setDefaultHostnameVerifier(new NegativeHostnameVerifier());
        Socket socket = mSocketFactory.createSocket(mTestHostAddress, HTTPS_PORT);
        // Need to provide the expected hostname here or the TLS handshake will
        // be unable to supply SNI to the remote host.
        mSocketFactory.setHostname(socket, TEST_HOST);
        assertConnectedSocket(socket);
        try {
          SSLCertificateSocketFactory.verifyHostname(socket, TEST_HOST);
          fail();
        } catch (SSLPeerUnverifiedException expected) {
            // expected
        }
        HttpsURLConnection.setDefaultHostnameVerifier(mDefaultVerifier);
        SSLCertificateSocketFactory.verifyHostname(socket, TEST_HOST);
        socket.close();
    }

    /**
     * Tests hostname verification for
     * {@link SSLCertificateSocketFactory#createSocket(InetAddress, int, InetAddress, int)}.
     *
     * <p>This method should return a socket which the documentation describes as "unconnected",
     * which actually means that the socket is fully connected at the TCP layer but TLS handshaking
     * and hostname verification have not yet taken place.
     *
     * <p>The TLS socket returned is also bound to the local address determined in {@link #setUp} to
     * be used for connections to TEST_HOST, and a wildcard port.
     *
     * <p>See {@link #createSocket_simple_no_hostname_verification()} for test methodology.
     */
    @Test
    @AppModeFull(reason = "Socket cannot bind in instant app mode")
    public void createSocket_bound_no_hostname_verification() throws Exception{
        HttpsURLConnection.setDefaultHostnameVerifier(new NegativeHostnameVerifier());
        Socket socket =
            mSocketFactory.createSocket(mTestHostAddress, HTTPS_PORT, mLocalAddress, 0);
        // Need to provide the expected hostname here or the TLS handshake will
        // be unable to supply SNI to the peer.
        mSocketFactory.setHostname(socket, TEST_HOST);
        assertConnectedSocket(socket);
        try {
          SSLCertificateSocketFactory.verifyHostname(socket, TEST_HOST);
          fail();
        } catch (SSLPeerUnverifiedException expected) {
            // expected
        }
        HttpsURLConnection.setDefaultHostnameVerifier(mDefaultVerifier);
        SSLCertificateSocketFactory.verifyHostname(socket, TEST_HOST);
        socket.close();
    }

    /**
     * Asserts a socket is fully connected to the expected peer.
     *
     * <p>For the variants of createSocket which verify the remote hostname,
     * {@code socket} should already be fully connected.
     *
     * <p>For the non-verifying variants, retrieving the input stream will trigger a TLS handshake
     * and so may throw an exception, for example if the peer's certificate is invalid.
     *
     * <p>Does no hostname verification.
     */
    private void assertConnectedSocket(Socket socket) throws Exception {
        assertNotNull(socket);
        assertTrue(socket.isConnected());
        assertNotNull(socket.getInputStream());
        assertNotNull(socket.getOutputStream());
        assertTrue(mTestSocketAddresses.contains(socket.getRemoteSocketAddress()));
    }

    /**
     * A HostnameVerifier which always returns false to simulate a server returning a
     * certificate which does not match the expected hostname.
     */
    private static class NegativeHostnameVerifier implements HostnameVerifier {
        @Override
        public boolean verify(String hostname, SSLSession sslSession) {
            return false;
        }
    }
}
