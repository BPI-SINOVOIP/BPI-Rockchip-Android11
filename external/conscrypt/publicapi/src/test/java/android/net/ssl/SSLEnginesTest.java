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

package android.net.ssl;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.org.conscrypt.tlswire.TlsTester;
import com.android.org.conscrypt.tlswire.handshake.ClientHello;
import com.android.org.conscrypt.tlswire.handshake.HelloExtension;
import java.nio.ByteBuffer;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLEngine;
import javax.net.ssl.SSLEngineResult;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLSession;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class SSLEnginesTest {

    private static class BrokenSSLEngine extends SSLEngine {
        @Override public SSLEngineResult wrap(ByteBuffer[] byteBuffers, int i, int i1,
                ByteBuffer byteBuffer) throws SSLException { throw new AssertionError(); }
        @Override public SSLEngineResult unwrap(ByteBuffer byteBuffer, ByteBuffer[] byteBuffers,
                int i, int i1) throws SSLException { throw new AssertionError(); }
        @Override public Runnable getDelegatedTask() { throw new AssertionError(); }
        @Override public void closeInbound() throws SSLException { throw new AssertionError(); }
        @Override public boolean isInboundDone() { throw new AssertionError(); }
        @Override public void closeOutbound() { throw new AssertionError(); }
        @Override public boolean isOutboundDone() { throw new AssertionError(); }
        @Override public String[] getSupportedCipherSuites() { throw new AssertionError(); }
        @Override public String[] getEnabledCipherSuites() { throw new AssertionError(); }
        @Override public void setEnabledCipherSuites(String[] strings) { throw new AssertionError(); }
        @Override public String[] getSupportedProtocols() { throw new AssertionError(); }
        @Override public String[] getEnabledProtocols() { throw new AssertionError(); }
        @Override public void setEnabledProtocols(String[] strings) { throw new AssertionError(); }
        @Override public SSLSession getSession() { throw new AssertionError(); }
        @Override public void beginHandshake() throws SSLException { throw new AssertionError(); }
        @Override public SSLEngineResult.HandshakeStatus getHandshakeStatus() { throw new AssertionError(); }
        @Override public void setUseClientMode(boolean b) { throw new AssertionError(); }
        @Override public boolean getUseClientMode() { throw new AssertionError(); }
        @Override public void setNeedClientAuth(boolean b) { throw new AssertionError(); }
        @Override public boolean getNeedClientAuth() { throw new AssertionError(); }
        @Override public void setWantClientAuth(boolean b) { throw new AssertionError(); }
        @Override public boolean getWantClientAuth() { throw new AssertionError(); }
        @Override public void setEnableSessionCreation(boolean b) { throw new AssertionError(); }
        @Override public boolean getEnableSessionCreation() { throw new AssertionError(); }
    }

    private static final ByteBuffer EMPTY_BUFFER = ByteBuffer.allocate(0);

    @Test
    public void testIsSupported() throws Exception {
        SSLEngine e = SSLContext.getDefault().createSSLEngine();
        assertTrue(SSLEngines.isSupportedEngine(e));

        e = new BrokenSSLEngine();
        assertFalse(SSLEngines.isSupportedEngine(e));
    }

    @Test
    public void testUseSessionTickets() throws Exception {
        try {
            SSLEngines.setUseSessionTickets(new BrokenSSLEngine(), true);
            fail();
        } catch (IllegalArgumentException expected) {
        }

        SSLEngine e = SSLContext.getDefault().createSSLEngine();
        e.setUseClientMode(true);
        SSLEngines.setUseSessionTickets(e, true);

        ClientHello hello = getClientHello(e);
        assertNotNull(hello.findExtensionByType(HelloExtension.TYPE_SESSION_TICKET));

        e = SSLContext.getDefault().createSSLEngine();
        e.setUseClientMode(true);
        SSLEngines.setUseSessionTickets(e, false);

        hello = getClientHello(e);
        assertNull(hello.findExtensionByType(HelloExtension.TYPE_SESSION_TICKET));
    }

    private static ClientHello getClientHello(SSLEngine e) throws Exception {
        ByteBuffer out = ByteBuffer.allocate(64 * 1024);

        e.wrap(EMPTY_BUFFER, out);
        out.flip();
        byte[] data = new byte[out.limit()];
        out.get(data);

        return TlsTester.parseClientHello(data);
    }
}
