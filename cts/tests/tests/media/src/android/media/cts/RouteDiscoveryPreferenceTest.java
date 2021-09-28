/*
 * Copyright 2020 The Android Open Source Project
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

package android.media.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.testng.Assert.assertThrows;

import android.media.RouteDiscoveryPreference;
import android.os.Parcel;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

@RunWith(AndroidJUnit4.class)
@SmallTest
@NonMediaMainlineTest
public class RouteDiscoveryPreferenceTest {

    private static final String TEST_FEATURE_1 = "TEST_FEATURE_1";
    private static final String TEST_FEATURE_2 = "TEST_FEATURE_2";

    @Test
    public void testBuilderConstructorWithNull() {
        // Tests null preferredFeatures
        assertThrows(NullPointerException.class,
                () -> new RouteDiscoveryPreference.Builder(null, true));

        // Tests null RouteDiscoveryPreference
        assertThrows(NullPointerException.class,
                () -> new RouteDiscoveryPreference.Builder((RouteDiscoveryPreference) null));
    }

    @Test
    public void testBuilderSetPreferredFeaturesWithNull() {
        RouteDiscoveryPreference.Builder builder =
                new RouteDiscoveryPreference.Builder(new ArrayList<>(), true);

        assertThrows(NullPointerException.class, () -> builder.setPreferredFeatures(null));
    }

    @Test
    public void testGetters() {
        final List<String> features = new ArrayList<>();
        features.add(TEST_FEATURE_1);
        features.add(TEST_FEATURE_2);

        RouteDiscoveryPreference preference =
                new RouteDiscoveryPreference.Builder(features, true /* isActiveScan */).build();
        assertEquals(features, preference.getPreferredFeatures());
        assertTrue(preference.shouldPerformActiveScan());
        assertEquals(0, preference.describeContents());
    }

    @Test
    public void testBuilderSetPreferredFeatures() {
        final List<String> features = new ArrayList<>();
        features.add(TEST_FEATURE_1);
        features.add(TEST_FEATURE_2);

        RouteDiscoveryPreference preference =
                new RouteDiscoveryPreference.Builder(features, true /* isActiveScan */).build();

        final List<String> newFeatures = new ArrayList<>();
        newFeatures.add(TEST_FEATURE_1);

        // Using copy constructor, we only change the setPreferredFeatures.
        RouteDiscoveryPreference newPreference = new RouteDiscoveryPreference.Builder(preference)
                .setPreferredFeatures(newFeatures)
                .build();

        assertEquals(newFeatures, newPreference.getPreferredFeatures());
        assertTrue(newPreference.shouldPerformActiveScan());
    }

    @Test
    public void testBuilderSetActiveScan() {
        final List<String> features = new ArrayList<>();
        features.add(TEST_FEATURE_1);
        features.add(TEST_FEATURE_2);

        RouteDiscoveryPreference preference =
                new RouteDiscoveryPreference.Builder(features, true /* isActiveScan */).build();

        // Using copy constructor, we only change the activeScan to 'false'.
        RouteDiscoveryPreference newPreference = new RouteDiscoveryPreference.Builder(preference)
                .setShouldPerformActiveScan(false)
                .build();

        assertEquals(features, newPreference.getPreferredFeatures());
        assertFalse(newPreference.shouldPerformActiveScan());
    }

    @Test
    public void testEqualsCreatedWithSameArguments() {
        final List<String> features = new ArrayList<>();
        features.add(TEST_FEATURE_1);
        features.add(TEST_FEATURE_2);

        RouteDiscoveryPreference preference1 =
                new RouteDiscoveryPreference.Builder(features, true /* isActiveScan */).build();

        RouteDiscoveryPreference preference2 =
                new RouteDiscoveryPreference.Builder(features, true /* isActiveScan */).build();

        assertEquals(preference1, preference2);
    }

    @Test
    public void testEqualsCreatedWithBuilderCopyConstructor() {
        final List<String> features = new ArrayList<>();
        features.add(TEST_FEATURE_1);
        features.add(TEST_FEATURE_2);

        RouteDiscoveryPreference preference1 =
                new RouteDiscoveryPreference.Builder(features, true /* isActiveScan */).build();

        RouteDiscoveryPreference preference2 =
                new RouteDiscoveryPreference.Builder(preference1).build();

        assertEquals(preference1, preference2);
    }

    @Test
    public void testEqualsReturnFalse() {
        final List<String> features = new ArrayList<>();
        features.add(TEST_FEATURE_1);
        features.add(TEST_FEATURE_2);

        RouteDiscoveryPreference preference =
                new RouteDiscoveryPreference.Builder(features, true /* isActiveScan */).build();

        RouteDiscoveryPreference preferenceWithDifferentFeatures =
                new RouteDiscoveryPreference.Builder(new ArrayList<>(), true /* isActiveScan */)
                        .build();
        assertNotEquals(preference, preferenceWithDifferentFeatures);

        RouteDiscoveryPreference preferenceWithDifferentActiveScan =
                new RouteDiscoveryPreference.Builder(features, false /* isActiveScan */)
                        .build();
        assertNotEquals(preference, preferenceWithDifferentActiveScan);
    }

    @Test
    public void testEqualsReturnFalseWithCopyConstructor() {
        final List<String> features = new ArrayList<>();
        features.add(TEST_FEATURE_1);
        features.add(TEST_FEATURE_2);

        RouteDiscoveryPreference preference =
                new RouteDiscoveryPreference.Builder(features, true /* isActiveScan */).build();

        final List<String> newFeatures = new ArrayList<>();
        newFeatures.add(TEST_FEATURE_1);
        RouteDiscoveryPreference preferenceWithDifferentFeatures =
                new RouteDiscoveryPreference.Builder(preference)
                        .setPreferredFeatures(newFeatures)
                        .build();
        assertNotEquals(preference, preferenceWithDifferentFeatures);

        RouteDiscoveryPreference preferenceWithDifferentActiveScan =
                new RouteDiscoveryPreference.Builder(preference)
                        .setShouldPerformActiveScan(false)
                        .build();
        assertNotEquals(preference, preferenceWithDifferentActiveScan);
    }

    @Test
    public void testParcelingAndUnParceling() {
        final List<String> features = new ArrayList<>();
        features.add(TEST_FEATURE_1);
        features.add(TEST_FEATURE_2);

        RouteDiscoveryPreference preference =
                new RouteDiscoveryPreference.Builder(features, true /* isActiveScan */).build();

        Parcel parcel = Parcel.obtain();
        parcel.writeParcelable(preference, 0);
        parcel.setDataPosition(0);

        RouteDiscoveryPreference preferenceFromParcel = parcel.readParcelable(null);
        assertEquals(preference, preferenceFromParcel);
        parcel.recycle();

        // In order to mark writeToParcel as tested, we let's just call it directly.
        Parcel dummyParcel = Parcel.obtain();
        preference.writeToParcel(dummyParcel, 0);
        dummyParcel.recycle();
    }
}
