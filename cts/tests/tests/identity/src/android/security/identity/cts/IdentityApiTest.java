/*
 * Copyright 2020 The Android Open Source Project
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

package android.security.identity.cts;

import android.content.Context;
import android.hardware.biometrics.BiometricPrompt.CryptoObject;
import android.security.identity.AccessControlProfileId;
import android.security.identity.AlreadyPersonalizedException;
import android.security.identity.CipherSuiteNotSupportedException;
import android.security.identity.DocTypeNotSupportedException;
import android.security.identity.EphemeralPublicKeyNotFoundException;
import android.security.identity.IdentityCredential;
import android.security.identity.IdentityCredentialException;
import android.security.identity.IdentityCredentialStore;
import android.security.identity.InvalidReaderSignatureException;
import android.security.identity.InvalidRequestMessageException;
import android.security.identity.MessageDecryptionException;
import android.security.identity.NoAuthenticationKeyAvailableException;
import android.security.identity.SessionTranscriptMismatchException;
import android.security.identity.UnknownAuthenticationKeyException;

import androidx.test.InstrumentationRegistry;

import org.junit.Test;
import static org.junit.Assert.assertEquals;

public class IdentityApiTest {
    private static final String TAG = "IdentityApiTest";

    @Test
    public void testConstructorsAlreadyPersonalizedException()  {
        AlreadyPersonalizedException e = new AlreadyPersonalizedException("Message");
        assertEquals("Message", e.getMessage());
        assertEquals(null, e.getCause());
        Throwable t = new RuntimeException("Other Exception");
        e = new AlreadyPersonalizedException("Message 2", t);
        assertEquals("Message 2", e.getMessage());
        assertEquals(t, e.getCause());
    }

    @Test
    public void testConstructorsCipherSuiteNotSupportedException()  {
        CipherSuiteNotSupportedException e = new CipherSuiteNotSupportedException("Message");
        assertEquals("Message", e.getMessage());
        assertEquals(null, e.getCause());
        Throwable t = new RuntimeException("Other Exception");
        e = new CipherSuiteNotSupportedException("Message 2", t);
        assertEquals("Message 2", e.getMessage());
        assertEquals(t, e.getCause());
    }

    @Test
    public void testConstructorsDocTypeNotSupportedException()  {
        DocTypeNotSupportedException e = new DocTypeNotSupportedException("Message");
        assertEquals("Message", e.getMessage());
        assertEquals(null, e.getCause());
        Throwable t = new RuntimeException("Other Exception");
        e = new DocTypeNotSupportedException("Message 2", t);
        assertEquals("Message 2", e.getMessage());
        assertEquals(t, e.getCause());
    }

    @Test
    public void testConstructorsEphemeralPublicKeyNotFoundException()  {
        EphemeralPublicKeyNotFoundException e = new EphemeralPublicKeyNotFoundException("Message");
        assertEquals("Message", e.getMessage());
        assertEquals(null, e.getCause());
        Throwable t = new RuntimeException("Other Exception");
        e = new EphemeralPublicKeyNotFoundException("Message 2", t);
        assertEquals("Message 2", e.getMessage());
        assertEquals(t, e.getCause());
    }

    @Test
    public void testConstructorsIdentityCredentialException()  {
        IdentityCredentialException e = new IdentityCredentialException("Message");
        assertEquals("Message", e.getMessage());
        assertEquals(null, e.getCause());
        Throwable t = new RuntimeException("Other Exception");
        e = new IdentityCredentialException("Message 2", t);
        assertEquals("Message 2", e.getMessage());
        assertEquals(t, e.getCause());
    }

    @Test
    public void testConstructorsInvalidReaderSignatureException()  {
        InvalidReaderSignatureException e = new InvalidReaderSignatureException("Message");
        assertEquals("Message", e.getMessage());
        assertEquals(null, e.getCause());
        Throwable t = new RuntimeException("Other Exception");
        e = new InvalidReaderSignatureException("Message 2", t);
        assertEquals("Message 2", e.getMessage());
        assertEquals(t, e.getCause());
    }

    @Test
    public void testConstructorsInvalidRequestMessageException()  {
        InvalidRequestMessageException e = new InvalidRequestMessageException("Message");
        assertEquals("Message", e.getMessage());
        assertEquals(null, e.getCause());
        Throwable t = new RuntimeException("Other Exception");
        e = new InvalidRequestMessageException("Message 2", t);
        assertEquals("Message 2", e.getMessage());
        assertEquals(t, e.getCause());
    }

    @Test
    public void testConstructorsMessageDecryptionException()  {
        MessageDecryptionException e = new MessageDecryptionException("Message");
        assertEquals("Message", e.getMessage());
        assertEquals(null, e.getCause());
        Throwable t = new RuntimeException("Other Exception");
        e = new MessageDecryptionException("Message 2", t);
        assertEquals("Message 2", e.getMessage());
        assertEquals(t, e.getCause());
    }

    @Test
    public void testConstructorsNoAuthenticationKeyAvailableException()  {
        NoAuthenticationKeyAvailableException e =
                new NoAuthenticationKeyAvailableException("Message");
        assertEquals("Message", e.getMessage());
        assertEquals(null, e.getCause());
        Throwable t = new RuntimeException("Other Exception");
        e = new NoAuthenticationKeyAvailableException("Message 2", t);
        assertEquals("Message 2", e.getMessage());
        assertEquals(t, e.getCause());
    }

    @Test
    public void testConstructorsSessionTranscriptMismatchException()  {
        SessionTranscriptMismatchException e = new SessionTranscriptMismatchException("Message");
        assertEquals("Message", e.getMessage());
        assertEquals(null, e.getCause());
        Throwable t = new RuntimeException("Other Exception");
        e = new SessionTranscriptMismatchException("Message 2", t);
        assertEquals("Message 2", e.getMessage());
        assertEquals(t, e.getCause());
    }

    @Test
    public void testConstructorsUnknownAuthenticationKeyException()  {
        UnknownAuthenticationKeyException e = new UnknownAuthenticationKeyException("Message");
        assertEquals("Message", e.getMessage());
        assertEquals(null, e.getCause());
        Throwable t = new RuntimeException("Other Exception");
        e = new UnknownAuthenticationKeyException("Message 2", t);
        assertEquals("Message 2", e.getMessage());
        assertEquals(t, e.getCause());
    }

    @Test
    public void testCryptoObjectGetter()  {
        IdentityCredential i = null;
        CryptoObject c = new CryptoObject(i);
        assertEquals(c.getIdentityCredential(), i);
    }

    @Test
    public void testAccessControlProfile()  {
        AccessControlProfileId id = new AccessControlProfileId(42);
        assertEquals(id.getId(), 42);
    }

    @Test
    public void testGetDirectAccessInstance()  {
        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore directInstance =
                IdentityCredentialStore.getDirectAccessInstance(appContext);
    }
}
