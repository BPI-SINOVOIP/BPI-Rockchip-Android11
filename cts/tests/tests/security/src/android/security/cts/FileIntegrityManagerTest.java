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

package android.security.cts;

import android.content.Context;
import android.security.FileIntegrityManager;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.RestrictedBuildTest;
import android.platform.test.annotations.SecurityTest;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.CtsAndroidTestCase;
import com.android.compatibility.common.util.PropertyUtil;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;

@AppModeFull
@SecurityTest
public class FileIntegrityManagerTest extends CtsAndroidTestCase {

    private static final String TAG = "FileIntegrityManagerTest";
    private static final int MIN_REQUIRED_API_LEVEL = 30;

    private Context mContext;
    private FileIntegrityManager mFileIntegrityManager;
    private CertificateFactory mCertFactory;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mContext = InstrumentationRegistry.getInstrumentation().getContext();
        mFileIntegrityManager = mContext.getSystemService(FileIntegrityManager.class);
        mCertFactory = CertificateFactory.getInstance("X.509");
    }


    @CddTest(requirement="9.10/C-0-3,C-1-1")
    public void testSupportedOnDevicesFirstLaunchedWithR() throws Exception {
        if (PropertyUtil.getFirstApiLevel() >= MIN_REQUIRED_API_LEVEL) {
            assertTrue(mFileIntegrityManager.isApkVeritySupported());
        }
    }

    @CddTest(requirement="9.10/C-0-3,C-1-1")
    public void testCtsReleaseCertificateTrusted() throws Exception {
        boolean isReleaseCertTrusted = mFileIntegrityManager.isAppSourceCertificateTrusted(
                readAssetAsX509Certificate("fsverity-release.x509.der"));
        if (mFileIntegrityManager.isApkVeritySupported()) {
            assertTrue(isReleaseCertTrusted);
        } else {
            assertFalse(isReleaseCertTrusted);
        }
    }

    @CddTest(requirement="9.10/C-0-3,C-1-1")
    @RestrictedBuildTest
    public void testPlatformDebugCertificateNotTrusted() throws Exception {
        boolean isDebugCertTrusted = mFileIntegrityManager.isAppSourceCertificateTrusted(
                readAssetAsX509Certificate("fsverity-debug.x509.der"));
        assertFalse(isDebugCertTrusted);
    }

    private X509Certificate readAssetAsX509Certificate(String assetName)
            throws CertificateException, IOException {
        InputStream is = mContext.getAssets().open(assetName);
        return toX509Certificate(readAllBytes(is));
    }

    // TODO: Switch to InputStream#readAllBytes when Java 9 is supported
    private byte[] readAllBytes(InputStream is) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buf = new byte[8192];
        int len;
        while ((len = is.read(buf, 0, buf.length)) > 0) {
            output.write(buf, 0, len);
        }
        return output.toByteArray();
    }

    private X509Certificate toX509Certificate(byte[] bytes) throws CertificateException {
        return (X509Certificate) mCertFactory.generateCertificate(new ByteArrayInputStream(bytes));
    }
}
