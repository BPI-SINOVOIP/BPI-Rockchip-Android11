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
package android.content.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.content.LocusId;
import android.os.Parcel;
import android.platform.test.annotations.AppModeFull;

import androidx.annotation.NonNull;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@AppModeFull(reason = "unit test")
@RunWith(JUnit4.class)
public class LocusIdTest {

    private static final String ID = "file://dev/null";
    private static final String ANOTHER_ID = "http://h.t.t.p";

    @Test
    public void testConstructor_nullId() {
        assertThrows(IllegalArgumentException.class, () -> new LocusId(null));
    }

    @Test
    public void testConstructor_emptyId() {
        assertThrows(IllegalArgumentException.class, () -> new LocusId(""));
    }

    @Test
    public void testParcel() {
        final LocusId clone = cloneThroughParcel(new LocusId(ID));
        assertThat(clone.getId()).isEqualTo(ID);
    }

    @Test
    public void testEquals() {
        final LocusId id1  = new LocusId(ID);
        final LocusId id2  = new LocusId(ID);
        assertThat(id1).isEqualTo(id2);
        assertThat(id2).isEqualTo(id1);

        final LocusId id3 = new LocusId(ANOTHER_ID);
        assertThat(id1).isNotEqualTo(id3);
        assertThat(id3).isNotEqualTo(id1);
    }

    @Test
    public void testHashcode() {
        final LocusId id1  = new LocusId(ID);
        final LocusId id2  = new LocusId(ID);
        assertThat(id1.hashCode()).isEqualTo(id2.hashCode());
        assertThat(id2.hashCode()).isEqualTo(id1.hashCode());

        final LocusId id3 = new LocusId(ANOTHER_ID);
        assertThat(id1.hashCode()).isNotEqualTo(id3.hashCode());
        assertThat(id3.hashCode()).isNotEqualTo(id1.hashCode());
    }

    private LocusId cloneThroughParcel(@NonNull LocusId original) {
        final Parcel parcel = Parcel.obtain();

        try {
            // Write to parcel
            parcel.setDataPosition(0); // Sanity / paranoid check
            original.writeToParcel(parcel, 0);

            // Read from parcel
            parcel.setDataPosition(0);
            final LocusId clone = LocusId.CREATOR.createFromParcel(parcel);
            assertThat(clone).isNotNull();
            return clone;
        } finally {
            parcel.recycle();
        }
    }
}
