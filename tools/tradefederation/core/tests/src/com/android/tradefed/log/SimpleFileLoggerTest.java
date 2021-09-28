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
package com.android.tradefed.log;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import static org.hamcrest.CoreMatchers.endsWith;
import static org.junit.Assert.assertTrue;

import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.file.Files;
import java.util.List;
import java.util.stream.Collectors;

/** Unit tests for {@link SimpleFileLogger}. */
@RunWith(JUnit4.class)
public class SimpleFileLoggerTest {

    private static final String LOG_TAG = SimpleFileLoggerTest.class.getSimpleName();

    private File mLogFile;
    private SimpleFileLogger mLogger;

    @Before
    public void setUp() throws IOException, ConfigurationException {
        mLogFile = Files.createTempFile(LOG_TAG, null).toFile();
        mLogger = new SimpleFileLogger();
        OptionSetter setter = new OptionSetter(mLogger);
        setter.setOptionValue("path", mLogFile.getAbsolutePath());
    }

    @After
    public void tearDown() {
        if (mLogger != null) {
            mLogger.closeLog();
        }
        FileUtil.deleteFile(mLogFile);
    }

    @Test
    public void testPrintLog() throws IOException {
        mLogger.init();
        mLogger.printLog(LogLevel.DEBUG, LOG_TAG, "debug");
        mLogger.printLog(LogLevel.INFO, LOG_TAG, "info");
        mLogger.printLog(LogLevel.WARN, LOG_TAG, "warn");
        mLogger.printLog(LogLevel.ERROR, LOG_TAG, "error");

        List<String> lines = readLines(new FileInputStream(mLogFile));
        assertEquals(4, lines.size());
        assertThat(lines.get(0), endsWith(String.format("D/%s: %s", LOG_TAG, "debug")));
        assertThat(lines.get(1), endsWith(String.format("I/%s: %s", LOG_TAG, "info")));
        assertThat(lines.get(2), endsWith(String.format("W/%s: %s", LOG_TAG, "warn")));
        assertThat(lines.get(3), endsWith(String.format("E/%s: %s", LOG_TAG, "error")));
    }

    @Test
    public void testGetLog() throws IOException {
        mLogger.init();
        mLogger.printLog(LogLevel.INFO, LOG_TAG, "hello world");
        // getting the log is equivalent to reading the file
        List<String> expected = readLines(new FileInputStream(mLogFile));
        List<String> actual = readLines(mLogger.getLog().createInputStream());
        assertEquals(expected, actual);
    }

    @Test
    public void testInit() throws IOException {
        // logging is ignored if not initialized
        mLogger.printLog(LogLevel.INFO, LOG_TAG, "hello world");
        List<String> lines = readLines(new FileInputStream(mLogFile));
        assertTrue(lines.isEmpty());
    }

    @Test
    public void testCloseLog() throws IOException {
        mLogger.init();
        mLogger.closeLog();
        // logging is ignored if closed
        mLogger.printLog(LogLevel.INFO, LOG_TAG, "hello world");
        List<String> lines = readLines(new FileInputStream(mLogFile));
        assertTrue(lines.isEmpty());
    }

    @Test
    public void testClone() throws IOException {
        mLogger.init();
        mLogger.printLog(LogLevel.DEBUG, LOG_TAG, "original");

        // clone will append to same log file
        SimpleFileLogger clone = mLogger.clone();
        try {
            clone.init();
            clone.printLog(LogLevel.DEBUG, LOG_TAG, "clone");
        } finally {
            clone.closeLog();
        }

        List<String> lines = readLines(new FileInputStream(mLogFile));
        assertEquals(2, lines.size());
        assertThat(lines.get(0), endsWith("original"));
        assertThat(lines.get(1), endsWith("clone"));
    }

    // Read input stream as a list of strings.
    private static List<String> readLines(InputStream is) {
        return new BufferedReader(new InputStreamReader(is)).lines().collect(Collectors.toList());
    }
}
