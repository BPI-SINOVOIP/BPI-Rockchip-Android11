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
package com.android.tradefed.device.cloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import com.android.tradefed.device.cloud.AcloudConfigParser.AcloudKeys;
import com.android.tradefed.util.FileUtil;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

/** Unit tests for {@link AcloudConfigParser}. */
@RunWith(JUnit4.class)
public class AcloudConfigParserTest {

    private static final String FAKE_CONFIG =
            "service_account_name: \"someaccount@developer.gserviceaccount.com\"\n"
                    + "service_account_private_key_path: \"/some/key.p12\"\n"
                    + "project: \"android-treehugger\"\n"
                    + "zone: \"us-central1-c\"\n"
                    + "machine_type: \"n1-standard-2\"\n"
                    + "network: \"default\"\n"
                    + "ssh_private_key_path: \"\"\n"
                    + "storage_bucket_name: \"android-artifacts-cache\"\n"
                    + "orientation: \"portrait\"\n"
                    + "resolution: \"1080x1920x32x240\"\n"
                    + "extra_data_disk_size_gb: 4\n";

    /** Test that the acloud configuration file is properly parsed and values are available. */
    @Test
    public void testParse() throws Exception {
        File tmpConfig = FileUtil.createTempFile("acloud-fake-config", ".config");
        try {
            FileUtil.writeToFile(FAKE_CONFIG, tmpConfig);
            AcloudConfigParser res = AcloudConfigParser.parseConfig(tmpConfig);
            assertNotNull(res);
            assertEquals("us-central1-c", res.getValueForKey(AcloudKeys.ZONE));
            assertEquals("android-treehugger", res.getValueForKey(AcloudKeys.PROJECT));
            assertEquals(
                    "someaccount@developer.gserviceaccount.com",
                    res.getValueForKey(AcloudKeys.SERVICE_ACCOUNT_NAME));
            assertEquals(
                    "/some/key.p12", res.getValueForKey(AcloudKeys.SERVICE_ACCOUNT_PRIVATE_KEY));
        } finally {
            FileUtil.deleteFile(tmpConfig);
        }
    }

    /** Test that if we fail to read the acloud config file we return null. */
    @Test
    public void testFailParse() throws Exception {
        AcloudConfigParser res = AcloudConfigParser.parseConfig(new File("doesnotexistsfile"));
        assertNull(res);
    }
}
