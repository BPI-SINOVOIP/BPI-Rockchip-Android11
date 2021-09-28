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

package com.android.tv.tuner.tvinput.factory;

import android.media.tv.TvInputService.Session;
import android.net.Uri;

import com.android.tv.tuner.tvinput.datamanager.ChannelDataManager;

/** {@link android.media.tv.TvInputService.Session} factory */
public interface TunerSessionFactory {

    /** Called when a session is released */
    interface SessionReleasedCallback {

        /**
         * Called when the given session is released.
         *
         * @param session The session that has been released.
         */
        void onReleased(Session session);
    }

    /** Called when recording URI is required for playback */
    interface SessionRecordingCallback {

        /**
         * Called when recording URI is required for playback.
         *
         * @param channelUri for which recording URI is requested.
         */
        Uri getRecordingUri(Uri channelUri);
    }

    Session create(
            ChannelDataManager channelDataManager,
            SessionReleasedCallback releasedCallback,
            SessionRecordingCallback recordingCallback);
}
