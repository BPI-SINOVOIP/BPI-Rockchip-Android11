/*
 * Copyright (C) 2019 The Android Open Source Project
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

public interface IAutoMediaCenterHelper extends IAppHelper {

    /** enum for screen state */
    enum ScreenState {
        HOME,
        NOW_PLAYING,
    }

    /** enum for account type state */
    enum AccountTypeState {
        NONE,
        FREE,
        PAID,
    }

    /**
     * Setup expectations: Media test app is open.
     *
     * <p>This method is used to set account type.
     */
    void setAccountType(String accountType);

    /**
     * Setup expectations: Media test app is open.
     *
     * <p>This method is used to set root node type.
     */
    void setRootNodeType(String rootNodeType);

    /**
     * Setup expectations: Media test app is open.
     *
     * <p>This method is used to set root reply delay.
     */
    void setRootReplyDelay(String rootReplyDelay);

    /**
     * Setup expectations: media test app is open.
     *
     * <p>This method is used to open settings.
     */
    void openSettings();

    /**
     * Setup expectations: media test app is open.
     *
     * <p>This method is used to close settings.
     */
    void closeSettings();

    /**
     * Setup expectations: media test app is open.
     *
     * @param - tabName used to select tab name.
     */
    void selectTab(String tabName);

    /**
     * Setup expectations: media test app is open.
     *
     * @param - title used search media track,artist.
     */
    void search(String title);

    /**
     * This method is used to check if media is currently playing Returns true if media is playing
     * else returns false
     */
    boolean isPlaying();

    /**
     * Setup expectations: media test app is open.
     *
     * clicks on a particular album
     */
    void clickAlbum();

    /**
     * Setup expectations: media test app is open.
     *
     * clicks to go back to previous screen
     */
    void clickBackButton();

    /**
     * Setup expectations: media test app is open.
     *
     * <p>This method is used to open Folder Menu with menuOptions and scroll into view the track.
     * Example - openMenu->Folder->Mediafilename->trackName
     * openMenuWith(Folder,mediafilename,trackName);
     *
     * @param - menuOptions used to pass multiple level of menu options in one go.
     */
    void selectMediaTrack(String... menuOptions);

    /**
     * Setup expectations: Launch Media App.
     *
     * @param - String packagename - Android media package to be opened.
     */
    void openMediaApp(String packagename);
}
