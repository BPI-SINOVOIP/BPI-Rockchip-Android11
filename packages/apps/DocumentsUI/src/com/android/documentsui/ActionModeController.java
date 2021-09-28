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

import static com.android.documentsui.base.SharedMinimal.DEBUG;

import android.app.Activity;
import android.util.Log;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.recyclerview.selection.MutableSelection;
import androidx.recyclerview.selection.SelectionTracker;
import androidx.recyclerview.selection.SelectionTracker.SelectionObserver;

import com.android.documentsui.MenuManager.SelectionDetails;
import com.android.documentsui.base.EventHandler;
import com.android.documentsui.base.Menus;
import com.android.documentsui.ui.MessageBuilder;

/**
 * A controller that listens to selection changes and manages life cycles of action modes.
 */
public class ActionModeController extends SelectionObserver<String>
        implements ActionMode.Callback, ActionModeAddons {

    private static final String TAG = "ActionModeController";

    private final Activity mActivity;
    private final SelectionTracker<String> mSelectionMgr;
    private final MenuManager mMenuManager;
    private final MessageBuilder mMessages;

    private final ContentScope mScope = new ContentScope();
    private final MutableSelection<String> mSelected = new MutableSelection<>();

    private @Nullable ActionMode mActionMode;
    private @Nullable Menu mMenu;

    public ActionModeController(
            Activity activity,
            SelectionTracker<String> selectionMgr,
            MenuManager menuManager,
            MessageBuilder messages) {

        mActivity = activity;
        mSelectionMgr = selectionMgr;
        mMenuManager = menuManager;
        mMessages = messages;
    }

    @Override
    public void onSelectionChanged() {
        mSelectionMgr.copySelection(mSelected);
        if (mSelected.size() > 0) {
            if (mActionMode == null) {
                if (DEBUG) {
                    Log.d(TAG, "Starting action mode.");
                }
                mActionMode = mActivity.startActionMode(this);
                final View closeButton = mActivity.findViewById(R.id.action_mode_close_button);
                if (closeButton != null) {
                    closeButton.setContentDescription(mActivity.getString(android.R.string.cancel));
                }
            }
            updateActionMenu();
        } else {
            if (mActionMode != null) {
                if (DEBUG) {
                    Log.d(TAG, "Finishing action mode.");
                }
                mActionMode.finish();
            }
        }

        if (mActionMode != null) {
            assert(!mSelected.isEmpty());
            final String title = mMessages.getQuantityString(
                    R.plurals.elements_selected, mSelected.size());
            mActionMode.setTitle(title);
            mActivity.getWindow().setTitle(title);
        }
    }

    @Override
    public void onSelectionRestored() {
        onSelectionChanged();
    }

    // Called when the user exits the action mode
    @Override
    public void onDestroyActionMode(ActionMode mode) {
        if (mActionMode == null) {
            if (DEBUG) {
                Log.w(TAG, "Received call to destroy action mode on alien mode object.");
            }
        }

        assert(mActionMode.equals(mode));

        if (DEBUG) {
            Log.d(TAG, "Handling action mode destroyed.");
        }
        mActionMode = null;
        mMenu = null;

        if (mSelected.size() > 0) {
            mSelectionMgr.clearSelection();
        }

        // Reset window title back to activity title, i.e. Root name
        mActivity.getWindow().setTitle(mActivity.getTitle());

        // Re-enable TalkBack for the toolbars, as they are no longer covered by action mode.
        mScope.accessibilityImportanceSetter.setAccessibilityImportance(
                View.IMPORTANT_FOR_ACCESSIBILITY_AUTO, R.id.toolbar, R.id.roots_toolbar);
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        int size = mSelectionMgr.getSelection().size();
        mode.getMenuInflater().inflate(R.menu.action_mode_menu, menu);
        mode.setTitle(mActivity.getResources().getQuantityString(R.plurals.selected_count, size));

        if (size > 0) {

            // Hide the toolbars if action mode is enabled, so TalkBack doesn't navigate to
            // these controls when using linear navigation.
            mScope.accessibilityImportanceSetter.setAccessibilityImportance(
                    View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS,
                    R.id.toolbar,
                    R.id.roots_toolbar);
            return true;
        }

        return false;
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        mMenu = menu;
        updateActionMenu();
        return true;
    }

    private void updateActionMenu() {
        assert(mMenu != null);
        mMenuManager.updateActionMenu(mMenu, mScope.selectionDetails);
        Menus.disableHiddenItems(mMenu);
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        return mScope.menuItemClicker.accept(item);
    }

    private static void setImportantForAccessibility(
            Activity activity, int accessibilityImportance, @IdRes int[] viewIds) {
        for (final int id : viewIds) {
            final View v = activity.findViewById(id);
            if (v != null) {
                v.setImportantForAccessibility(accessibilityImportance);
            }
        }
    }

    @FunctionalInterface
    private interface AccessibilityImportanceSetter {
        void setAccessibilityImportance(int accessibilityImportance, @IdRes int... viewIds);
    }

    @Override
    public void finishActionMode() {
        if (mActionMode != null) {
            mActionMode.finish();
            mActionMode = null;
        } else {
            Log.w(TAG, "Tried to finish a null action mode.");
        }
    }

    public ActionModeController reset(
            SelectionDetails selectionDetails, EventHandler<MenuItem> menuItemClicker) {
        assert(mActionMode == null);
        assert(mMenu == null);

        mScope.menuItemClicker = menuItemClicker;
        mScope.selectionDetails = selectionDetails;
        mScope.accessibilityImportanceSetter =
                (int accessibilityImportance, @IdRes int[] viewIds) -> {
                    setImportantForAccessibility(
                            mActivity, accessibilityImportance, viewIds);
                };

        return this;
    }

    private static final class ContentScope {
        private EventHandler<MenuItem> menuItemClicker;
        private SelectionDetails selectionDetails;
        private AccessibilityImportanceSetter accessibilityImportanceSetter;
    }
}
