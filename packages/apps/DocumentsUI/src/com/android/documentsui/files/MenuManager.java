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

package com.android.documentsui.files;

import android.content.Context;
import android.content.res.Resources;
import android.net.Uri;
import android.view.KeyEvent;
import android.view.KeyboardShortcutGroup;
import android.view.KeyboardShortcutInfo;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;

import androidx.fragment.app.Fragment;
import androidx.recyclerview.selection.SelectionTracker;

import com.android.documentsui.R;
import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.Features;
import com.android.documentsui.base.Lookup;
import com.android.documentsui.base.LookupApplicationName;
import com.android.documentsui.base.Menus;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.Shared;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;
import com.android.documentsui.queries.SearchViewManager;

import java.util.List;
import java.util.function.IntFunction;
import java.util.function.IntSupplier;

public final class MenuManager extends com.android.documentsui.MenuManager {

    private final Features mFeatures;
    private final Context mContext;
    private final SelectionTracker<String> mSelectionManager;
    private final Lookup<String, Uri> mUriLookup;
    private final LookupApplicationName mAppNameLookup;

    public MenuManager(
            Features features,
            SearchViewManager searchManager,
            State displayState,
            DirectoryDetails dirDetails,
            Context context,
            SelectionTracker<String> selectionManager,
            LookupApplicationName appNameLookup,
            Lookup<String, Uri> uriLookup,
            IntSupplier filesCountSupplier) {

        super(searchManager, displayState, dirDetails, filesCountSupplier);

        mFeatures = features;
        mContext = context;
        mSelectionManager = selectionManager;
        mAppNameLookup = appNameLookup;
        mUriLookup = uriLookup;
    }

    @Override
    public void updateKeyboardShortcutsMenu(
            List<KeyboardShortcutGroup> data, IntFunction<String> stringSupplier) {
        KeyboardShortcutGroup group = new KeyboardShortcutGroup(
                stringSupplier.apply(R.string.app_label));
        group.addItem(new KeyboardShortcutInfo(
                stringSupplier.apply(R.string.menu_cut_to_clipboard), KeyEvent.KEYCODE_X,
                KeyEvent.META_CTRL_ON));
        group.addItem(new KeyboardShortcutInfo(
                stringSupplier.apply(R.string.menu_copy_to_clipboard), KeyEvent.KEYCODE_C,
                KeyEvent.META_CTRL_ON));
        group.addItem(new KeyboardShortcutInfo(
                stringSupplier.apply(R.string.menu_paste_from_clipboard), KeyEvent.KEYCODE_V,
                KeyEvent.META_CTRL_ON));
        group.addItem(new KeyboardShortcutInfo(
                stringSupplier.apply(R.string.menu_create_dir), KeyEvent.KEYCODE_E,
                KeyEvent.META_CTRL_ON));
        group.addItem(new KeyboardShortcutInfo(
                stringSupplier.apply(R.string.menu_select_all), KeyEvent.KEYCODE_A,
                KeyEvent.META_CTRL_ON));
        group.addItem(new KeyboardShortcutInfo(
                stringSupplier.apply(R.string.menu_new_window), KeyEvent.KEYCODE_N,
                KeyEvent.META_CTRL_ON));
        data.add(group);
    }

    @Override
    public void showContextMenu(Fragment f, View v, float x, float y) {
        // Register context menu here so long-press doesn't trigger this context floating menu.
        f.registerForContextMenu(v);
        v.showContextMenu(x, y);
        f.unregisterForContextMenu(v);
    }

    @Override
    public void inflateContextMenuForContainer(
            Menu menu, MenuInflater inflater, SelectionDetails selectionDetails) {
        inflater.inflate(R.menu.container_context_menu, menu);
        updateContextMenuForContainer(menu, selectionDetails);
    }

    @Override
    public void inflateContextMenuForDocs(
            Menu menu, MenuInflater inflater, SelectionDetails selectionDetails) {
        final boolean hasDir = selectionDetails.containsDirectories();
        final boolean hasFile = selectionDetails.containsFiles();

        assert(hasDir || hasFile);
        if (!hasDir) {
            inflater.inflate(R.menu.file_context_menu, menu);
            updateContextMenuForFiles(menu, selectionDetails);
            return;
        }

        if (!hasFile) {
            inflater.inflate(R.menu.dir_context_menu, menu);
            updateContextMenuForDirs(menu, selectionDetails);
            return;
        }

        inflater.inflate(R.menu.mixed_context_menu, menu);
        updateContextMenu(menu, selectionDetails);
    }

    @Override
    protected void updateSettings(MenuItem settings, RootInfo root) {
        Menus.setEnabledAndVisible(settings, root.hasSettings());
    }

    @Override
    protected void updateEject(MenuItem eject, RootInfo root) {
        Menus.setEnabledAndVisible(eject, root.supportsEject() && !root.ejecting);
    }

    @Override
    protected void updateSettings(MenuItem settings) {
        boolean enabled = mDirDetails.hasRootSettings();
        Menus.setEnabledAndVisible(settings, enabled);
    }

    @Override
    protected void updateNewWindow(MenuItem newWindow) {
        Menus.setEnabledAndVisible(newWindow, true);
    }

    @Override
    protected void updateOpenWith(MenuItem openWith, SelectionDetails selectionDetails) {
        Menus.setEnabledAndVisible(openWith, selectionDetails.canOpenWith());
    }

