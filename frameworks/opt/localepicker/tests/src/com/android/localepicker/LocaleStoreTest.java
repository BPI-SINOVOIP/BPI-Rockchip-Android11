/*
 * Copyright 2018 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.localepicker;

import static com.google.common.truth.Truth.assertThat;

import static org.robolectric.RuntimeEnvironment.application;
import static org.robolectric.Shadows.shadowOf;

import android.icu.util.ULocale;
import android.telephony.TelephonyManager;

import com.android.internal.app.LocalePicker;
import com.android.localepicker.LocaleStore.LocaleInfo;
import com.android.localepicker.LocaleStoreTest.ShadowICU;
import com.android.localepicker.LocaleStoreTest.ShadowLocalePicker;

import libcore.icu.ICU;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import java.util.Locale;

@RunWith(RobolectricTestRunner.class)
@Config(
        shadows = {
                ShadowLocalePicker.class,
                ShadowICU.class,
        })
public class LocaleStoreTest {

    @Before
    public void setupTelephonyManager() {
        TelephonyManager telephonyManager = application.getSystemService(TelephonyManager.class);
        shadowOf(telephonyManager).setNetworkCountryIso("us");
        shadowOf(telephonyManager).setSimCountryIso("us");
    }

    @Before
    public void fillCache() {
        LocaleStore.fillCache(application);
    }

    @Test
    public void getLevel() {
        LocaleInfo localeInfo = LocaleStore.getLocaleInfo(Locale.forLanguageTag("zh-Hant-HK"));
        assertThat(localeInfo.getParent().toLanguageTag()).isEqualTo("zh-Hant");
        assertThat(localeInfo.isTranslated()).named("is translated").isTrue();
    }

    @Implements(LocalePicker.class)
    public static class ShadowLocalePicker {

        @Implementation
        public static String[] getSystemAssetLocales() {
            return new String[] { "en-US", "zh-HK", "ja-JP", "zh-TW" };
        }
    }

    @Implements(ICU.class)
    public static class ShadowICU {

        @Implementation
        public static Locale addLikelySubtags(Locale locale) {
            ULocale uLocale = ULocale.addLikelySubtags(ULocale.forLocale(locale));
            return uLocale.toLocale();
        }
    }
}