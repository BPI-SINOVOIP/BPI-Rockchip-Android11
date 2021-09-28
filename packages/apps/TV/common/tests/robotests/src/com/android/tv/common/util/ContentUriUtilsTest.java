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
package com.android.tv.common.util;

import static com.google.common.truth.Truth.assertThat;

import android.net.Uri;

import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for {@link ContentUriUtils}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class ContentUriUtilsTest {

    @Test
    public void safeParseId_empty() {
        assertThat(ContentUriUtils.safeParseId(Uri.EMPTY)).isEqualTo(-1);
    }

    @Test
    public void safeParseId_bad() {
        assertThat(ContentUriUtils.safeParseId(Uri.parse("foo/bad"))).isEqualTo(-1);
    }

    @Test
    public void safeParseId_123() {
        assertThat(ContentUriUtils.safeParseId(Uri.parse("foo/123"))).isEqualTo(123);
    }
}
