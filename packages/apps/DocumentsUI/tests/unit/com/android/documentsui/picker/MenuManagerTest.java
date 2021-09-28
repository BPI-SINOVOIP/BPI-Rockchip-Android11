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

package com.android.documentsui.picker;

import static com.android.documentsui.base.State.ACTION_CREATE;
import static com.android.documentsui.base.State.ACTION_GET_CONTENT;
import static com.android.documentsui.base.State.ACTION_OPEN;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.database.MatrixCursor;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsContract.Root;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.documentsui.DirectoryResult;
import com.android.documentsui.Model;
import com.android.documentsui.R;
import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.State;
import com.android.documentsui.roots.RootCursorWrapper;
import com.android.documentsui.testing.TestDirectoryDetails;
import com.android.documentsui.testing.TestFeatures;
import com.android.documentsui.testing.TestMenu;
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
    private TestMenuItem dirOpenInNewWindow;
    private TestMenuItem dirPasteIntoFolder;

    /* Root List Context Menu items */
    private TestMenuItem rootEjectRoot;
    private TestMenuItem rootOpenInNewWindow;
    private TestMenuItem rootPasteIntoFolder;
    private TestMenuItem rootSettings;

    /* Action Mode menu items */
    private TestMenuItem actionModeOpen;
    private TestMenuItem actionModeOpenWith;
    private TestMenuItem actionModeSelect;
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
    private TestMenuItem actionModeSort;

    /* Option Menu items */
    private TestMenuItem optionSearch;
    private TestMenuItem optionDebug;
    private TestMenuItem optionNewWindow;
    private TestMenuItem optionCreateDir;
    private TestMenuItem optionSelectAll;
    private TestMenuItem optionSettings;
    private TestMenuItem optionSort;
    private TestMenuItem mOptionLauncher;
    private TestMenuItem mOptionShowHiddenFiles;

    private TestMenuItem subOptionGrid;
    private TestMenuItem subOptionList;

    private TestSelectionDetails selectionDetails;
    private TestDirectoryDetails dirDetails;
    private TestSearchViewManager testSearchManager;
    private State state = new State();
    private RootInfo testRootInfo;
    private DocumentInfo testDocInfo;
    private MenuManager mgr;

    private int mFilesCount;

    @Before
    public void setUp() {
        testMenu = TestMenu.create();
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
        dirOpenInNewWindow = testMenu.findItem(R.id.dir_menu_open_in_new_window);
        dirPasteIntoFolder = testMenu.findItem(R.id.dir_menu_paste_into_folder);

        rootEjectRoot = testMenu.findItem(R.id.root_menu_eject_root);
        rootOpenInNewWindow = testMenu.findItem(R.id.root_menu_open_in_new_window);
        rootPasteIntoFolder = testMenu.findItem(R.id.root_menu_paste_into_folder);
        rootSettings = testMenu.findItem(R.id.root_menu_settings);

        actionModeOpenWith = testMenu.findItem(R.id.action_menu_open_with);
        actionModeSelect = testMenu.findItem(R.id.action_menu_select);
        actionModeShare = testMenu.findItem(R.id.action_menu_share);
        actionModeDelete = testMenu.findItem(R.id.action_menu_delete);
        actionModeSelectAll = testMenu.findItem(R.id.action_menu_select_all);
        mActionModeDeselectAll = testMenu.findItem(R.id.action_menu_deselect_all);
        actionModeCopyTo = testMenu.findItem(R.id.action_menu_copy_to);
        actionModeExtractTo = testMenu.findItem(R.id.action_menu_extract_to);
        actionModeMoveTo = testMenu.findItem(R.id.action_menu_move_to);
        actionModeCompress = testMenu.findItem(R.id.action_menu_compress);
        actionModeRename = testMenu.findItem(R.id.action_menu_rename);
        actionModeViewInOwner = testMenu.findItem(R.id.action_menu_view_in_owner);
        actionModeSort = testMenu.findItem(R.id.action_menu_sort);

        optionSearch = testMenu.findItem(R.id.option_menu_search);
        optionDebug = testMenu.findItem(R.id.option_menu_debug);
        optionNewWindow = testMenu.findItem(R.id.option_menu_new_window);
        optionCreateDir = testMenu.findItem(R.id.option_menu_create_dir);
        optionSelectAll = testMenu.findItem(R.id.option_menu_select_all);
        optionSettings = testMenu.findItem(R.id.option_menu_settings);
        optionSort = testMenu.findItem(R.id.option_menu_sort);
        mOptionLauncher = testMenu.findItem(R.id.option_menu_launcher);
        mOptionShowHiddenFiles = testMenu.findItem(R.id.option_menu_show_hidden_files);

        // Menu actions on root title row.
        subOptionGrid = testMenu.findItem(R.id.sub_menu_grid);
        subOptionList = testMenu.findItem(R.id.sub_menu_list);

        selectionDetails = new TestSelectionDetails();
        dirDetails = new TestDirectoryDetails();
        testSearchManager = new TestSearchViewManager();
        mgr = new MenuManager(testSearchManager, state, dirDetails, this::getFilesCount);
        selectionDetails.size = 1;
        mFilesCount = 10;

        testRootInfo = new RootInfo();
        testDocInfo = new DocumentInfo();
        state.action = ACTION_CREATE;
        state.allowMultiple = true;
    }

    private int getFilesCount() {
        return mFilesCount;
    }

    @Test
    public void testActionMenu() {
        mgr.updateActionMenu(testMenu, selectionDetails);
        actionModeSelect.assertDisabledAndInvisible();
        actionModeDelete.assertDisabledAndInvisible();
        actionModeShare.assertDisabledAndInvisible();
        actionModeRename.assertDisabledAndInvisible();
        actionModeSelectAll.assertEnabledAndVisible();
        mActionModeDeselectAll.assertDisabledAndInvisible();
        actionModeViewInOwner.assertDisabledAndInvisible();
        actionModeSort.assertEnabledAndVisible();
    }

    @Test
    public void testActionMenu_selectAction() {
        state.action = ACTION_OPEN;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeSelect.assertEnabledAndVisible();
    }

    @Test
    public void testActionMenu_selectActionTitle() {
        state.action = ACTION_OPEN;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeSelect.assertTitle(R.string.menu_select);
    }

    @Test
    public void testActionMenu_getContentAction() {
        state.action = ACTION_GET_CONTENT;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeSelect.assertEnabledAndVisible();
    }

    @Test
    public void testActionMenu_getContentActionTitle() {
        state.action = ACTION_GET_CONTENT;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeSelect.assertTitle(R.string.menu_select);
    }

    @Test
    public void testActionMenu_notAllowMultiple() {
        state.allowMultiple = false;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeSelectAll.assertDisabledAndInvisible();
        mActionModeDeselectAll.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_AllowMultiple() {
        state.allowMultiple = true;
        mgr.updateActionMenu(testMenu, selectionDetails);

        actionModeSelectAll.assertEnabledAndVisible();
        mActionModeDeselectAll.assertDisabledAndInvisible();
    }

    @Test
    public void testActionMenu_CanDeselectAll() {
        state.allowMultiple = true;
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
        assertTrue(testSearchManager.showMenuCalled());
    }

    @Test
    public void testOptionMenu_notPicking() {
        state.action = ACTION_OPEN;
        state.derivedMode = State.MODE_LIST;
        mgr.updateOptionMenu(testMenu);
        mgr.updateSubMenu(testMenu);

        optionCreateDir.assertDisabledAndInvisible();
        subOptionGrid.assertEnabledAndVisible();
        subOptionList.assertDisabledAndInvisible();
        assertFalse(testSearchManager.showMenuCalled());
    }

    @Test
    public void testOptionMenu_canCreateDirectory() {
        dirDetails.canCreateDirectory = true;
        mgr.updateOptionMenu(testMenu);

        optionCreateDir.assertEnabledAndVisible();
    }

    @Test
    public void testOptionMenu_inRecents() {
        dirDetails.isInRecents = true;
        mgr.updateOptionMenu(testMenu);
        mgr.updateSubMenu(testMenu);

        subOptionGrid.assertDisabledAndInvisible();
        subOptionList.assertDisabledAndInvisible();
    }


    @Test
    public void testOptionMenu_onlyContainer() {
        state.allowMultiple = true;
        mgr.updateModel(getTestModel(true));
        mgr.updateOptionMenu(testMenu);

        optionSelectAll.assertDisabledAndInvisible();
    }

    @Test
    public void testOptionMenu_containerAndFile() {
        state.allowMultiple = true;
        mgr.updateModel(getTestModel(false));
        mgr.updateOptionMenu(testMenu);

        optionSelectAll.assertEnabledAndVisible();
    }

    @Test
    public void testContextMenu_EmptyArea() {
        dirDetails.hasItemsToPaste = false;
        dirDetails.canCreateDoc = false;
        dirDetails.canCreateDirectory = false;

        mgr.updateContextMenuForContainer(testMenu, selectionDetails);

        dirSelectAll.assertEnabledAndVisible();
        mDirDeselectAll.assertDisabledAndInvisible();
        dirPasteFromClipboard.assertDisabledAndInvisible();
        dirCreateDir.assertDisabledAndInvisible();
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
    public void testContextMenu_EmptyArea_canPaste() {
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
    public void testContextMenu_EmptyArea_CanDeselectAll() {
        selectionDetails.size = 1;
        mFilesCount = 1;

        mgr.updateContextMenuForContainer(testMenu, selectionDetails);

        dirSelectAll.assertDisabledAndInvisible();
        mDirDeselectAll.assertEnabledAndVisible();
    }

    @Test
    public void testContextMenu_OnFile() {
        mgr.updateContextMenuForFiles(testMenu, selectionDetails);
        // We don't want share in pickers.
        dirShare.assertDisabledAndInvisible();
        // We don't want openWith in pickers.
        dirOpenWith.assertDisabledAndInvisible();
        dirCutToClipboard.assertDisabledAndInvisible();
        dirCopyToClipboard.assertEnabledAndVisible();
        dirRename.assertDisabledAndInvisible();
        dirDelete.assertDisabledAndInvisible();
    }

    @Test
    public void testContextMenu_OnDirectory() {
        selectionDetails.canPasteInto = true;
        mgr.updateContextMenuForDirs(testMenu, selectionDetails);
        // We don't want openInNewWindow in pickers
        dirOpenInNewWindow.assertDisabledAndInvisible();
        dirCutToClipboard.assertDisabledAndInvisible();
        dirCopyToClipboard.assertEnabledAndVisible();
        // Doesn't matter if directory is selected, we don't want pasteInto for PickerActivity
        dirPasteIntoFolder.assertDisabledAndInvisible();
        dirRename.assertDisabledAndInvisible();
        dirDelete.assertDisabledAndInvisible();
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
    public void testRootContextMenu() {
        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootEjectRoot.assertDisabledAndInvisible();
        rootOpenInNewWindow.assertDisabledAndInvisible();
        rootPasteIntoFolder.assertDisabledAndInvisible();
        rootSettings.assertDisabledAndInvisible();
    }

    @Test
    public void testRootContextMenu_hasRootSettings() {
        testRootInfo.flags = Root.FLAG_HAS_SETTINGS;
        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootSettings.assertDisabledAndInvisible();
    }

    @Test
    public void testRootContextMenu_nonWritableRoot() {
        dirDetails.hasItemsToPaste = true;
        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootPasteIntoFolder.assertDisabledAndInvisible();
    }

    @Test
    public void testRootContextMenu_nothingToPaste() {
        testRootInfo.flags = Root.FLAG_SUPPORTS_CREATE;
        dirDetails.hasItemsToPaste = false;
        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootPasteIntoFolder.assertDisabledAndInvisible();
    }

    @Test
    public void testRootContextMenu_canEject() {
        testRootInfo.flags = Root.FLAG_SUPPORTS_EJECT;
        mgr.updateRootContextMenu(testMenu, testRootInfo, testDocInfo);

        rootEjectRoot.assertDisabledAndInvisible();
    }

    private Model getTestModel(boolean onlyDirectory) {
        String[] COLUMNS = new String[]{
                RootCursorWrapper.COLUMN_AUTHORITY,
                Document.COLUMN_DOCUMENT_ID,
                Document.COLUMN_FLAGS,
                Document.COLUMN_DISPLAY_NAME,
                Document.COLUMN_SIZE,
                Document.COLUMN_LAST_MODIFIED,
                Document.COLUMN_MIME_TYPE
        };
        MatrixCursor c = new MatrixCursor(COLUMNS);
        for (int i = 0; i < 3; ++i) {
            MatrixCursor.RowBuilder row = c.newRow();
            row.add(Document.COLUMN_DOCUMENT_ID, Integer.toString(i));
            row.add(Document.COLUMN_MIME_TYPE, Document.MIME_TYPE_DIR);
        }

        if (!onlyDirectory) {
            MatrixCursor.RowBuilder row = c.newRow();
            row.add(Document.COLUMN_DOCUMENT_ID, "4");
            row.add(Document.COLUMN_MIME_TYPE, "image/jpg");
        }

        DirectoryResult r = new DirectoryResult();
        r.cursor = c;
        Model model = new Model(new TestFeatures());
        model.update(r);

        return model;
    }
}
