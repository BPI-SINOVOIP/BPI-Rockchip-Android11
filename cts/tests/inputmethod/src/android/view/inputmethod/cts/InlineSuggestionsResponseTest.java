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

import android.os.Parcel;
import android.view.inputmethod.InlineSuggestion;
import android.view.inputmethod.InlineSuggestionsResponse;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class InlineSuggestionsResponseTest {

    @Test
    public void testNullInlinePresentationSpecThrowsException() {
        assertThrows(NullPointerException.class,
                () -> InlineSuggestionsResponse.newInlineSuggestionsResponse(/* inlineSuggestions */
                        null));
    }

    @Test
    public void testInlineSuggestionsResponseValues() {
        ArrayList<InlineSuggestion> inlineSuggestions = new ArrayList<>();
        InlineSuggestionsResponse response =
                InlineSuggestionsResponse.newInlineSuggestionsResponse(inlineSuggestions);

        assertThat(response.getInlineSuggestions()).isNotNull();
        assertThat(response.getInlineSuggestions().size()).isEqualTo(0);
    }

    @Test
    public void testInlineSuggestionsResponseParcelizeDeparcelize() {
        ArrayList<InlineSuggestion> inlineSuggestions = new ArrayList<>();
        InlineSuggestionsResponse response =
                InlineSuggestionsResponse.newInlineSuggestionsResponse(inlineSuggestions);
        Parcel p = Parcel.obtain();
        response.writeToParcel(p, 0);
        p.setDataPosition(0);

        InlineSuggestionsResponse targetResponse =
                InlineSuggestionsResponse.CREATOR.createFromParcel(p);
        p.recycle();

        assertThat(targetResponse).isEqualTo(response);
    }
}
