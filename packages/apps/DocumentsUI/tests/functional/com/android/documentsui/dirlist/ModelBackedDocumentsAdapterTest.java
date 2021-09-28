/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.documentsui.dirlist;

import android.content.Context;
import android.test.AndroidTestCase;

import androidx.test.filters.MediumTest;

import com.android.documentsui.ActionHandler;
import com.android.documentsui.Model;
import com.android.documentsui.base.State;
import com.android.documentsui.testing.TestActionHandler;
import com.android.documentsui.testing.TestEnv;
import com.android.documentsui.testing.TestFileTypeLookup;

@MediumTest
public class ModelBackedDocumentsAdapterTest extends AndroidTestCase {

    private static final String AUTHORITY = "test_authority";

    private TestEnv mEnv;
    private ActionHandler mActionHandler;
    private ModelBackedDocumentsAdapter mAdapter;

    public void setUp() {

        final Context testContext = TestContext.createStorageTestContext(getContext(), AUTHORITY);
        mEnv = TestEnv.create(AUTHORITY);
        mActionHandler = new TestActionHandler();

        DocumentsAdapter.Environment env = new TestEnvironment(testContext, mEnv, mActionHandler);

        mAdapter = new ModelBackedDocumentsAdapter(
                env,
                new IconHelper(testContext, State.MODE_GRID, /* maybeShowBadge= */ false),
                new TestFileTypeLookup());
        mAdapter.getModelUpdateListener().accept(Model.Update.UPDATE);
    }

    // Tests that the item count is correct.
    public void testItemCount() {
        assertEquals(mEnv.model.getItemCount(), mAdapter.getItemCount());
    }
}
