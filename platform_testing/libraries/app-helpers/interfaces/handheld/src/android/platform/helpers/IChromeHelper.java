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

package android.platform.helpers;

import android.support.test.uiautomator.Direction;

public interface IChromeHelper extends IAppHelper {
    public enum MenuItem {
        BOOKMARKS("Bookmarks"),
        NEW_TAB("New tab"),
        DOWNLOADS("Downloads"),
        HISTORY("History"),
        SETTINGS("Settings");

        private String mDisplayName;

        MenuItem(String displayName) {
            mDisplayName = displayName;
        }

        @Override
        public String toString() {
            return mDisplayName;
        }
    }

    public enum ClearRange {
        PAST_HOUR("past hour"),
        PAST_DAY("past day"),
        PAST_WEEK("past week"),
        LAST_4_WEEKS("last 4 weeks"),
        BEGINNING_OF_TIME("beginning of time");

        private String mDisplayName;

        ClearRange(String displayName) {
            mDisplayName = displayName;
        }

        @Override
        public String toString() {
            return mDisplayName;
        }
    }

    /**
     * Setup expectations: Chrome is open and on a standard page, i.e. a tab is open.
     *
     * <p>This method will open the URL supplied and block until the page is open.
     */
    public abstract void openUrl(String url);

    /**
     * Setup expectations: Chrome is open on a page.
     *
     * <p>This method will scroll the page as directed and block until idle.
     */
    public abstract void flingPage(Direction dir);

    /**
     * Setup expectations: Chrome is open on a page.
     *
     * <p>This method will open the overload menu, indicated by three dots and block until open.
     */
    public abstract void openMenu();

    /**
     * Setup expectations: Chrome is open on a page.
     *
     * <p>This method will open provided item in the menu.
     */
    public abstract void openMenuItem(IChromeHelper.MenuItem menuItem);

    /**
     * Setup expectations: Chrome is open on a page.
     *
     * <p>This method will open provided item in the menu.
     *
     * @param menuItem The name of menu item.
     * @param waitForPageLoad Wait for the page to load completely or not.
     */
    public default void openMenuItem(IChromeHelper.MenuItem menuItem, boolean waitForPageLoad) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Chrome is open on a page and the tabs are treated as apps.
     *
     * <p>This method will change the settings to treat tabs inside of Chrome and block until Chrome
     * is open on the original tab.
     */
    public abstract void mergeTabs();

    /**
     * Setup expectations: Chrome is open on a page and the tabs are merged.
     *
     * <p>This method will change the settings to treat tabs outside of Chrome and block until
     * Chrome is open on the original tab.
     */
    public abstract void unmergeTabs();

    /**
     * Setup expectations: Chrome is open on a page.
     *
     * <p>This method will reload the page by clicking the refresh button, and block until the page
     * is reopened.
     */
    public abstract void reloadPage();

    /**
     * Setup expectations: Chrome is open on a page.
     *
     * <p>This method will stop loading page then reload the page by clicking the refresh button,
     * and block until the page is reopened.
     */
    public default void stopAndReloadPage() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Chrome is open on a page.
     *
     * <p>This method will stop loading page then reload the page by clicking the refresh button,
     *
     * @param waitForPageLoad Wait for the page to load completely or not.
     */
    public default void stopAndReloadPage(boolean waitForPageLoad) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Chrome is open on a page.
     *
     * <p>This method is getter for contentDescription of Tab elements.
     */
    public abstract String getTabDescription();

    /**
     * Setup expectations: Chrome is open on a History page.
     *
     * <p>This method clears browser history for provided period of time.
     */
    public abstract void clearBrowsingData(IChromeHelper.ClearRange range);

    /**
     * Setup expectations: Chrome is open on a Downloads page.
     *
     * <p>This method checks header is displayed on Downloads page.
     */
    public abstract void checkIfDownloadsOpened();

    /**
     * Setup expectations: Chrome is open on a Settings page.
     *
     * <p>This method clicks on Privacy setting on Settings page.
     */
    public abstract void openPrivacySettings();

    /**
     * Setup expectations: Chrome is open on a page.
     *
     * <p>This method will add the current page to Bookmarks
     */
    public default boolean addCurrentPageToBookmark() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Chrome is open on a Bookmarks page.
     *
     * <p>This method selects a bookmark from the Bookmarks page.
     *
     * @param index The Index of bookmark to select.
     */
    public default void openBookmark(int index) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Chrome is open on a Bookmarks page.
     *
     * <p>This method selects a bookmark from the Bookmarks page.
     *
     * @param bookmarkName The string of the target bookmark to select.
     * @param waitForPageLoad Wait for the page to load completely or not.
     */
    public default void openBookmark(String bookmarkName, boolean waitForPageLoad) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Chrome is open on a page.
     *
     * <p>Selects the link with specific text.
     *
     * @param linkText The text of the link to select.
     */
    public default void selectLink(String linkText) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Chrome is open on a page.
     *
     * <p>Performs a scroll gesture on the page.
     *
     * @param dir The direction on the page to scroll.
     * @param percent The distance to scroll as a percentage of the page's visible size.
     */
    public default void scrollPage(Direction dir, float percent) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }
}
