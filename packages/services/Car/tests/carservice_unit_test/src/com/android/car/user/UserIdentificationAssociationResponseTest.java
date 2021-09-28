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
package com.android.car.user;

import static android.car.user.UserIdentificationAssociationResponse.forFailure;
import static android.car.user.UserIdentificationAssociationResponse.forSuccess;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.car.user.UserIdentificationAssociationResponse;

import org.junit.Test;

public final class UserIdentificationAssociationResponseTest {

    @Test
    public void testFailure_noMessage() {
        UserIdentificationAssociationResponse response = forFailure();

        assertThat(response).isNotNull();
        assertThat(response.isSuccess()).isFalse();
        assertThat(response.getErrorMessage()).isNull();
        assertThat(response.getValues()).isNull();
    }

    @Test
    public void testFailure_withMessage() {
        UserIdentificationAssociationResponse response = forFailure("D'OH!");

        assertThat(response).isNotNull();
        assertThat(response.isSuccess()).isFalse();
        assertThat(response.getErrorMessage()).isEqualTo("D'OH!");
        assertThat(response.getValues()).isNull();
    }

    @Test
    public void testSuccess_nullValues() {
        assertThrows(IllegalArgumentException.class, () -> forSuccess(null));
    }

    @Test
    public void testSuccess_nullValuesWithMessage() {
        assertThrows(IllegalArgumentException.class, () -> forSuccess(null, "D'OH!"));
    }

    @Test
    public void testSuccess_emptyValues() {
        assertThrows(IllegalArgumentException.class, () -> forSuccess(new int[0]));
    }

    @Test
    public void testSuccess_emptyValuesWithMessage() {
        assertThrows(IllegalArgumentException.class, () -> forSuccess(new int[0], "D'OH!"));
    }

    @Test
    public void testSuccess_noMessage() {
        UserIdentificationAssociationResponse response = forSuccess(new int[] {1, 2, 3});

        assertThat(response).isNotNull();
        assertThat(response.isSuccess()).isTrue();
        assertThat(response.getErrorMessage()).isNull();
        assertThat(response.getValues()).asList().containsAllOf(1, 2, 3).inOrder();
    }

    @Test
    public void testSuccess_withMessage() {
        UserIdentificationAssociationResponse response = forSuccess(new int[] {1, 2, 3}, "D'OH!");

        assertThat(response).isNotNull();
        assertThat(response.isSuccess()).isTrue();
        assertThat(response.getErrorMessage()).isEqualTo("D'OH!");
        assertThat(response.getValues()).asList().containsAllOf(1, 2, 3).inOrder();
    }
}
