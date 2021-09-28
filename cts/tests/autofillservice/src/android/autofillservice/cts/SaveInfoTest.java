/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static android.service.autofill.SaveInfo.FLAG_DELAY_SAVE;
import static android.service.autofill.SaveInfo.FLAG_DONT_SAVE_ON_FINISH;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_GENERIC;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.testng.Assert.assertThrows;

import android.platform.test.annotations.AppModeFull;
import android.service.autofill.InternalSanitizer;
import android.service.autofill.Sanitizer;
import android.service.autofill.SaveInfo;
import android.view.autofill.AutofillId;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@AppModeFull(reason = "Unit test")
public class SaveInfoTest {

    private final AutofillId mId = new AutofillId(42);
    private final AutofillId[] mIdArray = { mId };
    private final InternalSanitizer mSanitizer = mock(InternalSanitizer.class);

    @Test
    public void testRequiredIdsBuilder_null() {
        assertThrows(IllegalArgumentException.class,
                () -> new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC, null));
    }

    @Test
    public void testRequiredIdsBuilder_empty() {
        assertThrows(IllegalArgumentException.class,
                () -> new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC, new AutofillId[] {}));
    }

    @Test
    public void testRequiredIdsBuilder_nullEntry() {
        assertThrows(IllegalArgumentException.class,
                () -> new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC,
                        new AutofillId[] { null }));
    }

    @Test
    public void testBuild_noOptionalIds() {
        final SaveInfo.Builder builder = new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC);
        assertThrows(IllegalStateException.class, ()-> builder.build());
    }

    @Test
    public void testSetOptionalIds_null() {
        final SaveInfo.Builder builder = new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC,
                mIdArray);
        assertThrows(IllegalArgumentException.class, ()-> builder.setOptionalIds(null));
    }

    @Test
    public void testSetOptional_empty() {
        final SaveInfo.Builder builder = new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC,
                mIdArray);
        assertThrows(IllegalArgumentException.class,
                () -> builder.setOptionalIds(new AutofillId[] {}));
    }

    @Test
    public void testSetOptional_nullEntry() {
        final SaveInfo.Builder builder = new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC,
                mIdArray);
        assertThrows(IllegalArgumentException.class,
                () -> builder.setOptionalIds(new AutofillId[] { null }));
    }

    @Test
    public void testAddSanitizer_illegalArgs() {
        final SaveInfo.Builder builder = new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC,
                mIdArray);
        // Null sanitizer
        assertThrows(IllegalArgumentException.class,
                () -> builder.addSanitizer(null, mId));
        // Invalid sanitizer class
        assertThrows(IllegalArgumentException.class,
                () -> builder.addSanitizer(mock(Sanitizer.class), mId));
        // Null ids
        assertThrows(IllegalArgumentException.class,
                () -> builder.addSanitizer(mSanitizer, (AutofillId[]) null));
        // Empty ids
        assertThrows(IllegalArgumentException.class,
                () -> builder.addSanitizer(mSanitizer, new AutofillId[] {}));
        // Repeated ids
        assertThrows(IllegalArgumentException.class,
                () -> builder.addSanitizer(mSanitizer, new AutofillId[] {mId, mId}));
    }

    @Test
    public void testAddSanitizer_sameIdOnDifferentCalls() {
        final SaveInfo.Builder builder = new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC,
                mIdArray);
        builder.addSanitizer(mSanitizer, mId);
        assertThrows(IllegalArgumentException.class, () -> builder.addSanitizer(mSanitizer, mId));
    }

    @Test
    public void testBuild_invalid() {
        // No nothing
        assertThrows(IllegalStateException.class, () -> new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC)
                .build());
        // Flag only, but invalid flag
        assertThrows(IllegalStateException.class, () -> new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC)
                .setFlags(FLAG_DONT_SAVE_ON_FINISH).build());
    }

    @Test
    public void testBuild_valid() {
        // Required ids
        assertThat(new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC, mIdArray)
                .build()).isNotNull();

        // Optional ids
        assertThat(new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC).setOptionalIds(mIdArray)
                .build()).isNotNull();

        // Delayed save
        assertThat(new SaveInfo.Builder(SAVE_DATA_TYPE_GENERIC).setFlags(FLAG_DELAY_SAVE)
                .build()).isNotNull();
    }

}
