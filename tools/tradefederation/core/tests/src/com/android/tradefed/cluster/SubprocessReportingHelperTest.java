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
package com.android.tradefed.cluster;

import static org.junit.Assert.*;

import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.ZipUtil2;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;

/** Unit tests for {@link SubprocessReportingHelper} */
@RunWith(JUnit4.class)
public class SubprocessReportingHelperTest {
    private SubprocessReportingHelper mHelper;
    private File mWorkDir;

    @Before
    public void setUp() throws IOException {
        mHelper = new SubprocessReportingHelper();
        mWorkDir = FileUtil.createTempDir("tfjar");
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mWorkDir);
    }

    @Test
    public void testCreateSubprocessReporterJar() throws IOException {
        File jar = mHelper.createSubprocessReporterJar(mWorkDir);
        assertNotNull(jar);
        File extractedJar = ZipUtil2.extractZipToTemp(jar, "tmp-jar");
        try {
            assertNotNull(FileUtil.findFile(extractedJar, "LegacySubprocessResultsReporter.class"));
            assertNotNull(FileUtil.findFile(extractedJar, "SubprocessTestResultsParser.class"));
            assertNotNull(FileUtil.findFile(extractedJar, "SubprocessEventHelper.class"));
            assertNotNull(FileUtil.findFile(extractedJar, "SubprocessResultsReporter.class"));
        } finally {
            FileUtil.recursiveDelete(extractedJar);
        }
    }

    @Test
    public void testGetNewCommandLine() throws IOException {
        String oldCommandLine = "cts arg1 arg2 arg3";
        String newCommandLine = mHelper.buildNewCommandConfig(oldCommandLine, "1024", mWorkDir);
        assertNotNull(FileUtil.findFile(mWorkDir, "_cts.xml"));
        assertEquals("_cts.xml arg1 arg2 arg3", newCommandLine);
    }

    @Test
    public void testGetNewCommandLine_withSlashes() throws IOException {
        String oldCommandLine = "foo/bar/cts arg1 arg2 arg3";
        String newCommandLine = mHelper.buildNewCommandConfig(oldCommandLine, "1024", mWorkDir);
        assertNotNull(FileUtil.findFile(mWorkDir, "_foo\\$bar\\$cts.xml"));
        assertEquals("_foo$bar$cts.xml arg1 arg2 arg3", newCommandLine);
    }
}
