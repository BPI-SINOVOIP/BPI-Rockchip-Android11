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
package com.android.tradefed.clearcut;

import static org.junit.Assert.assertEquals;

import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

/** Unit tests for {@link ClearcutClient}. */
@RunWith(JUnit4.class)
public class ClearcutClientTest {

    private ClearcutClient mClient;

    @Before
    public void setUp() {
        mClient =
                new ClearcutClient("url") {
                    @Override
                    boolean isGoogleUser() {
                        return false;
                    }
                };
    }

    @After
    public void tearDown() {
        mClient.stop();
    }

    @Test
    public void testGetGroupingKey() throws Exception {
        File testFile = FileUtil.createTempFile("uuid-test", "");
        try {
            mClient.setCachedUuidFile(testFile);
            String grouping = mClient.getGroupingKey();
            // Key was created and written to cached file.
            assertEquals(grouping, FileUtil.readStringFromFile(testFile));
        } finally {
            FileUtil.deleteFile(testFile);
        }
    }

    @Test
    public void testGetGroupingKey_exists() throws Exception {
        File testFile = FileUtil.createTempFile("uuid-test", "");
        try {
            FileUtil.writeToFile("test", testFile);
            mClient.setCachedUuidFile(testFile);
            String grouping = mClient.getGroupingKey();
            assertEquals("test", grouping);
        } finally {
            FileUtil.deleteFile(testFile);
        }
    }

    @Test
    public void testDisableClient() {
        ClearcutClient c =
                new ClearcutClient("url") {
                    @Override
                    boolean isClearcutDisabled() {
                        return true;
                    }

                    @Override
                    boolean isGoogleUser() {
                        throw new RuntimeException("Should not be called if disabled");
                    }
                };
        try {
            c.notifyTradefedStartEvent();
            c.notifyTradefedStartEvent();
            c.notifyTradefedStartEvent();
            assertEquals(0, c.getQueueSize());
        } finally {
            c.stop();
        }
    }
}
