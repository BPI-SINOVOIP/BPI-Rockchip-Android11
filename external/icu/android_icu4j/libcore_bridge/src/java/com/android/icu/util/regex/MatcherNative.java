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

package com.android.icu.util.regex;

import libcore.util.NativeAllocationRegistry;

public class MatcherNative {

    private static final NativeAllocationRegistry REGISTRY = NativeAllocationRegistry
        .createMalloced(MatcherNative.class.getClassLoader(), getNativeFinalizer());

    private final PatternNative nativePattern;
    @dalvik.annotation.optimization.ReachabilitySensitive
    private final long address;

    @libcore.api.IntraCoreApi
    public static MatcherNative create(PatternNative pattern) {
        return new MatcherNative(pattern);
    }

    private MatcherNative(PatternNative pattern) {
        nativePattern = pattern;
        address = pattern.openMatcher();
        REGISTRY.registerNativeAllocation(this, address);
    }

    @libcore.api.IntraCoreApi
    public int getMatchedGroupIndex(String groupName) {
        return nativePattern.getMatchedGroupIndex(groupName);
    }

    @libcore.api.IntraCoreApi
    public boolean find(int startIndex, int[] offsets) {
        return findImpl(address, startIndex, offsets);
    }

    @libcore.api.IntraCoreApi
    public boolean findNext(int[] offsets) {
        return findNextImpl(address, offsets);
    }

    @libcore.api.IntraCoreApi
    public int groupCount() {
        return groupCountImpl(address);
    }

    @libcore.api.IntraCoreApi
    public boolean hitEnd() {
        return hitEndImpl(address);
    }

    @libcore.api.IntraCoreApi
    public boolean lookingAt(int[] offsets) {
        return lookingAtImpl(address, offsets);
    }

    @libcore.api.IntraCoreApi
    public boolean matches(int[] offsets) {
        return matchesImpl(address, offsets);
    }

    @libcore.api.IntraCoreApi
    public boolean requireEnd() {
        return requireEndImpl(address);
    }

    @libcore.api.IntraCoreApi
    public void setInput(String input, int start, int end) {
        setInputImpl(address, input, start, end);
    }

    @libcore.api.IntraCoreApi
    public void useAnchoringBounds(boolean value) {
        useAnchoringBoundsImpl(address, value);
    }

    @libcore.api.IntraCoreApi
    public void useTransparentBounds(boolean value) {
        useTransparentBoundsImpl(address, value);
    }

    private static native boolean findImpl(long addr, int startIndex, int[] offsets);
    private static native boolean findNextImpl(long addr, int[] offsets);
    private static native int groupCountImpl(long addr);
    private static native boolean hitEndImpl(long addr);
    private static native boolean lookingAtImpl(long addr, int[] offsets);
    private static native boolean matchesImpl(long addr, int[] offsets);
    private static native boolean requireEndImpl(long addr);
    private static native void setInputImpl(long addr, String input, int start, int end);
    private static native void useAnchoringBoundsImpl(long addr, boolean value);
    private static native void useTransparentBoundsImpl(long addr, boolean value);

    /**
     * @return address of a native function of type <code>void f(void* nativePtr)</code>
     *         used to free this kind of native allocation
     */
    private static native long getNativeFinalizer();

}
