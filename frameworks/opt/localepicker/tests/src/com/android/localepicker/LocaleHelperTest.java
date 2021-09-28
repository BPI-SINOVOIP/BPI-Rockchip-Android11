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

import com.android.localepicker.LocaleHelper.LocaleInfoComparator;
import com.android.localepicker.LocaleStore.LocaleInfo;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;

@RunWith(RobolectricTestRunner.class)
public class LocaleHelperTest {

    @Test
    public void toSentenceCase_shouldUpperCaseFirstLetter() {
        String sentenceCase = LocaleHelper.toSentenceCase("hello Google", Locale.US);
        assertThat(sentenceCase).isEqualTo("Hello Google");
    }

    @Test
    public void normalizeForSearch_sameStrings_shouldBeEqualAfterNormalized() {
        String lowerCase = LocaleHelper.normalizeForSearch("english", Locale.US);
        String upperCase = LocaleHelper.normalizeForSearch("ENGLISH", Locale.US);
        assertThat(lowerCase).isEqualTo(upperCase);
    }

    @Test
    public void getDisplayName_shouldReturnLocaleDisplayName() {
        String displayName =
                LocaleHelper.getDisplayName(Locale.US, Locale.US, /* sentenceCase */ true);
        assertThat(displayName).isEqualTo("English (United States)");
    }

    @Test
    public void getDisplayName_withDifferentLocale_shouldReturnLocalizedDisplayName() {
        String displayName =
                LocaleHelper.getDisplayName(
                        Locale.CANADA_FRENCH,
                        Locale.US,
                        /* sentenceCase */ true);
        assertThat(displayName).isEqualTo("French (Canada)");
    }

    @Test
    public void getDisplayCountry_shouldReturnLocalizedCountryName() {
        String displayCountry = LocaleHelper.getDisplayCountry(Locale.GERMANY, Locale.GERMANY);
        assertThat(displayCountry).isEqualTo("Deutschland");
    }

    @Test
    public void localeInfoComparator_shouldSortLocales() {
        LocaleInfo germany = LocaleStore.getLocaleInfo(Locale.GERMANY);
        LocaleInfo unitedStates = LocaleStore.getLocaleInfo(Locale.US);
        LocaleInfo japan = LocaleStore.getLocaleInfo(Locale.JAPAN);

        ArrayList<LocaleInfo> list =
                new ArrayList<>(Arrays.asList(germany, unitedStates, japan));
        list.sort(new LocaleInfoComparator(Locale.US, /* countryMode */ false));
        assertThat(list).containsExactly(germany, unitedStates, japan).inOrder();
    }
}
