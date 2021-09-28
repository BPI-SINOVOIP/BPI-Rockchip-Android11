/*
 * Copyright (C) 2017 The Android Open Source Project
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
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.database.Cursor;
import android.provider.DocumentsContract.Document;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;
import com.android.documentsui.testing.ActivityManagers;
import com.android.documentsui.testing.TestCursor;
import com.android.documentsui.testing.TestEnv;
import com.android.documentsui.testing.TestFileTypeLookup;
import com.android.documentsui.testing.TestImmediateExecutor;
import com.android.documentsui.testing.TestProvidersAccess;
import com.android.documentsui.testing.UserManagers;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class RecentsLoaderTests {

    private TestEnv mEnv;
    private TestActivity mActivity;
    private RecentsLoader mLoader;
    private boolean mContentChanged;

    @Before
    public void setUp() {
        mEnv = TestEnv.create();
        mActivity = TestActivity.create(mEnv);
        mActivity.activityManager = ActivityManagers.create(false);
        mActivity.userManager = UserManagers.create();

        mEnv.state.action = State.ACTION_BROWSE;
        mEnv.state.acceptMimes = new String[] { "*/*" };
        mEnv.state.canShareAcrossProfile = true;

        mLoader = new RecentsLoader(mActivity, mEnv.providers, mEnv.state,
                TestImmediateExecutor.createLookup(), new TestFileTypeLookup(),
                UserId.DEFAULT_USER);
    }

    @Test
    public void testNotLocalOnlyRoot_beIgnored() {
        assertTrue(mLoader.shouldIgnoreRoot(TestProvidersAccess.PICKLES));
    }

    @Test
    public void testLocalOnlyRoot_supportRecent_notIgnored() {
        assertFalse(mLoader.shouldIgnoreRoot(TestProvidersAccess.DOWNLOADS));
    }

    @Test
    public void testLocalOnlyRoot_supportRecent_differentUser_beIgnored() {
        assertTrue(mLoader.shouldIgnoreRoot(TestProvidersAccess.OtherUser.DOWNLOADS));
    }

    @Test
    public void testDocumentsNotIncludeDirectory() {
        final DocumentInfo doc = mEnv.model.createFolder("test");
        doc.lastModified = System.currentTimeMillis();

        mEnv.mockProviders.get(TestProvidersAccess.HOME.authority)
                .setNextChildDocumentsReturns(doc);

        final DirectoryResult result = mLoader.loadInBackground();

        final Cursor c = result.cursor;
        assertEquals(0, c.getCount());
    }

    @Test
    public void testShowOrHideHiddenFiles() {
        final DocumentInfo doc = mEnv.model.createFile(".test");
        doc.lastModified = System.currentTimeMillis();
        mEnv.mockProviders.get(TestProvidersAccess.HOME.authority)
                .setNextRecentDocumentsReturns(doc);

        assertEquals(false, mLoader.mState.showHiddenFiles);
        DirectoryResult result = mLoader.loadInBackground();
        assertEquals(0, result.cursor.getCount());

        mLoader.mState.showHiddenFiles = true;
        result = mLoader.loadInBackground();
        assertEquals(1, result.cursor.getCount());
    }

    @Test
    public void testDocumentsNotMovable() {
        final DocumentInfo doc = mEnv.model.createFile("freddy.jpg",
                Document.FLAG_SUPPORTS_MOVE
                        | Document.FLAG_SUPPORTS_DELETE
                        | Document.FLAG_SUPPORTS_REMOVE);
        doc.lastModified = System.currentTimeMillis();
        mEnv.mockProviders.get(TestProvidersAccess.HOME.authority)
                .setNextRecentDocumentsReturns(doc);

        final DirectoryResult result = mLoader.loadInBackground();

        final Cursor c = result.cursor;
        assertEquals(1, c.getCount());
        for (int i = 0; i < c.getCount(); ++i) {
            c.moveToNext();
            final int flags = c.getInt(c.getColumnIndex(Document.COLUMN_FLAGS));
            assertEquals(0, flags & Document.FLAG_SUPPORTS_DELETE);
            assertEquals(0, flags & Document.FLAG_SUPPORTS_REMOVE);
            assertEquals(0, flags & Document.FLAG_SUPPORTS_MOVE);
        }
    }

    @Test
    public void testContentsUpdate_observable() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        Runnable callback = () -> {
            latch.countDown();
            mContentChanged = true;
        };
        mLoader.setObserver(new LockingContentObserver(new ContentLock(), callback));

        final DocumentInfo doc = mEnv.model.createFile("freddy.jpg");
        doc.lastModified = System.currentTimeMillis();
        mEnv.mockProviders.get(TestProvidersAccess.HOME.authority)
                .setNextRecentDocumentsReturns(doc);

        mLoader.loadInBackground();

        TestCursor c = (TestCursor) mEnv.mockProviders.get(TestProvidersAccess.HOME.authority)
                .queryRecentDocuments(null, null);
        c.mockOnChange();

        latch.await(1, TimeUnit.SECONDS);
        assertTrue(mContentChanged);
    }

    @Test
    public void testLoaderOnUserWithoutPermission() {
        mEnv.state.canShareAcrossProfile = false;
        mLoader = new RecentsLoader(mActivity, mEnv.providers, mEnv.state,
                TestImmediateExecutor.createLookup(), new TestFileTypeLookup(),
                TestProvidersAccess.OtherUser.USER_ID);
        final DirectoryResult result = mLoader.loadInBackground();

        assertThat(result.cursor).isNull();
        assertThat(result.exception).isInstanceOf(CrossProfileNoPermissionException.class);
    }

    @Test
    public void testLoaderOnUser_quietMode() {
        when(mActivity.userManager.isQuietModeEnabled(any())).thenReturn(true);
        final DirectoryResult result = mLoader.loadInBackground();

        assertThat(result.cursor).isNull();
        assertThat(result.exception).isInstanceOf(CrossProfileQuietModeException.class);
    }
}
