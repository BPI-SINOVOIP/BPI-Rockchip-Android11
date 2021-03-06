/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.util.cts;

import static org.junit.Assert.assertEquals;

import android.util.AndroidException;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class AndroidExceptionTest {
    private static final String NAME = "Test_AndroidException";
    private static final Exception CAUSE = new Exception();

    @Test
    public void testAndroidException() {
        try {
            throw new AndroidException();
        } catch (AndroidException e) {
        }

        try {
            throw new AndroidException(NAME);
        } catch (AndroidException e) {
            assertEquals(NAME, e.getMessage());
        }

        try {
            throw new AndroidException(CAUSE);
        } catch (AndroidException e) {
            assertEquals(CAUSE, e.getCause());
        }
    }
}
