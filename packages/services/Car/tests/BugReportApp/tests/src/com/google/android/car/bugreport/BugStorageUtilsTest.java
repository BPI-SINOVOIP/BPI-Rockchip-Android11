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
package com.android.car.bugreport;

import static com.android.car.bugreport.MetaBugReport.TYPE_INTERACTIVE;
import static com.android.car.bugreport.Status.STATUS_PENDING_USER_ACTION;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.time.Instant;
import java.util.Date;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class BugStorageUtilsTest {
    private static final String TIMESTAMP_TODAY = MetaBugReport.toBugReportTimestamp(new Date());
    private static final String BUGREPORT_ZIP_FILE_NAME = "bugreport@ASD.zip";
    private static final int BUGREPORT_ZIP_FILE_CONTENT = 1;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getContext();
    }

    @Test
    public void test_createBugReport_createsAndReturnsMetaBugReport() throws Exception {
        MetaBugReport bug = createBugReportWithStatus(TIMESTAMP_TODAY,
                STATUS_PENDING_USER_ACTION, TYPE_INTERACTIVE, /* createBugReportFile= */ true);

        assertThat(BugStorageUtils.findBugReport(mContext, bug.getId()).get()).isEqualTo(bug);
    }

    @Test
    public void test_expireBugReport_marksBugReportDeletedAndDeletesZip() throws Exception {
        MetaBugReport bug = createBugReportWithStatus(TIMESTAMP_TODAY,
                STATUS_PENDING_USER_ACTION, TYPE_INTERACTIVE, /* createBugReportFile= */ true);
        try (InputStream in = BugStorageUtils.openBugReportFileToRead(mContext, bug)) {
            assertThat(in).isNotNull();
        }
        Instant now = Instant.now();

        boolean deleteResult = BugStorageUtils.expireBugReport(mContext, bug, now);

        assertThat(deleteResult).isTrue();
        assertThat(BugStorageUtils.findBugReport(mContext, bug.getId()).get())
                .isEqualTo(bug.toBuilder()
                        .setStatus(Status.STATUS_EXPIRED.getValue())
                        .setStatusMessage("Expired on " + now).build());
        assertThrows(FileNotFoundException.class, () ->
                BugStorageUtils.openBugReportFileToRead(mContext, bug));
    }

    @Test
    public void test_completeDeleteBugReport_removesBugReportRecordFromDb() throws Exception {
        MetaBugReport bug = createBugReportWithStatus(TIMESTAMP_TODAY,
                STATUS_PENDING_USER_ACTION, TYPE_INTERACTIVE, /* createBugReportFile= */ true);
        try (InputStream in = BugStorageUtils.openBugReportFileToRead(mContext, bug)) {
            assertThat(in).isNotNull();
        }

        boolean deleteResult = BugStorageUtils.completeDeleteBugReport(mContext, bug.getId());

        assertThat(deleteResult).isTrue();
        assertThat(BugStorageUtils.findBugReport(mContext, bug.getId()).isPresent()).isFalse();
        assertThrows(IllegalArgumentException.class, () ->
                BugStorageUtils.openBugReportFileToRead(mContext, bug));
    }

    private MetaBugReport createBugReportWithStatus(
            String timestamp, Status status, int type, boolean createBugReportFile)
            throws IOException {
        MetaBugReport bugReport = BugStorageUtils.createBugReport(
                mContext, "sample title", timestamp, "driver", type);
        if (createBugReportFile) {
            bugReport = BugStorageUtils.update(mContext,
                    bugReport.toBuilder().setBugReportFileName(BUGREPORT_ZIP_FILE_NAME).build());
            try (OutputStream out = BugStorageUtils.openBugReportFileToWrite(mContext, bugReport)) {
                out.write(BUGREPORT_ZIP_FILE_CONTENT);
            }
        }
        return BugStorageUtils.setBugReportStatus(mContext, bugReport, status, "");
    }

    private static void assertThrows(Class<? extends Throwable> exceptionClass,
            ExceptionRunnable r) {
        try {
            r.run();
        } catch (Throwable e) {
            assertTrue("Expected exception type " + exceptionClass.getName() + " but got "
                    + e.getClass().getName(), exceptionClass.isAssignableFrom(e.getClass()));
            return;
        }
        fail("Expected exception type " + exceptionClass.getName()
                + ", but no exception was thrown");
    }

    private interface ExceptionRunnable {
        void run() throws Exception;
    }
}
