/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package android.content.cts;

import static org.junit.Assert.assertEquals;

import android.content.ContentProviderResult;
import android.net.Uri;
import android.os.Bundle;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class ContentProviderResultTest {
    private final Uri TEST_URI = Uri.EMPTY;
    private final Bundle TEST_BUNDLE = Bundle.EMPTY;
    private final Exception TEST_EXCEPTION = new IllegalArgumentException();

    @Test
    public void testUri() throws Exception {
        assertEquals(TEST_URI, new ContentProviderResult(TEST_URI).uri);
    }

    @Test
    public void testCount() throws Exception {
        assertEquals(42, (int) new ContentProviderResult(42).count);
    }

    @Test
    public void testExtras() throws Exception {
        assertEquals(TEST_BUNDLE, new ContentProviderResult(TEST_BUNDLE).extras);
    }

    @Test
    public void testException() throws Exception {
        assertEquals(TEST_EXCEPTION, new ContentProviderResult(TEST_EXCEPTION).exception);
    }
}
