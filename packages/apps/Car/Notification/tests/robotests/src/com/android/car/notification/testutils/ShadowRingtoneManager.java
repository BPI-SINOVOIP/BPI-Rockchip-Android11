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

package com.android.car.notification.testutils;

import android.content.Context;
import android.media.Ringtone;
import android.media.RingtoneManager;
import android.net.Uri;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

@Implements(RingtoneManager.class)
public class ShadowRingtoneManager {
    private static Ringtone sRingtone;
    private static Uri sRingtoneUri;

    /** Sets the sRingtone to return when getRingtone is called. */
    public static void setRingtone(Ringtone ringtone) {
        ShadowRingtoneManager.sRingtone = ringtone;
        sRingtoneUri = null;
    }

    /** Returns the sRingtone URI provided to the getRingtone method. */
    public static Uri getRingtoneUri() {
        return sRingtoneUri;
    }

    /** Returns sRingtone and stores the provided sRingtone URI for later retrieval. */
    @Implementation
    public static Ringtone getRingtone(Context context, Uri ringtoneUri) {
        ShadowRingtoneManager.sRingtoneUri = ringtoneUri;
        return sRingtone;
    }
}