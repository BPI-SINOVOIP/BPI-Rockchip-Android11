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

package com.android.documentsui;

import static junit.framework.Assert.fail;

import android.content.pm.PackageManager;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.documentsui.testing.TestEnv;
import com.android.documentsui.testing.TestProvidersAccess;

import com.google.common.collect.Lists;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class DocumentsAccessTest {

    private TestActivity mActivity;
    private DocumentsAccess mDocumentsAccess;
    private TestEnv mEnv;

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        mEnv = TestEnv.create();
        mEnv.reset();
        mActivity = TestActivity.create(mEnv);
        mDocumentsAccess = DocumentsAccess.create(mActivity, mEnv.state);
    }

    @Test
    public void testCreateDocument_noPermission() throws Exception {
        try {
            mDocumentsAccess.getDocuments(TestProvidersAccess.OtherUser.USER_ID, "authority",
                    Lists.newArrayList("docId"));
            fail("Expects CrossProfileNoPermissionException");
        } catch (CrossProfileNoPermissionException e) {
            // expected.
        }
    }
}
