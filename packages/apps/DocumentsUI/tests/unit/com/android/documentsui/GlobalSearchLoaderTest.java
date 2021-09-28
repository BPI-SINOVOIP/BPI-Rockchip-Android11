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

package com.android.documentsui;

import static com.google.common.truth.Truth.assertThat;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;

import android.database.Cursor;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.State;
import com.android.documentsui.sorting.SortDimension;
import com.android.documentsui.sorting.SortModel;
import com.android.documentsui.testing.ActivityManagers;
import com.android.documentsui.testing.TestEnv;
import com.android.documentsui.testing.TestFileTypeLookup;
import com.android.documentsui.testing.TestImmediateExecutor;
import com.android.documentsui.testing.TestProvidersAccess;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class GlobalSearchLoaderTest {

    private static final String SEARCH_STRING = "freddy";
    private static int FILE_FLAG = Document.FLAG_SUPPORTS_MOVE | Document.FLAG_SUPPORTS_DELETE
            | Document.FLAG_SUPPORTS_REMOVE;

    private TestEnv mEnv;
    private TestActivity mActivity;
    private GlobalSearchLoader mLoader;
    private Bundle mQueryArgs = new Bundle();

    @Before
    public void setUp() {
        mEnv = TestEnv.create();
        mActivity = TestActivity.create(mEnv);
        mActivity.activityManager = ActivityManagers.create(false);

        mEnv.state.action = State.ACTION_BROWSE;
        mEnv.state.acceptMimes = new String[]{"*/*"};

        mQueryArgs.putString(DocumentsContract.QUERY_ARG_DISPLAY_NAME, SEARCH_STRING);
        mLoader = new GlobalSearchLoader(mActivity, mEnv.providers, mEnv.state,
                TestImmediateExecutor.createLookup(), new TestFileTypeLookup(), mQueryArgs,
                TestProvidersAccess.USER_ID);

        final DocumentInfo doc = mEnv.model.createFile(SEARCH_STRING + ".jpg", FILE_FLAG);
        doc.lastModified = System.currentTimeMillis();
        mEnv.mockProviders.get(TestProvidersAccess.DOWNLOADS.authority)
                .setNextChildDocumentsReturns(doc);

        TestProvidersAccess.DOWNLOADS.flags |= DocumentsContract.Root.FLAG_SUPPORTS_SEARCH;
    }

    @After
    public void tearDown() {
        TestProvidersAccess.DOWNLOADS.flags &= ~DocumentsContract.Root.FLAG_SUPPORTS_SEARCH;
    }

    @Test
    public void testNotSearchableRoot_beIgnored() {
        TestProvidersAccess.PICKLES.flags |= DocumentsContract.Root.FLAG_LOCAL_ONLY;
        assertTrue(mLoader.shouldIgnoreRoot(TestProvidersAccess.PICKLES));
        TestProvidersAccess.PICKLES.flags &= ~DocumentsContract.Root.FLAG_LOCAL_ONLY;
    }

    @Test
    public void testNotLocalOnlyRoot_beIgnored() {
        TestProvidersAccess.PICKLES.flags |= DocumentsContract.Root.FLAG_SUPPORTS_SEARCH;
        assertTrue(mLoader.shouldIgnoreRoot(TestProvidersAccess.PICKLES));
        TestProvidersAccess.PICKLES.flags &= ~DocumentsContract.Root.FLAG_SUPPORTS_SEARCH;
    }

    @Test
    public void testShowAdvance_storageRoot_beIgnored() {
        TestProvidersAccess.HOME.flags |= DocumentsContract.Root.FLAG_SUPPORTS_SEARCH;
        assertTrue(mLoader.shouldIgnoreRoot(TestProvidersAccess.HOME));
        TestProvidersAccess.HOME.flags &= ~(DocumentsContract.Root.FLAG_SUPPORTS_SEARCH);
    }

    @Test
    public void testCrossProfileRoot_notInTextSearch_beIgnored() {
        mEnv.state.supportsCrossProfile = true;
        mQueryArgs.remove(DocumentsContract.QUERY_ARG_DISPLAY_NAME);
        TestProvidersAccess.DOWNLOADS.userId = TestProvidersAccess.OtherUser.USER_ID;
        assertThat(mLoader.shouldIgnoreRoot(TestProvidersAccess.DOWNLOADS)).isTrue();
        TestProvidersAccess.DOWNLOADS.userId = TestProvidersAccess.USER_ID;
    }

    @Test
    public void testCrossProfileRoot_inTextSearch_beIncluded() {
        mEnv.state.supportsCrossProfile = true;
        TestProvidersAccess.DOWNLOADS.userId = TestProvidersAccess.OtherUser.USER_ID;
        assertThat(mLoader.shouldIgnoreRoot(TestProvidersAccess.DOWNLOADS)).isFalse();
        TestProvidersAccess.DOWNLOADS.userId = TestProvidersAccess.USER_ID;
    }

    @Test
    public void testSearchResult_includeDirectory() {
        final DocumentInfo doc = mEnv.model.createFolder(SEARCH_STRING);
        doc.lastModified = System.currentTimeMillis();

        mEnv.mockProviders.get(TestProvidersAccess.DOWNLOADS.authority)
                .setNextChildDocumentsReturns(doc);

        final DirectoryResult result = mLoader.loadInBackground();

        final Cursor c = result.cursor;
        assertEquals(1, c.getCount());

        c.moveToNext();
        final String mimeType = c.getString(c.getColumnIndex(Document.COLUMN_MIME_TYPE));
        assertEquals(Document.MIME_TYPE_DIR, mimeType);
    }

    @Test
    public void testShowOrHideHiddenFiles() {
        final DocumentInfo doc = mEnv.model.createFile(".test" + SEARCH_STRING);
        doc.lastModified = System.currentTimeMillis();
        mEnv.mockProviders.get(TestProvidersAccess.DOWNLOADS.authority)
                .setNextChildDocumentsReturns(doc);

        assertEquals(false, mLoader.mState.showHiddenFiles);
        DirectoryResult result = mLoader.loadInBackground();
        assertEquals(0, result.cursor.getCount());

        mLoader.mState.showHiddenFiles = true;
        result = mLoader.loadInBackground();
        assertEquals(1, result.cursor.getCount());
    }

    @Test
    public void testSearchResult_isNotMovable() {
        final DirectoryResult result = mLoader.loadInBackground();

        final Cursor c = result.cursor;
        assertEquals(1, c.getCount());

        c.moveToNext();
        final int flags = c.getInt(c.getColumnIndex(Document.COLUMN_FLAGS));
        assertEquals(0, flags & Document.FLAG_SUPPORTS_DELETE);
        assertEquals(0, flags & Document.FLAG_SUPPORTS_REMOVE);
        assertEquals(0, flags & Document.FLAG_SUPPORTS_MOVE);
    }

    @Test
    public void testSearchResult_includeDirectory_excludedOtherUsers() {
        mEnv.state.canShareAcrossProfile = false;

        TestProvidersAccess.DOWNLOADS.userId = TestProvidersAccess.USER_ID;
        TestProvidersAccess.PICKLES.userId = TestProvidersAccess.OtherUser.USER_ID;
        TestProvidersAccess.PICKLES.flags |= (DocumentsContract.Root.FLAG_SUPPORTS_SEARCH
                | DocumentsContract.Root.FLAG_LOCAL_ONLY);

        final DocumentInfo currentUserDoc = mEnv.model.createFile(
                SEARCH_STRING + "_currentUser.pdf");
        currentUserDoc.lastModified = System.currentTimeMillis();
        mEnv.mockProviders.get(TestProvidersAccess.DOWNLOADS.authority)
                .setNextChildDocumentsReturns(currentUserDoc);

        final DocumentInfo otherUserDoc = mEnv.model.createFile(SEARCH_STRING + "_otherUser.png");
        otherUserDoc.lastModified = System.currentTimeMillis();
        mEnv.mockProviders.get(TestProvidersAccess.PICKLES.authority)
                .setNextChildDocumentsReturns(otherUserDoc);

        final DirectoryResult result = mLoader.loadInBackground();
        final Cursor c = result.cursor;

        assertThat(c.getCount()).isEqualTo(1);
        c.moveToNext();
        final String docName = c.getString(c.getColumnIndex(Document.COLUMN_DISPLAY_NAME));
        assertThat(docName).contains("currentUser");

        TestProvidersAccess.DOWNLOADS.userId = TestProvidersAccess.USER_ID;
        TestProvidersAccess.PICKLES.userId = TestProvidersAccess.USER_ID;
        TestProvidersAccess.PICKLES.flags &= ~DocumentsContract.Root.FLAG_SUPPORTS_SEARCH;
    }

    @Test
    public void testSearchResult_includeSearchString() {
        final DocumentInfo pdfDoc = mEnv.model.createFile(SEARCH_STRING + ".pdf");
        pdfDoc.lastModified = System.currentTimeMillis();

        final DocumentInfo apkDoc = mEnv.model.createFile(SEARCH_STRING + ".apk");
        apkDoc.lastModified = System.currentTimeMillis();

        final DocumentInfo testApkDoc = mEnv.model.createFile("test.apk");
        testApkDoc.lastModified = System.currentTimeMillis();

        mEnv.mockProviders.get(TestProvidersAccess.DOWNLOADS.authority)
                .setNextChildDocumentsReturns(pdfDoc, apkDoc, testApkDoc);

        mEnv.state.sortModel.sortByUser(
                SortModel.SORT_DIMENSION_ID_TITLE, SortDimension.SORT_DIRECTION_ASCENDING);

        final DirectoryResult result = mLoader.loadInBackground();
        final Cursor c = result.cursor;

        assertEquals(2, c.getCount());

        c.moveToNext();
        String displayName = c.getString(c.getColumnIndex(Document.COLUMN_DISPLAY_NAME));
        assertTrue(displayName.contains(SEARCH_STRING));

        c.moveToNext();
        displayName = c.getString(c.getColumnIndex(Document.COLUMN_DISPLAY_NAME));
        assertTrue(displayName.contains(SEARCH_STRING));
    }

    @Test
    public void testSearchResult_includeDifferentRoot() {
        final DocumentInfo pdfDoc = mEnv.model.createFile(SEARCH_STRING + ".pdf");
        pdfDoc.lastModified = System.currentTimeMillis();

        final DocumentInfo apkDoc = mEnv.model.createFile(SEARCH_STRING + ".apk");
        apkDoc.lastModified = System.currentTimeMillis();

        final DocumentInfo testApkDoc = mEnv.model.createFile("test.apk");
        testApkDoc.lastModified = System.currentTimeMillis();

        mEnv.mockProviders.get(TestProvidersAccess.PICKLES.authority)
                .setNextChildDocumentsReturns(pdfDoc, apkDoc, testApkDoc);

        TestProvidersAccess.PICKLES.flags |= (DocumentsContract.Root.FLAG_SUPPORTS_SEARCH
                | DocumentsContract.Root.FLAG_LOCAL_ONLY);

        mEnv.state.sortModel.sortByUser(
                SortModel.SORT_DIMENSION_ID_TITLE, SortDimension.SORT_DIRECTION_ASCENDING);

        final DirectoryResult result = mLoader.loadInBackground();
        final Cursor c = result.cursor;

        assertEquals(3, c.getCount());

        TestProvidersAccess.PICKLES.flags &= ~(DocumentsContract.Root.FLAG_SUPPORTS_SEARCH
                | DocumentsContract.Root.FLAG_LOCAL_ONLY);
    }

    @Test
    public void testSearchResult_includeCurrentUserRootOnly() {
        mEnv.state.canShareAcrossProfile = false;
        mEnv.state.supportsCrossProfile = true;

        final DocumentInfo pdfDoc = mEnv.model.createFile(SEARCH_STRING + ".pdf");
        pdfDoc.lastModified = System.currentTimeMillis();

        final DocumentInfo apkDoc = mEnv.model.createFile(SEARCH_STRING + ".apk");
        apkDoc.lastModified = System.currentTimeMillis();

        final DocumentInfo testApkDoc = mEnv.model.createFile("test.apk");
        testApkDoc.lastModified = System.currentTimeMillis();

        mEnv.mockProviders.get(TestProvidersAccess.PICKLES.authority)
                .setNextChildDocumentsReturns(pdfDoc, apkDoc, testApkDoc);
        TestProvidersAccess.PICKLES.userId = TestProvidersAccess.OtherUser.USER_ID;

        TestProvidersAccess.PICKLES.flags |= (DocumentsContract.Root.FLAG_SUPPORTS_SEARCH
                | DocumentsContract.Root.FLAG_LOCAL_ONLY);
        mEnv.state.sortModel.sortByUser(
                SortModel.SORT_DIMENSION_ID_TITLE, SortDimension.SORT_DIRECTION_ASCENDING);

        final DirectoryResult result = mLoader.loadInBackground();
        final Cursor c = result.cursor;

        assertEquals(1, c.getCount());

        TestProvidersAccess.PICKLES.userId = TestProvidersAccess.USER_ID;
        TestProvidersAccess.PICKLES.flags &= ~(DocumentsContract.Root.FLAG_SUPPORTS_SEARCH
                | DocumentsContract.Root.FLAG_LOCAL_ONLY);
    }


    @Test
    public void testSearchResult_includeBothUsersRoots() {
        mEnv.state.canShareAcrossProfile = true;
        mEnv.state.supportsCrossProfile = true;

        final DocumentInfo pdfDoc = mEnv.model.createFile(SEARCH_STRING + ".pdf");
        pdfDoc.lastModified = System.currentTimeMillis();

        final DocumentInfo apkDoc = mEnv.model.createFile(SEARCH_STRING + ".apk");
        apkDoc.lastModified = System.currentTimeMillis();

        final DocumentInfo testApkDoc = mEnv.model.createFile("test.apk");
        testApkDoc.lastModified = System.currentTimeMillis();

        mEnv.mockProviders.get(TestProvidersAccess.PICKLES.authority)
                .setNextChildDocumentsReturns(pdfDoc, apkDoc, testApkDoc);
        TestProvidersAccess.PICKLES.userId = TestProvidersAccess.OtherUser.USER_ID;

        TestProvidersAccess.PICKLES.flags |= (DocumentsContract.Root.FLAG_SUPPORTS_SEARCH
                | DocumentsContract.Root.FLAG_LOCAL_ONLY);
        mEnv.state.sortModel.sortByUser(
                SortModel.SORT_DIMENSION_ID_TITLE, SortDimension.SORT_DIRECTION_ASCENDING);

        final DirectoryResult result = mLoader.loadInBackground();
        final Cursor c = result.cursor;

        assertEquals(3, c.getCount());

        TestProvidersAccess.PICKLES.userId = TestProvidersAccess.USER_ID;
        TestProvidersAccess.PICKLES.flags &= ~(DocumentsContract.Root.FLAG_SUPPORTS_SEARCH
                | DocumentsContract.Root.FLAG_LOCAL_ONLY);
    }


    @Test
    public void testSearchResult_emptyCurrentUsersRoot() {
        mEnv.state.canShareAcrossProfile = false;
        mEnv.state.supportsCrossProfile = true;

        final DocumentInfo pdfDoc = mEnv.model.createFile(SEARCH_STRING + ".pdf");
        pdfDoc.lastModified = System.currentTimeMillis();

        mEnv.mockProviders.get(TestProvidersAccess.PICKLES.authority)
                .setNextChildDocumentsReturns(pdfDoc);

        TestProvidersAccess.DOWNLOADS.userId = TestProvidersAccess.OtherUser.USER_ID;
        TestProvidersAccess.PICKLES.userId = TestProvidersAccess.OtherUser.USER_ID;
        TestProvidersAccess.PICKLES.flags |= (DocumentsContract.Root.FLAG_SUPPORTS_SEARCH
                | DocumentsContract.Root.FLAG_LOCAL_ONLY);
        mEnv.state.sortModel.sortByUser(
                SortModel.SORT_DIMENSION_ID_TITLE, SortDimension.SORT_DIRECTION_ASCENDING);

        final DirectoryResult result = mLoader.loadInBackground();
        assertThat(result.cursor.getCount()).isEqualTo(0);
        // We don't expect exception even if all roots are from other users.
        assertThat(result.exception).isNull();


        TestProvidersAccess.DOWNLOADS.userId = TestProvidersAccess.USER_ID;
        TestProvidersAccess.PICKLES.userId = TestProvidersAccess.USER_ID;
        TestProvidersAccess.PICKLES.flags &= ~(DocumentsContract.Root.FLAG_SUPPORTS_SEARCH
                | DocumentsContract.Root.FLAG_LOCAL_ONLY);
    }
}
