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

import static junit.framework.Assert.assertEquals;

import static org.junit.Assert.assertTrue;

import android.net.Uri;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsContract.Root;

import androidx.recyclerview.selection.SelectionTracker;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.documentsui.R;
import com.android.documentsui.SelectionHelpers;
import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;
import com.android.documentsui.dirlist.TestData;
import com.android.documentsui.testing.TestDirectoryDetails;
import com.android.documentsui.testing.TestEnv;
import com.android.documentsui.testing.TestFeatures;
import com.android.documentsui.testing.TestMenu;
import com.android.documentsui.testing.TestMenuInflater;
import com.android.documentsui.testing.TestMenuItem;
import com.android.documentsui.testing.TestSearchViewManager;
import com.android.documentsui.testing.TestSelectionDetails;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@SmallTest
public final class MenuManagerTest {

    private TestMenu testMenu;

    /* Directory Context Menu items */
    private TestMenuItem dirShare;
    private TestMenuItem dirOpen;
    private TestMenuItem dirOpenWith;
    private TestMenuItem dirCutToClipboard;
    private TestMenuItem dirCopyToClipboard;
    private TestMenuItem dirPasteFromClipboard;
    private TestMenuItem dirCreateDir;
    private TestMenuItem dirSelectAll;
    private TestMenuItem mDirDeselectAll;
    private TestMenuItem dirRename;
    private TestMenuItem dirDelete;
    private TestMenuItem dirViewInOwner;
    private TestMenuItem dirPasteIntoFolder;
    private TestMenuItem dirInspect;
    private TestMenuItem dirOpenInNewWindow;

    /* Root List Context Menu items */
    private TestMenuItem rootEjectRoot;
    private TestMenuItem rootOpenInNewWindow;
    private TestMenuItem rootPasteIntoFolder;
    private TestMenuItem rootSettings;

    /* Action Mode menu items */
    private TestMenuItem actionModeOpen;
    private TestMenuItem actionModeOpenWith;
    private TestMenuItem actionModeShare;
    private TestMenuItem actionModeDelete;
    private TestMenuItem actionModeSelectAll;
    private TestMenuItem mActionModeDeselectAll;
    private TestMenuItem actionModeCopyTo;
    private TestMenuItem actionModeExtractTo;
    private TestMenuItem actionModeMoveTo;
    private TestMenuItem actionModeCompress;
    private TestMenuItem actionModeRename;
    private TestMenuItem actionModeViewInOwner;
    private TestMenuItem actionModeInspector;
    private TestMenuItem actionModeSort;

    /* Option Menu items */
    private TestMenuItem optionSearch;
    private TestMenuItem optionDebug;
    private TestMenuItem optionNewWindow;
    private TestMenuItem optionCreateDir;
    private TestMenuItem optionSelectAll;
    private TestMenuItem optionSettings;
    private TestMenuItem optionInspector;
    private TestMenuItem optionSort;
    private TestMenuItem mOptionLauncher;
    private TestMenuItem mOptionShowHiddenFiles;

    /* Sub Option Menu items */
    private TestMenuItem subOptionGrid;
    private TestMenuItem subOptionList;

    private TestFeatures features;
    private TestSelectionDetails selectionDetails;
    private TestDirectoryDetails dirDetails;
    private TestSearchViewManager testSearchManager;
    private RootInfo testRootInfo;
    private DocumentInfo testDocInfo;
    private State state = new State();
    private MenuManager mgr;
    private TestActivity activity = TestActivity.create(TestEnv.create());
    private SelectionTracker<String> selectionManager;

    private int mFilesCount;

