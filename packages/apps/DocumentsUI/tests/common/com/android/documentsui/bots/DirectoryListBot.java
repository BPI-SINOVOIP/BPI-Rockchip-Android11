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

package com.android.documentsui.bots;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertTrue;
import static junit.framework.Assert.fail;

import android.app.UiAutomation;
import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.SystemClock;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.Configurator;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiScrollable;
import android.support.test.uiautomator.UiSelector;
import android.support.test.uiautomator.Until;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Pattern;

/**
 * A test helper class that provides support for controlling directory list
 * and making assertions against the state of it.
 */
public class DirectoryListBot extends Bots.BaseBot {

    private static final int MAX_LAYOUT_LEVEL = 10;

    private static final BySelector SNACK_DELETE =
            By.text(Pattern.compile("^Deleting [0-9]+ item.+"));

    private final String mDirContainerId;
    private final String mDirListId;
    private final String mItemRootId;
    private final String mPreviewId;
    private final String mIconId;

    private UiAutomation mAutomation;

    public DirectoryListBot(
            UiDevice device, UiAutomation automation, Context context, int timeout) {
        super(device, context, timeout);
        mAutomation = automation;
        mDirContainerId = mTargetPackage + ":id/container_directory";
        mDirListId = mTargetPackage + ":id/dir_list";
        mItemRootId = mTargetPackage + ":id/item_root";
        mPreviewId = mTargetPackage + ":id/preview_icon";
        mIconId = mTargetPackage + ":id/icon";
    }

    public void assertDocumentsCount(int count) throws UiObjectNotFoundException {
        UiObject docsList = findDocumentsList();
        assertEquals(count, docsList.getChildCount());
    }

    public void assertDocumentsPresent(String... labels) throws UiObjectNotFoundException {
        List<String> absent = new ArrayList<>();
        for (String label : labels) {
            if (!findDocument(label).exists()) {
                absent.add(label);
            }
        }
        if (!absent.isEmpty()) {
            fail("Expected documents " + Arrays.asList(labels)
                    + ", but missing " + absent);
        }
    }

    public void assertDocumentsAbsent(String... labels) throws UiObjectNotFoundException {
        List<String> found = new ArrayList<>();
        for (String label : labels) {
            if (findDocument(label).exists()) {
                found.add(label);
            }
        }
        if (!found.isEmpty()) {
            fail("Expected documents not present" + Arrays.asList(labels)
                    + ", but present " + found);
        }
    }

    public void assertDocumentsCountOnList(boolean exists, int count) throws UiObjectNotFoundException {
        UiObject docsList = findDocumentsList();
        assertEquals(exists, docsList.exists());
        if(docsList.exists()) {
            assertEquals(count, docsList.getChildCount());
        }
    }

    public void assertHasMessage(String expected) throws UiObjectNotFoundException {
        UiObject messageTextView = findHeaderMessageTextView();
        String msg = String.valueOf(expected);
        assertEquals(msg, messageTextView.getText());
    }

    public void assertHasMessage(boolean expected) throws UiObjectNotFoundException {
        UiObject messageTextView = findHeaderMessageTextView();
        if (expected) {
            assertTrue(messageTextView.exists());
        } else {
            assertFalse(messageTextView.exists());
        }
    }

    public void assertHasMessageButtonText(String expected) throws UiObjectNotFoundException {
        UiObject button = findHeaderMessageButton();
        String msg = String.valueOf(expected);
        assertEquals(msg.toUpperCase(), button.getText().toUpperCase());
    }

    public void clickMessageButton() throws UiObjectNotFoundException {
        UiObject button = findHeaderMessageButton();
        button.click();
    }

    /**
     * Checks against placeholder text. Placeholder can be Empty page, No results page, or the
     * "Hourglass" page (ie. something-went-wrong page).
     */
    public void assertPlaceholderMessageText(String message) throws UiObjectNotFoundException {
        UiObject messageTextView = findPlaceholderMessageTextView();
        assertTrue(messageTextView.exists());

        String msg = String.valueOf(message);
        assertEquals(msg, messageTextView.getText());

    }

    private UiObject findHeaderMessageTextView() {
        return findObject(
                mDirContainerId,
                mTargetPackage + ":id/message_textview");
    }

    private UiObject findHeaderMessageButton() {
        return findObject(
                mDirContainerId,
                mTargetPackage + ":id/dismiss_button");
    }

    private UiObject findPlaceholderMessageTextView() {
        return findObject(
                mDirContainerId,
                mTargetPackage + ":id/message");
    }

