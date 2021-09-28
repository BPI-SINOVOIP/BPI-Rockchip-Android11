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
 * limitations under the License
 */
package com.android.tv.common.customization;

import static com.google.common.truth.Truth.assertThat;

import android.content.pm.PackageInfo;

import androidx.test.core.content.pm.PackageInfoBuilder;

import com.android.tv.testing.constants.ConfigConstants;

import com.google.common.collect.ImmutableList;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for {@link CustomizationManager}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class CustomizationManagerTest {

    @Test
    public void getCustomizationPackageName_empty() {
        assertThat(CustomizationManager.getCustomizationPackageName(ImmutableList.of())).isEmpty();
    }

    @Test
    public void getCustomizationPackageName_one() {
        assertThat(
                        CustomizationManager.getCustomizationPackageName(
                                ImmutableList.of(createPackageInfo("com.example.one"))))
                .isEqualTo("com.example.one");
    }

    @Test
    public void getCustomizationPackageName_first() {
        assertThat(
                        CustomizationManager.getCustomizationPackageName(
                                ImmutableList.of(
                                        createPackageInfo("com.example.one"),
                                        createPackageInfo("com.example.two"))))
                .isEqualTo("com.example.one");
    }

    @Test
    public void getCustomizationPackageName_firstNotAndroid() {
        assertThat(
                        CustomizationManager.getCustomizationPackageName(
                                ImmutableList.of(
                                        createPackageInfo("com.android.one"),
                                        createPackageInfo("com.example.two"),
                                        createPackageInfo("com.example.three"))))
                .isEqualTo("com.example.two");
    }

    @Test
    public void getCustomizationPackageName_androidOnly() {
        assertThat(
                        CustomizationManager.getCustomizationPackageName(
                                ImmutableList.of(
                                        createPackageInfo("com.android.one"),
                                        createPackageInfo("com.android.two"))))
                .isEqualTo("com.android.one");
    }

    private static PackageInfo createPackageInfo(String packageName) {
        return PackageInfoBuilder.newBuilder().setPackageName(packageName).build();
    }
}
