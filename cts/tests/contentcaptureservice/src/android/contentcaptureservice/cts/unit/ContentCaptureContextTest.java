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

package android.contentcaptureservice.cts.unit;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.content.LocusId;
import android.os.Bundle;
import android.os.Parcel;
import android.platform.test.annotations.AppModeFull;
import android.view.contentcapture.ContentCaptureContext;
import android.view.contentcapture.ContentCaptureContext.Builder;

import androidx.annotation.NonNull;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@AppModeFull(reason = "unit test")
@RunWith(JUnit4.class)
public class ContentCaptureContextTest {

    private static final String ID = "4815162342";

    private static final LocusId LOCUS_ID = new LocusId(ID);

    private final ContentCaptureContext.Builder mBuilder = new ContentCaptureContext.Builder(
            LOCUS_ID);

    private final Bundle mExtras = new Bundle();

    @Before
    public void setExtras() {
        mExtras.putString("DUDE", "SWEET");
    }

    @Test
    public void testBuilder_invalidId() {
        assertThrows(NullPointerException.class, () -> new Builder(null));
    }

    @Test
    public void testBuilder_invalidExtras() {
        assertThrows(NullPointerException.class, () -> mBuilder.setExtras(null));
    }

    @Test
    public void testAfterBuild_setExtras() {
        assertThat(mBuilder.build()).isNotNull();
        assertThrows(IllegalStateException.class, () -> mBuilder.setExtras(mExtras));
    }

    @Test
    public void testAfterBuild_build() {
        assertThat(mBuilder.setExtras(mExtras).build()).isNotNull();
        assertThrows(IllegalStateException.class, () -> mBuilder.build());
    }

    @Test
    public void testBuild_empty() {
        assertThat(mBuilder.build()).isNotNull();
    }

    @Test
    public void testGetId() {
        final ContentCaptureContext context = mBuilder.build();
        assertThat(context).isNotNull();
        assertThat(context.getLocusId()).isEqualTo(LOCUS_ID);
    }

    @Test
    public void testSetGetBundle() {
        final Builder builder = mBuilder.setExtras(mExtras);
        assertThat(builder).isSameAs(mBuilder);
        final ContentCaptureContext context = builder.build();
        assertThat(context).isNotNull();
        assertExtras(context.getExtras());
    }

    @Test
    public void testParcel() {
        final Builder builder = mBuilder
                .setExtras(mExtras);
        assertThat(builder).isSameAs(mBuilder);
        final ContentCaptureContext context = builder.build();
        assertEverything(context);

        final ContentCaptureContext clone = cloneThroughParcel(context);
        assertEverything(clone);
    }

    @Test
    public void testForLocus_null() {
        assertThrows(IllegalArgumentException.class, () -> ContentCaptureContext.forLocusId(null));
    }

    @Test
    public void testForLocus_valid() {
        final ContentCaptureContext context = ContentCaptureContext.forLocusId(ID);
        assertThat(context).isNotNull();
        assertThat(context.getExtras()).isNull();
        final LocusId locusId = context.getLocusId();
        assertThat(locusId).isNotNull();
        assertThat(locusId.getId()).isEqualTo(ID);
    }

    private void assertEverything(@NonNull ContentCaptureContext context) {
        assertThat(context).isNotNull();
        assertThat(context.getLocusId()).isEqualTo(LOCUS_ID);
        assertExtras(context.getExtras());
    }

    private void assertExtras(@NonNull Bundle bundle) {
        assertThat(bundle).isNotNull();
        assertThat(bundle.keySet()).hasSize(1);
        assertThat(bundle.getString("DUDE")).isEqualTo("SWEET");
    }

    private ContentCaptureContext cloneThroughParcel(@NonNull ContentCaptureContext context) {
        final Parcel parcel = Parcel.obtain();

        try {
            // Write to parcel
            parcel.setDataPosition(0); // Sanity / paranoid check
            context.writeToParcel(parcel, 0);

            // Read from parcel
            parcel.setDataPosition(0);
            final ContentCaptureContext clone = ContentCaptureContext.CREATOR
                    .createFromParcel(parcel);
            assertThat(clone).isNotNull();
            return clone;
        } finally {
            parcel.recycle();
        }
    }

}