    @Before
    public void setUp() {
        testMenu = TestMenu.create();

        // The context menu on anything in DirectoryList (including no selection).
        dirShare = testMenu.findItem(R.id.dir_menu_share);
        dirOpen = testMenu.findItem(R.id.dir_menu_open);
        dirOpenWith = testMenu.findItem(R.id.dir_menu_open_with);
        dirCutToClipboard = testMenu.findItem(R.id.dir_menu_cut_to_clipboard);
        dirCopyToClipboard = testMenu.findItem(R.id.dir_menu_copy_to_clipboard);
        dirPasteFromClipboard = testMenu.findItem(R.id.dir_menu_paste_from_clipboard);
        dirCreateDir = testMenu.findItem(R.id.dir_menu_create_dir);
        dirSelectAll = testMenu.findItem(R.id.dir_menu_select_all);
        mDirDeselectAll = testMenu.findItem(R.id.dir_menu_deselect_all);
        dirRename = testMenu.findItem(R.id.dir_menu_rename);
        dirDelete = testMenu.findItem(R.id.dir_menu_delete);
        dirViewInOwner = testMenu.findItem(R.id.dir_menu_view_in_owner);
        dirPasteIntoFolder = testMenu.findItem(R.id.dir_menu_paste_into_folder);
        dirInspect = testMenu.findItem(R.id.dir_menu_inspect);
        dirOpenInNewWindow = testMenu.findItem(R.id.dir_menu_open_in_new_window);

        rootEjectRoot = testMenu.findItem(R.id.root_menu_eject_root);
        rootOpenInNewWindow = testMenu.findItem(R.id.root_menu_open_in_new_window);
        rootPasteIntoFolder = testMenu.findItem(R.id.root_menu_paste_into_folder);
        rootSettings = testMenu.findItem(R.id.root_menu_settings);

        // Menu actions (including overflow) when action mode *is* active.
        actionModeOpenWith = testMenu.findItem(R.id.action_menu_open_with);
        actionModeShare = testMenu.findItem(R.id.action_menu_share);
        actionModeDelete = testMenu.findItem(R.id.action_menu_delete);
        actionModeSelectAll = testMenu.findItem(R.id.action_menu_select_all);
        mActionModeDeselectAll = testMenu.findItem(R.id.action_menu_deselect_all);
        actionModeCopyTo = testMenu.findItem(R.id.action_menu_copy_to);
        actionModeExtractTo = testMenu.findItem(R.id.action_menu_extract_to);
        actionModeMoveTo = testMenu.findItem(R.id.action_menu_move_to);
        actionModeCompress = testMenu.findItem(R.id.action_menu_compress);
        actionModeRename = testMenu.findItem(R.id.action_menu_rename);
        actionModeInspector = testMenu.findItem(R.id.action_menu_inspect);
        actionModeViewInOwner = testMenu.findItem(R.id.action_menu_view_in_owner);
        actionModeSort = testMenu.findItem(R.id.action_menu_sort);

        // Menu actions (including overflow) when action mode is not active.
        optionSearch = testMenu.findItem(R.id.option_menu_search);
        optionDebug = testMenu.findItem(R.id.option_menu_debug);
        optionNewWindow = testMenu.findItem(R.id.option_menu_new_window);
        optionCreateDir = testMenu.findItem(R.id.option_menu_create_dir);
        optionSelectAll = testMenu.findItem(R.id.option_menu_select_all);
        optionSettings = testMenu.findItem(R.id.option_menu_settings);
        optionInspector = testMenu.findItem(R.id.option_menu_inspect);
        optionSort = testMenu.findItem(R.id.option_menu_sort);
        mOptionLauncher = testMenu.findItem(R.id.option_menu_launcher);
        mOptionShowHiddenFiles = testMenu.findItem(R.id.option_menu_show_hidden_files);

        // Menu actions on root title row.
        subOptionGrid = testMenu.findItem(R.id.sub_menu_grid);
        subOptionList = testMenu.findItem(R.id.sub_menu_list);

        features = new TestFeatures();

        // These items by default are visible
        testMenu.findItem(R.id.dir_menu_select_all).setVisible(true);
        testMenu.findItem(R.id.option_menu_select_all).setVisible(true);
        testMenu.findItem(R.id.sub_menu_list).setVisible(true);

        selectionDetails = new TestSelectionDetails();
        dirDetails = new TestDirectoryDetails();
        testSearchManager = new TestSearchViewManager();
        selectionManager = SelectionHelpers.createTestInstance(TestData.create(1));
        selectionManager.select("0");

        selectionDetails.size = 1;
        mFilesCount = 10;

        mgr = new MenuManager(
                features,
                testSearchManager,
                state,
                dirDetails,
                activity,
                selectionManager,
                this::getApplicationNameFromAuthority,
                this::getUriFromModelId,
                this::getFilesCount);

        testRootInfo = new RootInfo();
        testDocInfo = new DocumentInfo();
        state.stack.push(testDocInfo);
    }

