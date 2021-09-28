/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.view.inputmethod.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.os.Bundle;
import android.os.LocaleList;
import android.os.Parcel;
import android.util.Size;
import android.view.inputmethod.InlineSuggestionsRequest;
import android.widget.inline.InlinePresentationSpec;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class InlineSuggestionsRequestTest {

    @Test
    public void testNullInlinePresentationSpecsThrowsException() {
        assertThrows(NullPointerException.class,
                () -> new InlineSuggestionsRequest.Builder(/* presentationSpecs */ null).build());
    }

    @Test
    public void testInvalidPresentationSpecsCountsThrowsException() {
        ArrayList<InlinePresentationSpec> presentationSpecs = new ArrayList<>();
        presentationSpecs.add(new InlinePresentationSpec.Builder(new Size(100, 100),
                new Size(400, 100)).build());
        presentationSpecs.add(new InlinePresentationSpec.Builder(new Size(100, 100),
                new Size(400, 100)).build());

        assertThrows(IllegalStateException.class,
                () -> new InlineSuggestionsRequest.Builder(presentationSpecs)
                        .setMaxSuggestionCount(1).build());
    }

    @Test
    public void testEmptyPresentationSpecsThrowsException() {
        assertThrows(IllegalStateException.class,
                () -> new InlineSuggestionsRequest.Builder(new ArrayList<>())
                        .setMaxSuggestionCount(1).build());
    }

    @Test
    public void testZeroMaxSuggestionCountThrowsException() {
        ArrayList<InlinePresentationSpec> presentationSpecs = new ArrayList<>();
        presentationSpecs.add(new InlinePresentationSpec.Builder(new Size(100, 100),
                new Size(400, 100)).build());
        assertThrows(IllegalStateException.class,
                () -> new InlineSuggestionsRequest.Builder(presentationSpecs)
                        .setMaxSuggestionCount(0).build());
    }

    @Test
    public void testInlineSuggestionsRequestValuesWithDefaults() {
        final int suggestionCount = 3;
        ArrayList<InlinePresentationSpec> presentationSpecs = new ArrayList<>();
        InlineSuggestionsRequest request =
                new InlineSuggestionsRequest.Builder(presentationSpecs)
                        .addInlinePresentationSpecs(
                                new InlinePresentationSpec.Builder(new Size(100, 100),
                                        new Size(400, 100)).build())
                        .setMaxSuggestionCount(suggestionCount).build();

        assertThat(request.getMaxSuggestionCount()).isEqualTo(suggestionCount);
        assertThat(request.getInlinePresentationSpecs()).isNotNull();
        assertThat(request.getInlinePresentationSpecs().size()).isEqualTo(1);
        assertThat(request.getInlinePresentationSpecs().get(0).getStyle()).isEqualTo(Bundle.EMPTY);
        assertThat(request.getExtras()).isEqualTo(Bundle.EMPTY);
        assertThat(request.getSupportedLocales()).isEqualTo(LocaleList.getDefault());

        // Tests the parceling/deparceling
        Parcel p = Parcel.obtain();
        request.writeToParcel(p, 0);
        p.setDataPosition(0);

        InlineSuggestionsRequest targetRequest =
                InlineSuggestionsRequest.CREATOR.createFromParcel(p);
        p.recycle();

        assertThat(targetRequest).isEqualTo(request);
    }

    @Test
    public void testInlineSuggestionsRequestValues() {
        final int suggestionCount = 3;
        final LocaleList localeList = LocaleList.forLanguageTags("fa-IR");
        final Bundle extra = new Bundle();
        extra.putString("key", "value");
        final Bundle style = new Bundle();
        style.putString("style", "value");
        final ArrayList<InlinePresentationSpec> presentationSpecs = new ArrayList<>();
        presentationSpecs.add(new InlinePresentationSpec.Builder(new Size(100, 100),
                new Size(400, 100)).setStyle(style).build());
        final InlineSuggestionsRequest request =
                new InlineSuggestionsRequest.Builder(new ArrayList<InlinePresentationSpec>())
                        .setInlinePresentationSpecs(presentationSpecs)
                        .addInlinePresentationSpecs(
                                new InlinePresentationSpec.Builder(new Size(100, 100),
                                        new Size(400, 100)).setStyle(style).build())
                        .setSupportedLocales(LocaleList.forLanguageTags("fa-IR"))
                        .setExtras(/* value */ extra)
                        .setMaxSuggestionCount(suggestionCount).build();

        assertThat(request.getMaxSuggestionCount()).isEqualTo(suggestionCount);
        assertThat(request.getInlinePresentationSpecs()).isNotNull();
        assertThat(request.getInlinePresentationSpecs().size()).isEqualTo(2);
        assertThat(request.getInlinePresentationSpecs().get(0).getStyle()).isEqualTo(style);
        assertThat(request.getExtras()).isEqualTo(extra);
        assertThat(request.getSupportedLocales()).isEqualTo(localeList);

        // Tests the parceling/deparceling
        Parcel p = Parcel.obtain();
        request.writeToParcel(p, 0);
        p.setDataPosition(0);

        InlineSuggestionsRequest targetRequest =
                InlineSuggestionsRequest.CREATOR.createFromParcel(p);
        p.recycle();

        assertThat(targetRequest).isEqualTo(request);
    }
}
