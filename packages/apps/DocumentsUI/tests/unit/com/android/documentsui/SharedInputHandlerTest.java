/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.recyclerview.selection.SelectionTracker;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.documentsui.base.Procedure;
import com.android.documentsui.dirlist.TestFocusHandler;
import com.android.documentsui.testing.TestDrawerController;
import com.android.documentsui.testing.TestFeatures;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class SharedInputHandlerTest {

    private SharedInputHandler mSharedInputHandler;
    private SelectionTracker<String> mSelectionMgr = SelectionHelpers.createTestInstance();
    private TestFeatures mFeatures = new TestFeatures();
    private TestFocusHandler mFocusHandler = new TestFocusHandler();
    private TestDrawerController mDrawer = TestDrawerController.create();
    private boolean mDirPopHappened;
    private boolean mCanceledSearch;
    private boolean mSearchExecuted;
    private Procedure mDirPopper = new Procedure() {
        @Override
        public boolean run() {
            mDirPopHappened = true;
            return true;
        }
    };
    private Runnable mSearchExecutor = new Runnable() {
        @Override
        public void run() {
            mSearchExecuted = true;
        }
    };

    @Before
    public void setUp() {
        mDirPopHappened = false;
        mSharedInputHandler = new SharedInputHandler(
                mFocusHandler,
                mSelectionMgr,
                () -> false,
                mDirPopper,
                mFeatures,
                mDrawer,
                mSearchExecutor);
    }

    @Test
    public void testUnrelatedButton_DoesNothing() {
        KeyEvent event =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_A, 0, 0);
        assertFalse(mSharedInputHandler.onKeyDown(event.getKeyCode(), event));
    }

    @Test
    public void testBackButton_CancelsSearch() {
        mSelectionMgr.select("1");
        mSharedInputHandler = new SharedInputHandler(
                new TestFocusHandler(),
                SelectionHelpers.createTestInstance(),
                () -> {
                        mCanceledSearch = true;
                        return true;
                },
                mDirPopper,
                new TestFeatures(),
                mDrawer,
                mSearchExecutor);
        KeyEvent backEvent =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK, 0, 0);
        assertTrue(mSharedInputHandler.onKeyDown(backEvent.getKeyCode(), backEvent));

        assertTrue(mCanceledSearch);
        assertEquals(mSelectionMgr.getSelection().size(), 1);
        assertFalse(mDirPopHappened);
        mDrawer.assertWasClosed();
    }

    @Test
    public void testBackButton_ClearsSelection() {
        mSelectionMgr.select("1");
        assertEquals(mSelectionMgr.getSelection().size(), 1);
        KeyEvent backEvent =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK, 0, 0);
        assertTrue(mSharedInputHandler.onKeyDown(backEvent.getKeyCode(), backEvent));

        assertFalse(mCanceledSearch);
        assertEquals(mSelectionMgr.getSelection().size(), 0);
        assertFalse(mDirPopHappened);
        mDrawer.assertWasClosed();
    }

    @Test
    public void testBackButton_PopsDirectory() {
        KeyEvent backEvent =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK, 0, 0);
        assertTrue(mSharedInputHandler.onKeyDown(backEvent.getKeyCode(), backEvent));

        assertFalse(mCanceledSearch);
        assertEquals(mSelectionMgr.getSelection().size(), 0);
        assertTrue(mDirPopHappened);
        mDrawer.assertWasClosed();
    }

    @Test
    public void testBackButton_CloseDrawer() {
        KeyEvent backEvent =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK, 0, 0);
        mDrawer.openDrawer(true);
        assertTrue(mSharedInputHandler.onKeyDown(backEvent.getKeyCode(), backEvent));
        mDrawer.assertWasOpened();
    }

    @Test
    public void testEscButton_CancelsSearch() {
        mSelectionMgr.select("1");
        mSharedInputHandler = new SharedInputHandler(
                new TestFocusHandler(),
                SelectionHelpers.createTestInstance(),
                () -> {
                        mCanceledSearch = true;
                        return true;
                },
                mDirPopper,
                new TestFeatures(),
                mDrawer,
                mSearchExecutor);
        KeyEvent escapeEvent =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE, 0, 0);
        assertTrue(mSharedInputHandler.onKeyDown(escapeEvent.getKeyCode(), escapeEvent));

        assertTrue(mCanceledSearch);
        assertEquals(mSelectionMgr.getSelection().size(), 1);
        assertFalse(mDirPopHappened);
    }

    @Test
    public void testEscButton_ClearsSelection() {
        mSelectionMgr.select("1");
        assertEquals(mSelectionMgr.getSelection().size(), 1);
        KeyEvent escapeEvent =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE, 0, 0);
        assertTrue(mSharedInputHandler.onKeyDown(escapeEvent.getKeyCode(), escapeEvent));

        assertFalse(mCanceledSearch);
        assertEquals(mSelectionMgr.getSelection().size(), 0);
        assertFalse(mDirPopHappened);
    }

    @Test
    public void testEscButton_DoesNotPopDirectory() {
        KeyEvent escapeEvent =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE, 0, 0);
        assertTrue(mSharedInputHandler.onKeyDown(escapeEvent.getKeyCode(), escapeEvent));

        assertFalse(mCanceledSearch);
        assertEquals(mSelectionMgr.getSelection().size(), 0);
        assertFalse(mDirPopHappened);
    }

    @Test
    public void testDeleteButton_PopsDirectory() {
        KeyEvent delEvent =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0, 0);
        assertTrue(mSharedInputHandler.onKeyDown(delEvent.getKeyCode(), delEvent));

        assertTrue(mDirPopHappened);
    }

    @Test
    public void testTab_AdvancesFocusArea() {
        mFeatures.systemKeyboardNavigation = false;
        KeyEvent tabEvent =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB, 0, 0);
        assertTrue(mSharedInputHandler.onKeyDown(tabEvent.getKeyCode(), tabEvent));

        assertTrue(mFocusHandler.advanceFocusAreaCalled);
    }

    @Test
    public void testNavKey_FocusesDirectory() {
        mFeatures.systemKeyboardNavigation = false;
        KeyEvent navEvent =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP, 0, 0);
        assertTrue(mSharedInputHandler.onKeyDown(navEvent.getKeyCode(), navEvent));

        assertTrue(mFocusHandler.focusDirectoryCalled);
    }

    @Test
    public void testSearchKey_LaunchSearchView() {
        KeyEvent searchEvent =
                new KeyEvent(0, 0, MotionEvent.ACTION_DOWN, KeyEvent.KEYCODE_SEARCH, 0, 0);
        assertTrue(mSharedInputHandler.onKeyDown(searchEvent.getKeyCode(), searchEvent));

        assertTrue(mSearchExecuted);
    }
}
