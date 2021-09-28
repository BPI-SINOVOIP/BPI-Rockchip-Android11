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

package android.dynamicmime.testapp.update;

import static android.dynamicmime.common.Constants.APK_BOTH_GROUPS;
import static android.dynamicmime.common.Constants.GROUP_FIRST;
import static android.dynamicmime.common.Constants.GROUP_SECOND;
import static android.dynamicmime.common.Constants.MIME_TEXT_PLAIN;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test cases for app update, without MIME groups changes
 */
@RunWith(AndroidJUnit4.class)
public class SameGroupsTest extends BaseUpdateTest {
    @Test
    public void testUpdate() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);

        updateApp();

        assertMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);
        assertMimeGroupIsEmpty(GROUP_SECOND);
    }

    @Override
    String installApkPath() {
        return APK_BOTH_GROUPS;
    }

    @Override
    String updateApkPath() {
        return APK_BOTH_GROUPS;
    }
}
