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

package android.cts.environment;

import android.os.Environment;

import androidx.test.runner.AndroidJUnit4;

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class EnvironmentTest {

  @Test
  public void testIsExternalStorageManagerFalse() {
    assertFalse(Environment.isExternalStorageManager());
  }

  @Test
  public void testIsExternalStorageManagerTrue() {
    assertTrue(Environment.isExternalStorageManager());
  }
}