    private Uri getUriFromModelId(String id) {
        return Uri.EMPTY;
    }

    private String getApplicationNameFromAuthority(UserId userId, String authority) {
        return "TestApp";
    }

    private int getFilesCount() {
        return mFilesCount;
    }

    @Test
    public void testActionMenu() {
        selectionDetails.canDelete = true;
        selectionDetails.canRename = true;
        dirDetails.canCreateDoc = true;

        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeRename.assertEnabledAndVisible();
        actionModeDelete.assertEnabledAndVisible();
        actionModeShare.assertEnabledAndVisible();
        actionModeCopyTo.assertEnabledAndVisible();
        actionModeCompress.assertEnabledAndVisible();
        actionModeExtractTo.assertDisabledAndInvisible();
        actionModeMoveTo.assertEnabledAndVisible();
        actionModeViewInOwner.assertDisabledAndInvisible();
        actionModeSort.assertEnabledAndVisible();
        actionModeSelectAll.assertEnabledAndVisible();
        mActionModeDeselectAll.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_ContainsPartial() {
        selectionDetails.containPartial = true;
        dirDetails.canCreateDoc = true;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeRename.assertDisabledAndInvisible();
        actionModeShare.assertDisabledAndInvisible();
        actionModeCopyTo.assertDisabledAndInvisible();
        actionModeCompress.assertDisabledAndInvisible();
        actionModeExtractTo.assertDisabledAndInvisible();
        actionModeMoveTo.assertDisabledAndInvisible();
        actionModeViewInOwner.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_CreateArchives_ReflectsFeatureState() {
        features.archiveCreation = false;
        dirDetails.canCreateDoc = true;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeCompress.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_CreateArchive() {
        dirDetails.canCreateDoc = true;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeCompress.assertEnabledAndVisible();
    }

    @Test
    public void testActionMenu_NoCreateArchive() {
        dirDetails.canCreateDoc = false;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeCompress.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_cantRename() {
        selectionDetails.canRename = false;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeRename.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_cantDelete() {
        selectionDetails.canDelete = false;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeDelete.assertDisabledAndInvisible();
        // We shouldn't be able to move files if we can't delete them
        actionModeMoveTo.assertDisabledAndInvisible();
    }

    @Test
    public void testActionsMenu_canViewInOwner() {
        activity.resources.strings.put(R.string.menu_view_in_owner, "Insert name here! %s");
        selectionDetails.canViewInOwner = true;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeViewInOwner.assertEnabledAndVisible();
    }

    @Test
    public void testActionsMenu_cantViewInOwner_noSelection() {
        // Simulate empty selection
        selectionManager = SelectionHelpers.createTestInstance();
        mgr = new MenuManager(
                features,
                testSearchManager,
                state,
                dirDetails,
                activity,
                selectionManager,
                this::getApplicationNameFromAuthority,
                this::getUriFromModelId,
                this::getFilesCount);

        selectionDetails.canViewInOwner = true;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeViewInOwner.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_changeToCanDelete() {
        selectionDetails.canDelete = false;
        mgr.updateActionMenu(testMenu, selectionDetails);

        selectionDetails.canDelete = true;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeDelete.assertEnabledAndVisible();
        actionModeMoveTo.assertEnabledAndVisible();
    }

    @Test
    public void testActionMenu_ContainsDirectory() {
        selectionDetails.containDirectories = true;
        mgr.updateActionMenu(testMenu, selectionDetails);

        // We can't share directories
        actionModeShare.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_RemovesDirectory() {
        selectionDetails.containDirectories = true;
        mgr.updateActionMenu(testMenu, selectionDetails);

        selectionDetails.containDirectories = false;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeShare.assertEnabledAndVisible();
    }

    @Test
    public void testActionMenu_CantExtract() {
        selectionDetails.canExtract = false;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeExtractTo.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_CanExtract_hidesCopyToAndCompressAndShare() {
        features.archiveCreation = true;
        selectionDetails.canExtract = true;
        dirDetails.canCreateDoc = true;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeExtractTo.assertEnabledAndVisible();
        actionModeCopyTo.assertDisabledAndInvisible();
        actionModeCompress.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_CanOpenWith() {
        selectionDetails.canOpenWith = true;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeOpenWith.assertEnabledAndVisible();
    }

    @Test
    public void testActionMenu_NoOpenWith() {
        selectionDetails.canOpenWith = false;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeOpenWith.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_Inspector_EnabledForSingleSelection() {
        features.inspector = true;
        selectionDetails.size = 1;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeInspector.assertEnabledAndVisible();
    }

    @Test
    public void testActionMenu_Inspector_DisabledForMultiSelection() {
        features.inspector = true;
        selectionDetails.size = 2;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeInspector.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_CanDeselectAll() {
        selectionDetails.size = 1;
        mFilesCount = 1;

        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeSelectAll.assertDisabledAndInvisible();
        mActionModeDeselectAll.assertEnabledAndVisible();
    }

    @Test
    public void testOptionMenu() {
        mgr.updateOptionMenu(testMenu);

        optionCreateDir.assertDisabledAndInvisible();
        optionDebug.assertDisabledAndInvisible();
        optionSort.assertEnabledAndVisible();
        mOptionLauncher.assertDisabledAndInvisible();
        mOptionShowHiddenFiles.assertEnabledAndVisible();
        assertTrue(testSearchManager.updateMenuCalled());
    }

    @Test
    public void testOptionMenu_CanCreateDirectory() {
        dirDetails.canCreateDirectory = true;
        mgr.updateOptionMenu(testMenu);

        optionCreateDir.assertEnabledAndVisible();
    }

    @Test
    public void testOptionMenu_HasRootSettings() {
        dirDetails.hasRootSettings = true;
        mgr.updateOptionMenu(testMenu);

        optionSettings.assertEnabledAndVisible();
    }

    @Test
    public void testOptionMenu_Inspector_VisibleAndEnabled() {
        features.inspector = true;
        dirDetails.canInspectDirectory = true;
        mgr.updateOptionMenu(testMenu);
        optionInspector.assertEnabledAndVisible();
    }

    @Test
    public void testOptionMenu_Inspector_VisibleButDisabled() {
        features.inspector = true;
        dirDetails.canInspectDirectory = false;
        mgr.updateOptionMenu(testMenu);
        optionInspector.assertDisabledAndInvisible();
    }

    @Test
    public void testInflateContextMenu_Files() {
        TestMenuInflater inflater = new TestMenuInflater();

        selectionDetails.containFiles = true;
        selectionDetails.containDirectories = false;
        mgr.inflateContextMenuForDocs(testMenu, inflater, selectionDetails);

        assertEquals(R.menu.file_context_menu, inflater.lastInflatedMenuId);
    }

    @Test
    public void testInflateContextMenu_Dirs() {
        TestMenuInflater inflater = new TestMenuInflater();

        selectionDetails.containFiles = false;
        selectionDetails.containDirectories = true;
        mgr.inflateContextMenuForDocs(testMenu, inflater, selectionDetails);

        assertEquals(R.menu.dir_context_menu, inflater.lastInflatedMenuId);
    }

    @Test
    public void testInflateContextMenu_Mixed() {
        TestMenuInflater inflater = new TestMenuInflater();

        selectionDetails.containFiles = true;
        selectionDetails.containDirectories = true;
        mgr.inflateContextMenuForDocs(testMenu, inflater, selectionDetails);

        assertEquals(R.menu.mixed_context_menu, inflater.lastInflatedMenuId);
    }

    @Test
    public void testContextMenu_EmptyArea() {
        mgr.updateContextMenuForContainer(testMenu, selectionDetails);

        dirSelectAll.assertEnabledAndVisible();
        mDirDeselectAll.assertDisabledAndInvisible();
        dirPasteFromClipboard.assertDisabledAndInvisible();
        dirCreateDir.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_EmptyArea_CanDeselectAll() {
        selectionDetails.size = 1;
        mFilesCount = 1;

        mgr.updateContextMenuForContainer(testMenu, selectionDetails);

        dirSelectAll.assertDisabledAndInvisible();
        mDirDeselectAll.assertEnabledAndVisible();
    }

    @Test
    public void testContextMenu_EmptyArea_NoItemToPaste() {
        dirDetails.hasItemsToPaste = false;
        dirDetails.canCreateDoc = true;

        mgr.updateContextMenuForContainer(testMenu, selectionDetails);

        dirSelectAll.assertEnabledAndVisible();
        dirPasteFromClipboard.assertDisabledAndInvisible();
        dirCreateDir.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_EmptyArea_CantCreateDoc() {
        dirDetails.hasItemsToPaste = true;
        dirDetails.canCreateDoc = false;

        mgr.updateContextMenuForContainer(testMenu, selectionDetails);

        dirSelectAll.assertEnabledAndVisible();
        dirPasteFromClipboard.assertDisabledAndInvisible();
        dirCreateDir.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_EmptyArea_CanPaste() {
        dirDetails.hasItemsToPaste = true;
        dirDetails.canCreateDoc = true;

        mgr.updateContextMenuForContainer(testMenu, selectionDetails);

        dirSelectAll.assertEnabledAndVisible();
        dirPasteFromClipboard.assertEnabledAndVisible();
        dirCreateDir.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_EmptyArea_CanCreateDirectory() {
        dirDetails.canCreateDirectory = true;

        mgr.updateContextMenuForContainer(testMenu, selectionDetails);

        dirSelectAll.assertEnabledAndVisible();
        dirPasteFromClipboard.assertDisabledAndInvisible();
        dirCreateDir.assertEnabledAndVisible();
    }

    @Test
    public void testContextMenu_OnFile() {
        selectionDetails.size = 1;
        mgr.updateContextMenuForFiles(testMenu, selectionDetails);
        dirOpen.assertDisabledAndInvisible();
        dirCutToClipboard.assertDisabledAndInvisible();
        dirCopyToClipboard.assertEnabledAndVisible();
        dirRename.assertDisabledAndInvisible();
        dirCreateDir.assertEnabledAndVisible();
        dirDelete.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_OnFile_CanOpenWith() {
        selectionDetails.canOpenWith = true;
        mgr.updateContextMenuForFiles(testMenu, selectionDetails);
        dirOpenWith.assertEnabledAndVisible();
    }

    @Test
    public void testContextMenu_OnFile_NoOpenWith() {
        selectionDetails.canOpenWith = false;
        mgr.updateContextMenuForFiles(testMenu, selectionDetails);
        dirOpenWith.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_OnMultipleFiles() {
        selectionDetails.size = 3;
        mgr.updateContextMenuForFiles(testMenu, selectionDetails);
        dirOpen.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_OnWritableDirectory() {
        selectionDetails.size = 1;
        selectionDetails.canPasteInto = true;
        dirDetails.hasItemsToPaste = true;
        mgr.updateContextMenuForDirs(testMenu, selectionDetails);
        dirOpenInNewWindow.assertEnabledAndVisible();
        dirCutToClipboard.assertDisabledAndInvisible();
        dirCopyToClipboard.assertEnabledAndVisible();
        dirPasteIntoFolder.assertEnabledAndVisible();
        dirRename.assertDisabledAndInvisible();
        dirDelete.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_OnNonWritableDirectory() {
        selectionDetails.size = 1;
        selectionDetails.canPasteInto = false;
        mgr.updateContextMenuForDirs(testMenu, selectionDetails);
        dirOpenInNewWindow.assertEnabledAndVisible();
        dirCutToClipboard.assertDisabledAndInvisible();
        dirCopyToClipboard.assertEnabledAndVisible();
        dirPasteIntoFolder.assertDisabledAndInvisible();
        dirRename.assertDisabledAndInvisible();
        dirDelete.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_CanInspectContainer() {
        features.inspector = true;
        dirDetails.canInspectDirectory = true;
        mgr.updateContextMenuForContainer(testMenu, selectionDetails);
        dirInspect.assertEnabledAndVisible();
    }

    @Test
    public void testContextMenu_OnWritableDirectory_NothingToPaste() {
        selectionDetails.canPasteInto = true;
        selectionDetails.size = 1;
        dirDetails.hasItemsToPaste = false;
        mgr.updateContextMenuForDirs(testMenu, selectionDetails);
        dirPasteIntoFolder.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_OnMultipleDirectories() {
        selectionDetails.size = 3;
        mgr.updateContextMenuForDirs(testMenu, selectionDetails);
        dirOpenInNewWindow.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_OnMixedDocs() {
        selectionDetails.containDirectories = true;
        selectionDetails.containFiles = true;
        selectionDetails.size = 2;
        selectionDetails.canDelete = true;
        mgr.updateContextMenu(testMenu, selectionDetails);
        dirCutToClipboard.assertEnabledAndVisible();
        dirCopyToClipboard.assertEnabledAndVisible();
        dirDelete.assertEnabledAndVisible();
    }

    @Test
    public void testContextMenu_OnMixedDocs_hasPartialFile() {
        selectionDetails.containDirectories = true;
        selectionDetails.containFiles = true;
        selectionDetails.size = 2;
        selectionDetails.containPartial = true;
        selectionDetails.canDelete = true;
        mgr.updateContextMenu(testMenu, selectionDetails);
        dirCutToClipboard.assertDisabledAndInvisible();
        dirCopyToClipboard.assertDisabledAndInvisible();
        dirDelete.assertEnabledAndVisible();
    }

    @Test
    public void testContextMenu_OnMixedDocs_hasUndeletableFile() {
        selectionDetails.containDirectories = true;
        selectionDetails.containFiles = true;
        selectionDetails.size = 2;
        selectionDetails.canDelete = false;
        mgr.updateContextMenu(testMenu, selectionDetails);
        dirCutToClipboard.assertDisabledAndInvisible();
        dirCopyToClipboard.assertEnabledAndVisible();
        dirDelete.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_CanInspectSingleSelection() {
        selectionDetails.size = 1;
        mgr.updateContextMenuForFiles(testMenu, selectionDetails);
        dirInspect.assertEnabledAndVisible();
    }

    @Test
    public void testRootContextMenu() {
        testRootInfo.flags = Root.FLAG_SUPPORTS_CREATE;

        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootEjectRoot.assertDisabledAndInvisible();
        rootOpenInNewWindow.assertEnabledAndVisible();
        rootPasteIntoFolder.assertDisabledAndInvisible();
        rootSettings.assertDisabledAndInvisible();
    }

    @Test
    public void testRootContextMenu_HasRootSettings() {
        testRootInfo.flags = Root.FLAG_HAS_SETTINGS;
        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootSettings.assertEnabledAndVisible();
    }

    @Test
    public void testRootContextMenu_NonWritableRoot() {
        dirDetails.hasItemsToPaste = true;
        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootPasteIntoFolder.assertDisabledAndInvisible();
    }

    @Test
    public void testRootContextMenu_NothingToPaste() {
        testRootInfo.flags = Root.FLAG_SUPPORTS_CREATE;
        testDocInfo.flags = Document.FLAG_DIR_SUPPORTS_CREATE;
        dirDetails.hasItemsToPaste = false;
        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootPasteIntoFolder.assertDisabledAndInvisible();
    }

    @Test
    public void testRootContextMenu_PasteIntoWritableRoot() {
        testRootInfo.flags = Root.FLAG_SUPPORTS_CREATE;
        testDocInfo.flags = Document.FLAG_DIR_SUPPORTS_CREATE;
        dirDetails.hasItemsToPaste = true;
        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootPasteIntoFolder.assertEnabledAndVisible();
    }

    @Test
    public void testRootContextMenu_Eject() {
        testRootInfo.flags = Root.FLAG_SUPPORTS_EJECT;
        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootEjectRoot.assertEnabledAndVisible();
    }

    @Test
    public void testRootContextMenu_EjectInProcess() {
        testRootInfo.flags = Root.FLAG_SUPPORTS_EJECT;
        testRootInfo.ejecting = true;
        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootEjectRoot.assertDisabledAndInvisible();
    }
}