    public void waitForHolderMessage() {
        findPlaceholderMessageTextView().waitForExists(mTimeout);
    }

    public void assertSnackbar(int id) {
        assertNotNull(getSnackbar(mContext.getString(id)));
    }

    public void openDocument(String label) throws UiObjectNotFoundException {
        int toolType = Configurator.getInstance().getToolType();
        Configurator.getInstance().setToolType(MotionEvent.TOOL_TYPE_FINGER);
        UiObject doc = findDocument(label, true);
        doc.click();
        Configurator.getInstance().setToolType(toolType);
    }

    public void selectDocument(String label) throws UiObjectNotFoundException {
        waitForDocument(label);
        UiObject2 selectionHotspot = findSelectionHotspot(label);
        selectionHotspot.click();
    }

    /**
     * @param label The filename of the document
     * @param number Which nth document it is. The number corresponding to "n selected"
     */
    public void selectDocument(String label, int number) throws UiObjectNotFoundException {
        selectDocument(label);

        // wait until selection is fully done to avoid future click being registered as double
        // clicking
        assertSelection(number);
    }

    public boolean isDocumentSelected(String label) throws UiObjectNotFoundException {
        waitForDocument(label);
        UiObject2 selectionHotspot = findSelectionHotspot(label);
        return selectionHotspot.getResourceName()
                .equals(mTargetPackage + ":id/icon_check");
    }

    public UiObject2 findSelectionHotspot(String label) throws UiObjectNotFoundException {
        final BySelector list = By.res(mDirListId);

        BySelector selector = By.hasChild(By.text(label));

        final UiSelector docList = findDocumentsListSelector();
        new UiScrollable(docList).scrollIntoView(new UiSelector().text(label));

        UiObject2 parent = mDevice.findObject(list).findObject(selector);
        for (int i = 1; i <= MAX_LAYOUT_LEVEL; i++) {
            parent = parent.getParent();
            if (mItemRootId.equals(parent.getResourceName())) {
                break;
            }
        }
        return parent.findObject(By.res(mIconId));
    }

    public void copyFilesToClipboard(String...labels) throws UiObjectNotFoundException {
        for (String label: labels) {
            selectDocument(label);
        }
        mDevice.pressKeyCode(KeyEvent.KEYCODE_C, KeyEvent.META_CTRL_ON);
    }

    public void pasteFilesFromClipboard() {
        mDevice.pressKeyCode(KeyEvent.KEYCODE_V, KeyEvent.META_CTRL_ON);
    }

    public UiObject2 getSnackbar(String message) {
        return mDevice.wait(Until.findObject(By.text(message)), mTimeout);
    }

    public void clickSnackbarAction() throws UiObjectNotFoundException {
        UiObject snackbarAction =
                findObject(mTargetPackage + ":id/snackbar_action");
        snackbarAction.click();
    }

    public void waitForDeleteSnackbar() {
        mDevice.wait(Until.findObject(SNACK_DELETE), mTimeout);
    }

    public void waitForDeleteSnackbarGone() {
        // wait a little longer for snackbar to go away, as it disappears after a timeout.
        mDevice.wait(Until.gone(SNACK_DELETE), mTimeout * 2);
    }

    public void waitForDocument(String label) throws UiObjectNotFoundException {
        findDocument(label).waitForExists(mTimeout);
    }

    public UiObject findDocument(String label) throws UiObjectNotFoundException {
        return findDocument(label, false);
    }

    public UiObject findDocument(String label, boolean withScroll)
            throws UiObjectNotFoundException {
        final UiSelector docList = findDocumentsListSelector();

        // Wait for the first list item to appear
        new UiObject(docList.childSelector(new UiSelector())).waitForExists(mTimeout);

        if (withScroll) {
            new UiScrollable(docList).scrollIntoView(new UiSelector().text(label));
        }
        return mDevice.findObject(docList.childSelector(new UiSelector().text(label)));
    }

    public boolean hasDocuments(String... labels) throws UiObjectNotFoundException {
        for (String label : labels) {
            if (!findDocument(label).exists()) {
                return false;
            }
        }
        return true;
    }

    public boolean hasDocumentPreview(String label) {
        final BySelector list = By.res(mDirListId);
        final UiObject2 text = mDevice.findObject(list).findObject(By.text(label));

        UiObject2 parent = text;
        for (int i = 1; i <= MAX_LAYOUT_LEVEL; i++) {
            parent = parent.getParent();
            if (mItemRootId.equals(parent.getResourceName())) {
                break;
            }
        }

        return parent.hasObject(By.res(mPreviewId));
    }

