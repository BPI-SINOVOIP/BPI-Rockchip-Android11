/*
 * Copyright (C) 2014 The Android Open Source Project
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

package android.os.cts;

import android.test.AndroidTestCase;
import android.os.cts.TaggedPointer;
import com.android.compatibility.common.util.CpuFeatures;

public class TaggedPointerTest extends AndroidTestCase {

    @Override
    protected void setUp() throws Exception {
        super.setUp();
    }

    public void testHasTaggedPointer() {
        // Tagged pointers are only supported on arm64. But if CPU is native bridged then the host
        // CPU is not arm64, so there is no tagged pointers support either.
        if (!CpuFeatures.isArm64Cpu() || CpuFeatures.isNativeBridgedCpu()) {
            return;
        }
        assertTrue("Machine does not support tagged pointers", TaggedPointer.hasTaggedPointer());
    }
}
