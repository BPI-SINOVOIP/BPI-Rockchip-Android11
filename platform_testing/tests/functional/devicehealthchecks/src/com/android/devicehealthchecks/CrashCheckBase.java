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
package com.android.devicehealthchecks;

import android.content.Context;
import android.os.DropBoxManager;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Pattern;

abstract class CrashCheckBase {

    private static final int MAX_DROPBOX_READ = 4096; // read up to 4K from a dropbox entry
    private static final int MAX_CRASH_SNIPPET_LINES = 40;
    private static final String INCLUDE_KNOWN_FAILURES = "include_known_failures";
    private static final String LOG_TAG = CrashCheckBase.class.getSimpleName();
    private Context mContext;
    private KnownFailures mKnownFailures = new KnownFailures();
    /** whether known failures should be reported anyways, useful for bug investigation */
    private boolean mIncludeKnownFailures = false;

    private List<String> failures = new ArrayList<>();

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation().getContext();
        Bundle bundle = InstrumentationRegistry.getArguments();
        mIncludeKnownFailures = TextUtils.equals("true", bundle.getString(INCLUDE_KNOWN_FAILURES));
        if (!mIncludeKnownFailures) {
            Log.i(LOG_TAG, "Will ignore known failures, populating known failure list");
            populateKnownFailures();
        }
    }

    /**
     * Check dropbox service for a particular label and assert if found
     */
    protected void checkCrash(String label) {
        DropBoxManager dropbox = (DropBoxManager) mContext
                .getSystemService(Context.DROPBOX_SERVICE);
        Assert.assertNotNull("Unable access the DropBoxManager service", dropbox);

        long timestamp = 0;
        DropBoxManager.Entry entry;
        int crashCount = 0;
        StringBuilder errorDetails = new StringBuilder("Error details:\n");
        while (null != (entry = dropbox.getNextEntry(label, timestamp))) {
            String dropboxSnippet;
            try {
                dropboxSnippet = entry.getText(MAX_DROPBOX_READ);
            } finally {
                entry.close();
            }
            KnownFailureItem k = mKnownFailures.findMatchedKnownFailure(label, dropboxSnippet);
            if (k != null && !mIncludeKnownFailures) {
                Log.i(
                        LOG_TAG,
                        String.format(
                                "Ignored a known failure, type: %s, pattern: %s, bug: b/%s",
                                label, k.failurePattern, k.bugNumber));
            } else {
                crashCount++;
                errorDetails.append(label);
                errorDetails.append(": ");
                errorDetails.append(truncate(dropboxSnippet, MAX_CRASH_SNIPPET_LINES));
                errorDetails.append("    ...\n");
            }
            timestamp = entry.getTimeMillis();
        }
        Assert.assertEquals(errorDetails.toString(), 0, crashCount);
    }

    /**
     * Truncate the text to at most the specified number of lines, and append a marker at the end
     * when truncated
     * @param text
     * @param maxLines
     * @return
     */
    private static String truncate(String text, int maxLines) {
        String[] lines = text.split("\\r?\\n");
        StringBuilder ret = new StringBuilder();
        for (int i = 0; i < maxLines && i < lines.length; i++) {
            ret.append(lines[i]);
            ret.append('\n');
        }
        if (lines.length > maxLines) {
            ret.append("... ");
            ret.append(lines.length - maxLines);
            ret.append(" more lines truncated ...\n");
        }
        return ret.toString();
    }

    /** Parse known failure file and add to the list of known failures */
    private void populateKnownFailures() {

        try {
            BufferedReader reader =
                    new BufferedReader(
                            new InputStreamReader(
                                    mContext.getAssets().open("bug_map"), StandardCharsets.UTF_8));
            while (reader.ready()) {
                failures = Arrays.asList(reader.readLine());

                for (String bug : failures) {
                    Log.i(LOG_TAG, String.format("ParsedFile: %s", bug));

                    List<String> split_bug = Arrays.asList(bug.split(" "));

                    if (split_bug.size() != 3) {
                        Log.e(
                                LOG_TAG,
                                String.format(
                                        "bug_map file splits lines using space, please correct: %s",
                                        bug));
                    } else {
                        String dropbox_label = split_bug.get(0);
                        Pattern pattern = Pattern.compile(split_bug.get(1), Pattern.MULTILINE);
                        String bug_id = split_bug.get(2);
                        Log.i(
                                LOG_TAG,
                                String.format(
                                        "Adding failure b/%s to test: %s", bug_id, dropbox_label));

                        mKnownFailures.addKnownFailure(dropbox_label, pattern, bug_id);
                    }
                }
            }
        } catch (IOException | NullPointerException e) {
            e.printStackTrace();
        }
    }
}
