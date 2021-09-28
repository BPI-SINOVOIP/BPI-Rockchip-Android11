/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.tradefed.util;

import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.OptionSetter;

import com.google.api.client.googleapis.auth.oauth2.GoogleCredential;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;

/** Unit test for {@link GoogleApiClientUtil}. */
@RunWith(JUnit4.class)
public class GoogleApiClientUtilTest {

    private static final Collection<String> SCOPES = Collections.singleton("ascope");
    private static final String HOST_OPTION_JSON_KEY = "host-option-json-key";
    private File mRoot;
    private StubGoogleApiClientUtil mUtil;
    private File mKey;
    private File mOldValue;

    static class StubGoogleApiClientUtil extends GoogleApiClientUtil {

        List<File> mKeyFiles = new ArrayList<>();
        boolean mDefaultCredentialUsed = false;

        @Override
        GoogleCredential doCreateCredentialFromJsonKeyFile(File file, Collection<String> scopes)
                throws IOException, GeneralSecurityException {
            mKeyFiles.add(file);
            return Mockito.mock(GoogleCredential.class);
        }

        @Override
        GoogleCredential doCreateDefaultCredential(Collection<String> scopes) throws IOException {
            mDefaultCredentialUsed = true;
            return Mockito.mock(GoogleCredential.class);
        }
    }

    @BeforeClass
    public static void setUpBeforeClass() throws Exception {
        try {
            GlobalConfiguration.getInstance();
        } catch (IllegalStateException e) {
            GlobalConfiguration.createGlobalConfiguration(new String[] {});
        }
    }

    @Before
    public void setUp() throws IOException {
        mUtil = new StubGoogleApiClientUtil();
        mRoot = FileUtil.createTempDir(GoogleApiClientUtilTest.class.getName());
        mKey = new File(mRoot, "key.json");
        FileUtil.writeToFile("key", mKey);
        mOldValue =
                GlobalConfiguration.getInstance()
                        .getHostOptions()
                        .getServiceAccountJsonKeyFiles()
                        .get(HOST_OPTION_JSON_KEY);
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mRoot);
        if (mOldValue != null) {
            GlobalConfiguration.getInstance()
                    .getHostOptions()
                    .getServiceAccountJsonKeyFiles()
                    .put(HOST_OPTION_JSON_KEY, mOldValue);
        }
    }

    @Test
    public void testCreateCredential() throws Exception {
        GoogleCredential credentail = mUtil.doCreateCredential(SCOPES, mKey, null);
        Assert.assertNotNull(credentail);
        Assert.assertEquals(1, mUtil.mKeyFiles.size());
        Assert.assertEquals(mKey, mUtil.mKeyFiles.get(0));
        Assert.assertFalse(mUtil.mDefaultCredentialUsed);
    }

    @Test
    public void testCreateCredential_useHostOptions() throws Exception {
        OptionSetter optionSetter =
                new OptionSetter(GlobalConfiguration.getInstance().getHostOptions());
        optionSetter.setOptionValue(
                "host_options:service-account-json-key-file",
                HOST_OPTION_JSON_KEY,
                mKey.getAbsolutePath());
        GoogleCredential credentail = mUtil.doCreateCredential(SCOPES, null, HOST_OPTION_JSON_KEY);
        Assert.assertNotNull(credentail);
        Assert.assertEquals(1, mUtil.mKeyFiles.size());
        Assert.assertEquals(mKey, mUtil.mKeyFiles.get(0));
        Assert.assertFalse(mUtil.mDefaultCredentialUsed);
    }

    @Test
    public void testCreateCredential_useFallbackKeyFile() throws Exception {
        GoogleCredential credentail = mUtil.doCreateCredential(SCOPES, null, "not-exist-key", mKey);
        Assert.assertNotNull(credentail);
        Assert.assertEquals(1, mUtil.mKeyFiles.size());
        Assert.assertEquals(mKey, mUtil.mKeyFiles.get(0));
        Assert.assertFalse(mUtil.mDefaultCredentialUsed);
    }

    @Test
    public void testCreateCredential_useDefault() throws Exception {
        GoogleCredential credentail = mUtil.doCreateCredential(SCOPES, null, "not-exist-key");
        Assert.assertNotNull(credentail);
        Assert.assertEquals(0, mUtil.mKeyFiles.size());
        Assert.assertTrue(mUtil.mDefaultCredentialUsed);
    }
}
