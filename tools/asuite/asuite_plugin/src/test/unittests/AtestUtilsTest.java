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
package unittests;

import com.android.atest.AtestUtils;
import org.junit.Assert;
import org.junit.Test;

public class AtestUtilsTest {

    /** Tests GetAndroidRoot. */
    @Test
    public void testGetAndroidRoot() {
        Assert.assertEquals(
                AtestUtils.getAndroidRoot("src/test/resources/root/a/b/c"),
                "src/test/resources/root");
        Assert.assertNull(AtestUtils.getAndroidRoot("src/test"));
    }

    /** Tests hasTestMapping. */
    @Test
    public void testhasTestMapping() {
        Assert.assertTrue(AtestUtils.hasTestMapping("src/test/resources/root/a/b"));
        Assert.assertFalse(AtestUtils.hasTestMapping("src/test/resources/root/a/b/c"));
    }
}
