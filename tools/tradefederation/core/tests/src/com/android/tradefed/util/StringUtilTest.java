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
package com.android.tradefed.util;

import static org.junit.Assert.*;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link StringUtil} */
@RunWith(JUnit4.class)
public class StringUtilTest {

    @Test
    public void testExpand() {
        Map<String, String> valueMap = new HashMap<>();
        valueMap.put("FOO", "foo");
        valueMap.put("BAR", "bar");

        assertEquals("foo-bar-zzz", StringUtil.expand("${FOO}-${BAR}-zzz", valueMap));
        assertEquals("foo-bar-", StringUtil.expand("${FOO}-${BAR}-${ZZZ}", valueMap));
        assertEquals("${FOO-bar-$ZZZ}", StringUtil.expand("${FOO-${BAR}-$ZZZ}", valueMap));
    }
}
