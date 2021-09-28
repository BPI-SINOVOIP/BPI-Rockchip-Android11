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

package com.android.tv.util;

import android.content.Context;
import android.os.Build;
import android.os.LocaleList;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.Size;
import android.view.accessibility.CaptioningManager;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class CaptionSettings {
    public static final int OPTION_SYSTEM = 0;
    public static final int OPTION_OFF = 1;
    public static final int OPTION_ON = 2;

    private final CaptioningManager mCaptioningManager;
    private int mOption = OPTION_SYSTEM;
    private String mLanguage;
    private String mTrackId;

    public CaptionSettings(Context context) {
        mCaptioningManager =
                (CaptioningManager) context.getSystemService(Context.CAPTIONING_SERVICE);
    }

    /** @deprecated use {@link #getSystemPreferenceLanguageList} instead. */
    @Deprecated
    public final String getSystemLanguage() {
        Locale l = mCaptioningManager.getLocale();
        if (l != null) {
            return l.getLanguage();
        }
        return null;
    }

    @NonNull
    @Size(min=1)
    public final List<String> getSystemPreferenceLanguageList() {
        List<String> languageList = new ArrayList<>();
        Locale l = mCaptioningManager.getLocale();
        if (l != null) {
            languageList.add(l.getLanguage());
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            LocaleList locales = LocaleList.getDefault();
            for (int i = 0; i < locales.size() ; i++) {
                languageList.add(locales.get(i).getLanguage());
            }
        } else {
            languageList.add(Locale.getDefault().getLanguage());
        }
        return languageList;
    }

    /** Returns the language of closed captions based on options. */
    @Nullable
    public final String getLanguage() {
        switch (mOption) {
            case OPTION_SYSTEM:
                return getSystemPreferenceLanguageList().get(0);
            case OPTION_OFF:
                return null;
            case OPTION_ON:
                return mLanguage;
        }
        return null;
    }

    public final boolean isSystemSettingEnabled() {
        return mCaptioningManager.isEnabled();
    }

    public final boolean isEnabled() {
        switch (mOption) {
            case OPTION_SYSTEM:
                return isSystemSettingEnabled();
            case OPTION_OFF:
                return false;
            case OPTION_ON:
                return true;
        }
        return false;
    }

    public int getEnableOption() {
        return mOption;
    }

    public void setEnableOption(int option) {
        mOption = option;
    }

    public void setLanguage(String language) {
        mLanguage = language;
    }

    /** Returns the track ID to be used as an alternative key. */
    public String getTrackId() {
        return mTrackId;
    }

    /** Sets the track ID to be used as an alternative key. */
    public void setTrackId(String trackId) {
        mTrackId = trackId;
    }
}
