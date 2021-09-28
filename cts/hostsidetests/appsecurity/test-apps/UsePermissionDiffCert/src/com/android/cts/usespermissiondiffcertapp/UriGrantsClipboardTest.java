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

package com.android.cts.usespermissiondiffcertapp;

import static com.android.cts.usespermissiondiffcertapp.AccessPermissionWithDiffSigTest.GRANTABLE;
import static com.android.cts.usespermissiondiffcertapp.AccessPermissionWithDiffSigTest.NOT_GRANTABLE;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertClipDataEquals;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertReadingClipAllowed;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertReadingClipNotAllowed;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertWritingClipNotAllowed;
import static com.android.cts.usespermissiondiffcertapp.UriGrantsTest.TAG;
import static com.android.cts.usespermissiondiffcertapp.UriGrantsTest.makeSingleClipData;
import static com.android.cts.usespermissiondiffcertapp.Utils.clearPrimaryClip;
import static com.android.cts.usespermissiondiffcertapp.Utils.setPrimaryClip;

import static junit.framework.Assert.fail;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.net.Uri;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class UriGrantsClipboardTest {
    private static Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    @Test
    public void testClipboardWithPermission() throws Exception {
        for (Uri target : GRANTABLE) {
            Log.d(TAG, "Testing " + target);
            final ClipData clip = makeSingleClipData(target);

            // Normally we can't see the underlying clip data
            assertReadingClipNotAllowed(clip);
            assertWritingClipNotAllowed(clip);

            // Use shell's permissions to ensure we can access the clipboard
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .adoptShellPermissionIdentity();
            ClipData clipFromClipboard;
            try {
                // But if someone puts it on the clipboard, we can read it
                setPrimaryClip(clip);
                clipFromClipboard = getContext()
                        .getSystemService(ClipboardManager.class).getPrimaryClip();
            } finally {
                InstrumentationRegistry.getInstrumentation().getUiAutomation()
                        .dropShellPermissionIdentity();
            }
            assertClipDataEquals(clip, clipFromClipboard);
            assertReadingClipAllowed(clipFromClipboard);
            assertWritingClipNotAllowed(clipFromClipboard);

            // And if clipboard is cleared, we lose access
            clearPrimaryClip();
            assertReadingClipNotAllowed(clipFromClipboard);
            assertWritingClipNotAllowed(clipFromClipboard);
        }
    }

    @Test
    public void testClipboardWithoutPermission() throws Exception {
        for (Uri target : NOT_GRANTABLE) {
            Log.d(TAG, "Testing " + target);
            final ClipData clip = makeSingleClipData(target);

            // Can't see it directly
            assertReadingClipNotAllowed(clip);
            assertWritingClipNotAllowed(clip);

            // Can't put on clipboard if we don't own it
            try {
                setPrimaryClip(clip);
                fail("Unexpected ability to put protected data " + clip + " on clipboard!");
            } catch (Exception expected) {
            }
        }
    }
}