    @Override
    protected void updateOpenInNewWindow(
            MenuItem openInNewWindow, SelectionDetails selectionDetails) {
        Menus.setEnabledAndVisible(openInNewWindow, selectionDetails.size() == 1
            && !selectionDetails.containsPartialFiles());
    }

    @Override
    protected void updateOpenInNewWindow(MenuItem openInNewWindow, RootInfo root) {
        assert(openInNewWindow.isVisible() && openInNewWindow.isEnabled());
    }

    @Override
    protected void updateMoveTo(MenuItem moveTo, SelectionDetails selectionDetails) {
        Menus.setEnabledAndVisible(moveTo,
                !selectionDetails.containsPartialFiles() && selectionDetails.canDelete());
    }

    @Override
    protected void updateCopyTo(MenuItem copyTo, SelectionDetails selectionDetails) {
        Menus.setEnabledAndVisible(copyTo, !selectionDetails.containsPartialFiles()
                && !selectionDetails.canExtract());
    }

    @Override
    protected void updateCompress(MenuItem compress, SelectionDetails selectionDetails) {
        final boolean readOnly = !mDirDetails.canCreateDoc();
        Menus.setEnabledAndVisible(compress, mFeatures.isArchiveCreationEnabled()
                && !readOnly
                && !selectionDetails.containsPartialFiles()
                && !selectionDetails.canExtract());
    }

    @Override
    protected void updateExtractTo(MenuItem extractTo, SelectionDetails selectionDetails) {
        boolean enabled = selectionDetails.canExtract();
        Menus.setEnabledAndVisible(extractTo, enabled);
    }

    @Override
    protected void updatePasteInto(MenuItem pasteInto, SelectionDetails selectionDetails) {
        Menus.setEnabledAndVisible(pasteInto,
                selectionDetails.canPasteInto() && mDirDetails.hasItemsToPaste());
    }

    @Override
    protected void updatePasteInto(MenuItem pasteInto, RootInfo root, DocumentInfo docInfo) {
        Menus.setEnabledAndVisible(pasteInto, root.supportsCreate()
                && docInfo != null
                && docInfo.isCreateSupported()
                && mDirDetails.hasItemsToPaste());
    }

    @Override
    protected void updateSelectAll(MenuItem selectAll) {
        Menus.setEnabledAndVisible(selectAll, true);
    }

    @Override
    protected void updateSelectAll(MenuItem selectAll, SelectionDetails selectionDetails) {
        final boolean visible = selectionDetails.size() < mFilesCountSupplier.getAsInt();
        Menus.setEnabledAndVisible(selectAll, visible);
    }

    @Override
    protected void updateDeselectAll(MenuItem deselectAll, SelectionDetails selectionDetails) {
        final boolean visible = selectionDetails.size() == mFilesCountSupplier.getAsInt();
        Menus.setEnabledAndVisible(deselectAll, visible);
    }

    @Override
    protected void updateCreateDir(MenuItem createDir) {
        Menus.setEnabledAndVisible(createDir, mDirDetails.canCreateDirectory());
    }

    @Override
    protected void updateShare(MenuItem share, SelectionDetails selectionDetails) {
        boolean enabled = !selectionDetails.containsDirectories()
                && !selectionDetails.containsPartialFiles()
                && !selectionDetails.canExtract();
        Menus.setEnabledAndVisible(share, enabled);
    }

    @Override
    protected void updateDelete(MenuItem delete, SelectionDetails selectionDetails) {
        boolean enabled = selectionDetails.canDelete();
        Menus.setEnabledAndVisible(delete, enabled);
    }

    @Override
    protected void updateRename(MenuItem rename, SelectionDetails selectionDetails) {
        Menus.setEnabledAndVisible(rename,
                !selectionDetails.containsPartialFiles() && selectionDetails.canRename());
    }

    @Override
    protected void updateInspect(MenuItem inspect) {
        boolean visible = mFeatures.isInspectorEnabled();
        Menus.setEnabledAndVisible(inspect, visible && mDirDetails.canInspectDirectory());
    }

    @Override
    protected void updateInspect(MenuItem inspect, SelectionDetails selectionDetails) {
        boolean visible = mFeatures.isInspectorEnabled() && selectionDetails.size() <= 1;
        Menus.setEnabledAndVisible(inspect, visible);
    }

    @Override
    protected void updateViewInOwner(MenuItem view, SelectionDetails selectionDetails) {
        if (selectionDetails.canViewInOwner() &&
                mSelectionManager.getSelection().iterator().hasNext()) {
            Menus.setEnabledAndVisible(view, true);
            Resources res = mContext.getResources();
            String selectedModelId = mSelectionManager.getSelection().iterator().next();
            Uri selectedUri = mUriLookup.lookup(selectedModelId);
            String appName = mAppNameLookup.getApplicationName(UserId.DEFAULT_USER,
                    selectedUri.getAuthority());
            String title = res.getString(R.string.menu_view_in_owner, appName);
            view.setTitle(title);
        }
        else {
            Menus.setEnabledAndVisible(view, false);
        }
    }

    @Override
    protected void updateLauncher(MenuItem launcher) {
        Menus.setEnabledAndVisible(launcher, mState.debugMode);
        launcher.setTitle(Shared.isLauncherEnabled(mContext)
                ? "Hide launcher icon" : "Show launcher icon");
    }
}
