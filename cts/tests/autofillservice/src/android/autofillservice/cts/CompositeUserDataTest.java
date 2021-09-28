/*
 * Copyright (C) 2018 The Android Open Source Project
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

import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;
import android.service.autofill.CompositeUserData;
import android.service.autofill.UserData;
import android.util.ArrayMap;

import com.google.common.base.Strings;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnitRunner;

@RunWith(MockitoJUnitRunner.class)
@AppModeFull(reason = "Unit test")
public class CompositeUserDataTest {

    private final String mShortValue = Strings.repeat("k", UserData.getMinValueLength() - 1);
    private final String mLongValue = "LONG VALUE, Y U NO SHORTER"
            + Strings.repeat("?", UserData.getMaxValueLength());
    private final String mId = "4815162342";
    private final String mId2 = "4815162343";
    private final String mCategoryId = "id1";
    private final String mCategoryId2 = "id2";
    private final String mCategoryId3 = "id3";
    private final String mValue = mShortValue + "-1";
    private final String mValue2 = mShortValue + "-2";
    private final String mValue3 = mShortValue + "-3";
    private final String mValue4 = mShortValue + "-4";
    private final String mValue5 = mShortValue + "-5";
    private final String mAlgo = "algo";
    private final String mAlgo2 = "algo2";
    private final String mAlgo3 = "algo3";
    private final String mAlgo4 = "algo4";

    private final UserData mEmptyGenericUserData = new UserData.Builder(mId, mValue, mCategoryId)
            .build();
    private final UserData mLoadedGenericUserData = new UserData.Builder(mId, mValue, mCategoryId)
            .add(mValue2, mCategoryId2)
            .setFieldClassificationAlgorithm(mAlgo, createBundle(false))
            .setFieldClassificationAlgorithmForCategory(mCategoryId2, mAlgo2, createBundle(false))
            .build();
    private final UserData mEmptyPackageUserData = new UserData.Builder(mId2, mValue3, mCategoryId3)
            .build();
    private final UserData mLoadedPackageUserData = new UserData
            .Builder(mId2, mValue3, mCategoryId3)
            .add(mValue4, mCategoryId2)
            .setFieldClassificationAlgorithm(mAlgo3, createBundle(true))
            .setFieldClassificationAlgorithmForCategory(mCategoryId2, mAlgo4, createBundle(true))
            .build();


    @Test
    public void testMergeInvalid_bothNull() {
        assertThrows(NullPointerException.class, () -> new CompositeUserData(null, null));
    }

    @Test
    public void testMergeInvalid_nullPackageUserData() {
        assertThrows(NullPointerException.class,
                () -> new CompositeUserData(mEmptyGenericUserData, null));
    }

    @Test
    public void testMerge_nullGenericUserData() {
        final CompositeUserData userData = new CompositeUserData(null, mEmptyPackageUserData);

        final String[] categoryIds = userData.getCategoryIds();
        assertThat(categoryIds.length).isEqualTo(1);
        assertThat(categoryIds[0]).isEqualTo(mCategoryId3);

        final String[] values = userData.getValues();
        assertThat(values.length).isEqualTo(1);
        assertThat(values[0]).isEqualTo(mValue3);

        assertThat(userData.getFieldClassificationAlgorithm()).isNull();
        assertThat(userData.getDefaultFieldClassificationArgs()).isNull();
    }

    @Test
    public void testMerge_bothEmpty() {
        final CompositeUserData userData = new CompositeUserData(mEmptyGenericUserData,
                mEmptyPackageUserData);

        final String[] categoryIds = userData.getCategoryIds();
        assertThat(categoryIds.length).isEqualTo(2);
        assertThat(categoryIds[0]).isEqualTo(mCategoryId3);
        assertThat(categoryIds[1]).isEqualTo(mCategoryId);

        final String[] values = userData.getValues();
        assertThat(values.length).isEqualTo(2);
        assertThat(values[0]).isEqualTo(mValue3);
        assertThat(values[1]).isEqualTo(mValue);

        assertThat(userData.getFieldClassificationAlgorithm()).isNull();
        assertThat(userData.getDefaultFieldClassificationArgs()).isNull();
    }

    @Test
    public void testMerge_emptyGenericUserData() {
        final CompositeUserData userData = new CompositeUserData(mEmptyGenericUserData,
                mLoadedPackageUserData);

        final String[] categoryIds = userData.getCategoryIds();
        assertThat(categoryIds.length).isEqualTo(3);
        assertThat(categoryIds[0]).isEqualTo(mCategoryId3);
        assertThat(categoryIds[1]).isEqualTo(mCategoryId2);
        assertThat(categoryIds[2]).isEqualTo(mCategoryId);

        final String[] values = userData.getValues();
        assertThat(values.length).isEqualTo(3);
        assertThat(values[0]).isEqualTo(mValue3);
        assertThat(values[1]).isEqualTo(mValue4);
        assertThat(values[2]).isEqualTo(mValue);

        assertThat(userData.getFieldClassificationAlgorithm()).isEqualTo(mAlgo3);

        final Bundle defaultArgs = userData.getDefaultFieldClassificationArgs();
        assertThat(defaultArgs).isNotNull();
        assertThat(defaultArgs.getBoolean("isPackage")).isTrue();
        assertThat(userData.getFieldClassificationAlgorithmForCategory(mCategoryId2))
                .isEqualTo(mAlgo4);

        final ArrayMap<String, Bundle> args = userData.getFieldClassificationArgs();
        assertThat(args.size()).isEqualTo(1);
        assertThat(args.containsKey(mCategoryId2)).isTrue();
        assertThat(args.get(mCategoryId2)).isNotNull();
        assertThat(args.get(mCategoryId2).getBoolean("isPackage")).isTrue();
    }

    @Test
    public void testMerge_emptyPackageUserData() {
        final CompositeUserData userData = new CompositeUserData(mLoadedGenericUserData,
                mEmptyPackageUserData);

        final String[] categoryIds = userData.getCategoryIds();
        assertThat(categoryIds.length).isEqualTo(3);
        assertThat(categoryIds[0]).isEqualTo(mCategoryId3);
        assertThat(categoryIds[1]).isEqualTo(mCategoryId);
        assertThat(categoryIds[2]).isEqualTo(mCategoryId2);

        final String[] values = userData.getValues();
        assertThat(values.length).isEqualTo(3);
        assertThat(values[0]).isEqualTo(mValue3);
        assertThat(values[1]).isEqualTo(mValue);
        assertThat(values[2]).isEqualTo(mValue2);

        assertThat(userData.getFieldClassificationAlgorithm()).isEqualTo(mAlgo);

        final Bundle defaultArgs = userData.getDefaultFieldClassificationArgs();
        assertThat(defaultArgs).isNotNull();
        assertThat(defaultArgs.getBoolean("isPackage")).isFalse();
        assertThat(userData.getFieldClassificationAlgorithmForCategory(mCategoryId2))
                .isEqualTo(mAlgo2);

        final ArrayMap<String, Bundle> args = userData.getFieldClassificationArgs();
        assertThat(args.size()).isEqualTo(1);
        assertThat(args.containsKey(mCategoryId2)).isTrue();
        assertThat(args.get(mCategoryId2)).isNotNull();
        assertThat(args.get(mCategoryId2).getBoolean("isPackage")).isFalse();
    }


    @Test
    public void testMerge_bothHaveData() {
        final CompositeUserData userData = new CompositeUserData(mLoadedGenericUserData,
                mLoadedPackageUserData);

        final String[] categoryIds = userData.getCategoryIds();
        assertThat(categoryIds.length).isEqualTo(3);
        assertThat(categoryIds[0]).isEqualTo(mCategoryId3);
        assertThat(categoryIds[1]).isEqualTo(mCategoryId2);
        assertThat(categoryIds[2]).isEqualTo(mCategoryId);

        final String[] values = userData.getValues();
        assertThat(values.length).isEqualTo(3);
        assertThat(values[0]).isEqualTo(mValue3);
        assertThat(values[1]).isEqualTo(mValue4);
        assertThat(values[2]).isEqualTo(mValue);

        assertThat(userData.getFieldClassificationAlgorithm()).isEqualTo(mAlgo3);
        assertThat(userData.getDefaultFieldClassificationArgs()).isNotNull();
        assertThat(userData.getFieldClassificationAlgorithmForCategory(mCategoryId2))
                .isEqualTo(mAlgo4);

        final Bundle defaultArgs = userData.getDefaultFieldClassificationArgs();
        assertThat(defaultArgs).isNotNull();
        assertThat(defaultArgs.getBoolean("isPackage")).isTrue();
        assertThat(userData.getFieldClassificationAlgorithmForCategory(mCategoryId2))
                .isEqualTo(mAlgo4);

        final ArrayMap<String, Bundle> args = userData.getFieldClassificationArgs();
        assertThat(args.size()).isEqualTo(1);
        assertThat(args.containsKey(mCategoryId2)).isTrue();
        assertThat(args.get(mCategoryId2)).isNotNull();
        assertThat(args.get(mCategoryId2).getBoolean("isPackage")).isTrue();
    }

    private Bundle createBundle(Boolean isPackageBundle) {
        final Bundle bundle = new Bundle();
        bundle.putBoolean("isPackage", isPackageBundle);
        return bundle;
    }
}
