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

import static android.view.contentcapture.DataRemovalRequest.FLAG_IS_PREFIX;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.testng.Assert.assertThrows;

import android.content.LocusId;
import android.os.Parcel;
import android.platform.test.annotations.AppModeFull;
import android.view.contentcapture.DataRemovalRequest;
import android.view.contentcapture.DataRemovalRequest.LocusIdRequest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.List;

@AppModeFull(reason = "unit test")
@RunWith(MockitoJUnitRunner.class)
public class DataRemovalRequestTest {

    private static final int NO_FLAGS = 0;

    private final LocusId mLocusId = new LocusId("content://com.example/");

    private DataRemovalRequest.Builder mBuilder = new DataRemovalRequest.Builder();

    @Test
    public void testBuilder_addLocusId_invalid() {
        assertThrows(NullPointerException.class, () -> mBuilder.addLocusId(null, NO_FLAGS));
        assertThrows(NullPointerException.class, () -> mBuilder.addLocusId(null, 666));
    }

    @Test
    public void testBuilder_addLocusId_valid() {
        assertThat(mBuilder.addLocusId(mLocusId, NO_FLAGS)).isNotNull();
        assertThat(mBuilder.addLocusId(new LocusId("content://com.example2"), FLAG_IS_PREFIX))
                .isNotNull();
    }

    @Test
    public void testBuilder_addLocusIdAfterForEverything() {
        assertThat(mBuilder.forEverything()).isNotNull();
        assertThrows(IllegalStateException.class, () -> mBuilder.addLocusId(mLocusId, NO_FLAGS));
    }

    @Test
    public void testBuilder_forEverythingAfterAddingLocusId() {
        assertThat(mBuilder.addLocusId(mLocusId, NO_FLAGS)).isNotNull();
        assertThrows(IllegalStateException.class, () -> mBuilder.forEverything());
    }

    @Test
    public void testBuild_invalid() {
        assertThrows(IllegalStateException.class, () -> mBuilder.build());
    }

    @Test
    public void testBuild_validForEverything_directly() {
        final DataRemovalRequest request = new DataRemovalRequest.Builder().forEverything()
                .build();
        assertForEverything(request);
    }

    @Test
    public void testBuild_validForEverything_parcel() {
        final DataRemovalRequest request = new DataRemovalRequest.Builder().forEverything()
                .build();
        final DataRemovalRequest clone = cloneThroughParcel(request);
        assertForEverything(clone);
    }

    private void assertForEverything(final DataRemovalRequest request) {
        assertThat(request).isNotNull();
        assertThat(request.isForEverything()).isTrue();
        assertThat(request.getLocusIdRequests()).isNull();
    }

    @Test
    public void testBuild_validForIds_directly() {
        final DataRemovalRequest request = buildForIds();
        assertForIds(request);
    }

    @Test
    public void testBuild_validForIds_parcel() {
        final DataRemovalRequest request = buildForIds();
        final DataRemovalRequest clone = cloneThroughParcel(request);
        assertForIds(clone);
    }

    private DataRemovalRequest buildForIds() {
        final DataRemovalRequest.Builder builder = new DataRemovalRequest.Builder();
        assertThat(builder.addLocusId(new LocusId("prefix1True"), FLAG_IS_PREFIX)).isNotNull();
        assertThat(builder.addLocusId(new LocusId("prefix2False"), NO_FLAGS)).isNotNull();

        return builder.build();
    }

    private void assertForIds(DataRemovalRequest request) {
        assertThat(request).isNotNull();
        assertThat(request.isForEverything()).isFalse();
        final List<LocusIdRequest> requests = request.getLocusIdRequests();
        assertThat(requests).isNotNull();
        assertThat(requests).hasSize(2);
        assertRequest(requests, 0, "prefix1True", FLAG_IS_PREFIX);
        assertRequest(requests, 1, "prefix2False", NO_FLAGS);
    }

    @Test
    public void testNoMoreInteractionsAfterBuild() {
        assertThat(mBuilder.forEverything().build()).isNotNull();

        assertThrows(IllegalStateException.class, () -> mBuilder.addLocusId(mLocusId, NO_FLAGS));
        assertThrows(IllegalStateException.class,
                () -> mBuilder.addLocusId(mLocusId, FLAG_IS_PREFIX));
        assertThrows(IllegalStateException.class, () -> mBuilder.forEverything());
        assertThrows(IllegalStateException.class, () -> mBuilder.build());
    }

    private void assertRequest(List<LocusIdRequest> requests, int index, String expectedId,
            int expectedFlags) {
        final LocusIdRequest request = requests.get(index);
        assertWithMessage("no request at index %s: %s", index, requests).that(request).isNotNull();
        final LocusId actualId = request.getLocusId();
        assertWithMessage("no id at index %s: %s", index, request).that(actualId).isNotNull();
        assertWithMessage("wrong id at index %s: %s", index, request).that(actualId.getId())
                .isEqualTo(expectedId);
        assertWithMessage("wrong flags at index %s: %s", index, request)
                .that(request.getFlags()).isEqualTo(expectedFlags);
    }

    private DataRemovalRequest cloneThroughParcel(DataRemovalRequest request) {
        final Parcel parcel = Parcel.obtain();

        try {
            // Write to parcel
            parcel.setDataPosition(0); // Sanity / paranoid check
            request.writeToParcel(parcel, 0);

            // Read from parcel
            parcel.setDataPosition(0);
            final DataRemovalRequest clone = DataRemovalRequest.CREATOR
                    .createFromParcel(parcel);
            assertThat(clone).isNotNull();
            return clone;
        } finally {
            parcel.recycle();
        }
    }
}
