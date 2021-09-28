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

public interface IYTMusicHelper extends IAppHelper {

    /**
     * Setup expectations: YT Music is open and the tab bar is visible.
     *
     * <p>This method will press "Library" to the Library tab. This method blocks until the process
     * is complete.
     */
    public void goToLibrary();

    /**
     * Setup expectations: YT Music is open and the Library tab is visible.
     *
     * <p>This method will select the albums. This method blocks until the process is complete.
     */
    public void openAlbums();

    /**
     * Setup expectations: YT Music is open and the Library tab is visible.
     *
     * <p>This method will select the songs. This method blocks until the process is complete.
     */
    public default void openSongs() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: YT Music is open and the Albums page is visible.
     *
     * <p>This method will browse the device files. The method will block until the process is
     * complete.
     */
    public void browseDeviceFiles();

    /**
     * Setup expectations: YT Music is open and the song list is visible.
     *
     * <p>This method will select the song. The method will block until the song is playing.
     */
    public default void selectSong(String song) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: YT Music is open and the album list is visible.
     *
     * <p>This method will select the album and select the song. The method will block until the
     * song is playing.
     */
    public void selectSong(String album, String song);

    /**
     * Setup expectations: YT Music is open with a song paused.
     *
     * <p>This method will play the song and block until the song is playing.
     */
    public void playSong();

    /**
     * Setup expectations: YT Music is open with a song playing.
     *
     * <p>This method will pause the song and block until the song is paused.
     */
    public void pauseSong();
}
