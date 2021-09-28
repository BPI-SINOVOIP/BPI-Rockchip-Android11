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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

@RunWith(RobolectricTestRunner.class)
public class SuggestedLocaleAdapterTest {

    private HashSet<LocaleInfo> mLocaleOptions;
    private HashSet<LocaleInfo> mEnglishCountryOptions;

    @Before
    public void setUp() {
        mLocaleOptions = new HashSet<>();
        mLocaleOptions.add(LocaleStore.getLocaleInfo(Locale.US));
        mLocaleOptions.add(LocaleStore.getLocaleInfo(Locale.GERMANY));
        mLocaleOptions.add(LocaleStore.getLocaleInfo(Locale.JAPAN));
        LocaleInfo korea = LocaleStore.getLocaleInfo(Locale.KOREA);
        korea.setTranslated(true);
        korea.mSuggestionFlags = LocaleInfo.SUGGESTION_TYPE_SIM;
        mLocaleOptions.add(korea);

        mEnglishCountryOptions = new HashSet<>();
        mEnglishCountryOptions.add(LocaleStore.getLocaleInfo(Locale.US));
        mEnglishCountryOptions.add(LocaleStore.getLocaleInfo(Locale.UK));
        mEnglishCountryOptions.add(LocaleStore.getLocaleInfo(Locale.CANADA));
        mEnglishCountryOptions.add(LocaleStore.getLocaleInfo(Locale.forLanguageTag("en-IN")));
        mEnglishCountryOptions.add(LocaleStore.getLocaleInfo(Locale.forLanguageTag("en-HK")));
        mEnglishCountryOptions.add(LocaleStore.getLocaleInfo(Locale.forLanguageTag("en-SG")));
        LocaleInfo australianEnglish = LocaleStore.getLocaleInfo(Locale.forLanguageTag("en-AU"));
        australianEnglish.setTranslated(true);
        australianEnglish.mSuggestionFlags = LocaleInfo.SUGGESTION_TYPE_SIM;
        mEnglishCountryOptions.add(australianEnglish);
    }

    @Test
    public void suggestedLocaleAdapter_notCountryMode_shouldDisplayLocalesSorted() {
        SuggestedLocaleAdapter suggestedLocaleAdapter =
                new SuggestedLocaleAdapter(mLocaleOptions, /* countryMode */ false);
        suggestedLocaleAdapter.sort(new LocaleInfoComparator(Locale.US, /* countryMode */ false));

        assertThat(getItemTypeList(suggestedLocaleAdapter))
                .containsExactly(
                        SuggestedLocaleAdapter.TYPE_HEADER_SUGGESTED,
                        SuggestedLocaleAdapter.TYPE_LOCALE,
                        SuggestedLocaleAdapter.TYPE_HEADER_ALL_OTHERS,
                        SuggestedLocaleAdapter.TYPE_LOCALE,
                        SuggestedLocaleAdapter.TYPE_LOCALE,
                        SuggestedLocaleAdapter.TYPE_LOCALE)
                .inOrder();
        assertThat(getLocaleTagList(suggestedLocaleAdapter))
                .containsExactly(
                        null,
                        "ko-KR",
                        null,
                        "de-DE",
                        "en-US",
                        "ja-JP")
                .inOrder();
    }

    @Test
    public void suggestedLocaleAdapter_countryMode_shouldDisplayCountriesSorted() {
        SuggestedLocaleAdapter suggestedLocaleAdapter =
                new SuggestedLocaleAdapter(mEnglishCountryOptions, /* countryMode */ true);
        suggestedLocaleAdapter.sort(new LocaleInfoComparator(Locale.US, /* countryMode */ true));

        assertThat(getItemTypeList(suggestedLocaleAdapter))
                .containsExactly(
                        SuggestedLocaleAdapter.TYPE_HEADER_SUGGESTED,
                        SuggestedLocaleAdapter.TYPE_LOCALE,
                        SuggestedLocaleAdapter.TYPE_HEADER_ALL_OTHERS,
                        SuggestedLocaleAdapter.TYPE_LOCALE,
                        SuggestedLocaleAdapter.TYPE_LOCALE,
                        SuggestedLocaleAdapter.TYPE_LOCALE,
                        SuggestedLocaleAdapter.TYPE_LOCALE,
                        SuggestedLocaleAdapter.TYPE_LOCALE,
                        SuggestedLocaleAdapter.TYPE_LOCALE)
                .inOrder();
        assertThat(getLocaleTagList(suggestedLocaleAdapter))
                .containsExactly(
                        null,
                        "en-AU",
                        null,
                        "en-CA",
                        "en-HK",
                        "en-IN",
                        "en-SG",
                        "en-GB",
                        "en-US")
                .inOrder();
    }

    private static List<Integer> getItemTypeList(final SuggestedLocaleAdapter adapter) {
        return IntStream.range(0, adapter.getCount())
                .mapToObj(adapter::getItemViewType)
                .collect(Collectors.toList());
    }

    private static List<String> getLocaleTagList(final SuggestedLocaleAdapter adapter) {
        return IntStream.range(0, adapter.getCount())
                .mapToObj(position -> {
                    LocaleInfo localeInfo = adapter.getItem(position);
                    if (localeInfo == null) {
                        return null;
                    }
                    return localeInfo.getLocale().toLanguageTag();
                })
                .collect(Collectors.toList());
    }
}
