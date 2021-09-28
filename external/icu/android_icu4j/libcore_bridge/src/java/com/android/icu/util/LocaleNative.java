/*
 * Copyright (C) 2008 The Android Open Source Project
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

package com.android.icu.util;

import java.util.Locale;
import libcore.api.IntraCoreApi;

/**
 * Used by java.util.Locale to access locale data in ICU4C.
 * java.util.Locale avoids using android.icu.util.ULocale to prevent circular runtime dependency.
 */
public final class LocaleNative {

    @IntraCoreApi
    public static String getDisplayCountry(Locale targetLocale, Locale locale) {
        return getDisplayCountryNative(targetLocale.toLanguageTag(), locale.toLanguageTag());
    }

    private static native String getDisplayCountryNative(String targetLanguageTag,
        String languageTag);

    @IntraCoreApi
    public static String getDisplayLanguage(Locale targetLocale, Locale locale) {
        return getDisplayLanguageNative(targetLocale.toLanguageTag(), locale.toLanguageTag());
    }

    private static native String getDisplayLanguageNative(String targetLanguageTag,
        String languageTag);

    @IntraCoreApi
    public static String getDisplayVariant(Locale targetLocale, Locale locale) {
        return getDisplayVariantNative(targetLocale.toLanguageTag(), locale.toLanguageTag());
    }

    private static native String getDisplayVariantNative(String targetLanguageTag,
        String languageTag);

    @IntraCoreApi
    public static String getDisplayScript(Locale targetLocale, Locale locale) {
        return getDisplayScriptNative(targetLocale.toLanguageTag(), locale.toLanguageTag());
    }

    private static native String getDisplayScriptNative(String targetLanguageTag,
        String languageTag);
}
