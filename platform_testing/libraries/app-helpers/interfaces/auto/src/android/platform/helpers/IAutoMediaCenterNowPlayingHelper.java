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

public interface IAutoMediaCenterNowPlayingHelper extends IAppHelper {

    /**
     * Setup expectations: Now Playing is open.
     *
     * <p>This method is used to play media.
     */
    void playMedia();

    /**
     * Setup expectations: Now Playing is open.
     *
     * <p>This method is used to pause media.
     */
    void pauseMedia();

    /**
     * Setup expectations: Now Playing is open.
     *
     * <p>This method is used to select next track.
     */
    void clickNextTrack();

    /**
     * Setup expectations: Now Playing is open.
     *
     * <p>This method is used to select previous track.
     */
    void clickPreviousTrack();

    /**
     * Setup expectations: Now Playing is open.
     *
     * @return to get current playing track name from home screen.
     */
    String getTrackName();

    /**
     * Setup expectations: Now Playing is open.
     *
     * @return to get current playing track's artist name from screen.
     */
    String getArtistName();

    /**
     * Setup expectations: Now Playing is open.
     *
     * <p>This method is used to minimize now playing.
     */
    void minimizeNowPlaying();
}
