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

package com.android.tools.layoutlib.create.dataclass;

import com.android.tools.layoutlib.create.DelegateClassAdapterTest;

import java.util.List;

/**
 * Test class with an inner class.
 *
 * Used by {@link DelegateClassAdapterTest}.
 */
public class OuterClass {
    public static final int OUTER_CONSTANT = 2;
    private int mOuterValue = 1;
    public OuterClass() {
    }

    // Outer.get returns 1 + a + b
    // Note: it's good to have a long or double for testing parameters since they take
    // 2 slots in the stack/locals maps.
    public int get(int a, long b) {
        return mOuterValue + a + (int) b;
    }

    public static int useLambdas(List<Integer> list) {
        return list.stream().mapToInt(i -> 2 * i).sum();
    }

    public class InnerClass {
        public int mInnerId;

        public InnerClass() {
            mInnerId = 12;
        }

        // Inner.get returns 2 + 1 + a + b
        public int get(int a, long b) {
            return 2 + mOuterValue + a + (int) b;
        }
    }

    public static class StaticInnerClass {
        public static final int INNER_CONSTANT = 3;
        public int mStaticInnerId;

        public StaticInnerClass() {
            mStaticInnerId = 4;
        }

        // StaticInnerClass.get returns 100 + a + b
        public int get(int a, long b) {
            return 100 + a + (int) b;
        }
    }

    @SuppressWarnings("unused")
    private String privateMethod() {
        return "outerPrivateMethod";
    }
}