    public void assertFirstDocumentHasFocus() throws UiObjectNotFoundException {
        final UiSelector docList = findDocumentsListSelector();

        // Wait for the first list item to appear
        UiObject doc = new UiObject(docList.childSelector(new UiSelector()));
        doc.waitForExists(mTimeout);

        assertTrue(doc.isFocused());
    }

    public UiObject findDocumentsList() {
        return findObject(
                mDirContainerId,
                mDirListId);
    }

    private UiSelector findDocumentsListSelector() {
        return new UiSelector().resourceId(
                mDirContainerId).childSelector(
                new UiSelector().resourceId(mDirListId));
    }

    public void assertHasFocus() {
        assertHasFocus(mDirListId);
    }

    public void assertSelection(int numSelected) {
        String assertSelectionText = numSelected + " selected";
        UiObject2 selectionText = mDevice.wait(
                Until.findObject(By.text(assertSelectionText)), mTimeout);
        assertTrue(selectionText != null);
    }

    public void assertOrder(String[] dirs, String[] files) throws UiObjectNotFoundException {
        for (int i = 0; i < dirs.length - 1; ++i) {
            assertOrder(dirs[i], dirs[i + 1]);
        }

        if (dirs.length > 0 && files.length > 0) {
            assertOrder(dirs[dirs.length - 1], files[0]);
        }

        for (int i = 0; i < files.length - 1; ++i) {
            assertOrder(files[i], files[i + 1]);
        }
    }

    public void rightClickDocument(String label) throws UiObjectNotFoundException {
        Rect startCoord = findDocument(label, true).getBounds();
        rightClickDocument(new Point(startCoord.centerX(), startCoord.centerY()));
    }

    public void rightClickDocument(Point point) throws UiObjectNotFoundException {
        //TODO: Use Espresso instead of doing the events mock ourselves
        MotionEvent motionDown = getTestMotionEvent(
                MotionEvent.ACTION_DOWN,
                MotionEvent.BUTTON_SECONDARY,
                MotionEvent.TOOL_TYPE_MOUSE,
                InputDevice.SOURCE_MOUSE,
                point.x,
                point.y);
        mAutomation.injectInputEvent(motionDown, true);
        SystemClock.sleep(100);

        MotionEvent motionUp = getTestMotionEvent(
                MotionEvent.ACTION_UP,
                MotionEvent.BUTTON_SECONDARY,
                MotionEvent.TOOL_TYPE_MOUSE,
                InputDevice.SOURCE_MOUSE,
                point.x,
                point.y);

        mAutomation.injectInputEvent(motionUp, true);
    }

    private MotionEvent getTestMotionEvent(
            int action, int buttonState, int toolType, int source, int x, int y) {
        long eventTime = SystemClock.uptimeMillis();

        MotionEvent.PointerProperties[] pp = {new MotionEvent.PointerProperties()};
        pp[0].clear();
        pp[0].id = 0;
        pp[0].toolType = toolType;

        MotionEvent.PointerCoords[] pointerCoords = {new MotionEvent.PointerCoords()};
        pointerCoords[0].clear();
        pointerCoords[0].x = x;
        pointerCoords[0].y = y;
        pointerCoords[0].pressure = 0;
        pointerCoords[0].size = 1;

        return MotionEvent.obtain(
                eventTime,
                eventTime,
                action,
                1,
                pp,
                pointerCoords,
                0,
                buttonState,
                1f,
                1f,
                0,
                0,
                source,
                0);
    }

    private void assertOrder(String first, String second) throws UiObjectNotFoundException {

        final UiObject firstObj = findDocument(first);
        final UiObject secondObj = findDocument(second);

        final int layoutDirection = mContext.getResources().getConfiguration().getLayoutDirection();
        final Rect firstBound = firstObj.getVisibleBounds();
        final Rect secondBound = secondObj.getVisibleBounds();
        if (layoutDirection == View.LAYOUT_DIRECTION_LTR) {
            assertTrue(
                    "\"" + first + "\" is not located above or to the left of \"" + second
                            + "\" in LTR",
                    firstBound.bottom < secondBound.top || firstBound.right < secondBound.left);
        } else {
            assertTrue(
                    "\"" + first + "\" is not located above or to the right of \"" + second +
                            "\" in RTL",
                    firstBound.bottom < secondBound.top || firstBound.left > secondBound.right);
        }
    }
}
