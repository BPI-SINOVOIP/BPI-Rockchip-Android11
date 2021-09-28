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

package android.autofillservice.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.app.slice.Slice;
import android.app.slice.SliceSpec;
import android.net.Uri;
import android.os.Parcel;
import android.service.autofill.InlinePresentation;
import android.util.Size;
import android.widget.inline.InlinePresentationSpec;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class InlinePresentationTest {

    @Test
    public void testNullInlinePresentationSpecsThrowsException() {
        assertThrows(NullPointerException.class,
                () -> createInlinePresentation(/* createSlice */true, /* createSpec */  false));
    }

    @Test
    public void testNullSliceThrowsException() {
        assertThrows(NullPointerException.class,
                () -> createInlinePresentation(/* createSlice */false, /* createSpec */  true));
    }

    @Test
    public void testInlinePresentationValues() {
        InlinePresentation presentation =
                createInlinePresentation(/* createSlice */true, /* createSpec */  true);

        assertThat(presentation.isPinned()).isFalse();
        assertThat(presentation.getInlinePresentationSpec()).isNotNull();
        assertThat(presentation.getSlice()).isNotNull();
        assertThat(presentation.getSlice().getItems().size()).isEqualTo(0);
    }

    @Test
    public void testtInlinePresentationParcelizeDeparcelize() {
        InlinePresentation presentation =
                createInlinePresentation(/* createSlice */true, /* createSpec */  true);

        Parcel p = Parcel.obtain();
        presentation.writeToParcel(p, 0);
        p.setDataPosition(0);

        InlinePresentation targetPresentation = InlinePresentation.CREATOR.createFromParcel(p);
        p.recycle();

        assertThat(targetPresentation.isPinned()).isEqualTo(presentation.isPinned());
        assertThat(targetPresentation.getInlinePresentationSpec()).isEqualTo(
                presentation.getInlinePresentationSpec());
        assertThat(targetPresentation.getSlice().getUri()).isEqualTo(
                presentation.getSlice().getUri());
        assertThat(targetPresentation.getSlice().getSpec()).isEqualTo(
                presentation.getSlice().getSpec());
    }

    private InlinePresentation createInlinePresentation(boolean createSlice, boolean createSpec) {
        Slice slice = createSlice ? new Slice.Builder(Uri.parse("testuri"),
                new SliceSpec("type", 1)).build() : null;
        InlinePresentationSpec spec = createSpec ? new InlinePresentationSpec.Builder(
                new Size(100, 100), new Size(400, 100)).build() : null;
        return new InlinePresentation(slice, spec, /* pined */ false);
    }
}
