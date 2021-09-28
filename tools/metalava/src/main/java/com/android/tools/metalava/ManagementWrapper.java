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

package com.android.tools.metalava;

import java.lang.reflect.Method;
import java.util.List;

/**
 * A collection of wrapper functions for java.lang.management APIs. Unfortunately we need to
 * use reflections for them due to b/151224432.
 *
 * TODO(b/151224432) Resolve the device-side dependency and stop using reflections here.
 *
 * To avoid extra possible allocations around casts in kotlin, it's written in Java.
 */
final class ManagementWrapper {
    private static Class<?>[] EMPTY_CLASSES = new Class[]{};
    private static Object[] EMPTY_ARGS = new Object[]{};

    private static void onReflectionException(Exception e) {
        throw new RuntimeException(e);
    }

    private abstract static class Lazy<T> {
        private boolean mInitialized;
        private T mValue;

        abstract T init();

        public T get() {
            if (!mInitialized) {
                mValue = init();
                mInitialized = true;
            }
            return mValue;
        }
    }

    private static class LazyClass extends Lazy<Class<?>> {
        private final String mName;

        private LazyClass(String name) {
            mName = name;
        }

        @Override
        Class<?> init() {
            try {
                return Class.forName(mName);
            } catch (Exception e) {
                onReflectionException(e);
                return null;
            }
        }
    }

    private static class LazyMethod<TV> extends Lazy<Method>  {
        private final LazyClass mClazz;
        private final String mName;
        private TV mCachedInvokeResult;
        private TV mDefault;

        private LazyMethod(LazyClass clazz, String name) {
            this(clazz, name, null);
        }

        private LazyMethod(LazyClass clazz, String name, TV defaultValue) {
            mClazz = clazz;
            mName = name;
            mDefault = defaultValue;
        }

        @Override
        Method init() {
            try {
                final Class<?> clazz = mClazz.get();
                final Method m = clazz.getMethod(mName, EMPTY_CLASSES);
                m.setAccessible(true);
                return m;
            } catch (Exception e) {
                onReflectionException(e);
                return null;
            }
        }

        @SuppressWarnings("unchecked")
        public TV invoke(Object target) {
            try {
                return (TV) get().invoke(target, EMPTY_ARGS);
            } catch (Exception e) {
                onReflectionException(e);
                return mDefault;
            }
        }

        public TV invokeOnce(Object target) {
            if (mCachedInvokeResult == null) {
                mCachedInvokeResult = invoke(target);
            }
            return mCachedInvokeResult;
        }
    }

    private static LazyClass sFactoryClazz = new LazyClass("java.lang.management.ManagementFactory");
    private static LazyClass sThreadMxClazz = new LazyClass("java.lang.management.ThreadMXBean");
    private static LazyClass sRuntimeMxClazz = new LazyClass("java.lang.management.RuntimeMXBean");
    private static LazyClass sMemoryMxClazz = new LazyClass("java.lang.management.MemoryMXBean");
    private static LazyClass sMemoryUsageClazz = new LazyClass("java.lang.management.MemoryUsage");

    private static LazyMethod<Object> sRuntimeMxGetter = new LazyMethod<>(sFactoryClazz, "getRuntimeMXBean");
    private static LazyMethod<Object> sThreadMxGetter = new LazyMethod<>(sFactoryClazz, "getThreadMXBean");
    private static LazyMethod<Object> sMemoryMxGetter = new LazyMethod<>(sFactoryClazz, "getMemoryMXBean");

    // RuntimeMxBean functions
    private static LazyMethod<List<String>> sGetInputArguments = new LazyMethod<>(sRuntimeMxClazz, "getInputArguments");

    /** This corresponds to RuntimeMxBean.getInputArguments() */
    public static List<String> getVmArguments() {
        return sGetInputArguments.invoke(sRuntimeMxGetter.invokeOnce(null));
    }

    // ThreadMxBean functions
    private static LazyMethod<Long> sUserTime = new LazyMethod<>(sThreadMxClazz, "getCurrentThreadUserTime", 0L);
    private static LazyMethod<Long> sCpuTime = new LazyMethod<>(sThreadMxClazz, "getCurrentThreadCpuTime", 0L);

    /** This corresponds to ThreadMxBean.getCurrentThreadUserTime() */
    public static long getUserTimeNano() {
        return sUserTime.invoke(sThreadMxGetter.invokeOnce(null));
    }

    /** This corresponds to ThreadMxBean.getCurrentThreadCpuTime() */
    public static long getCpuTimeNano() {
        return sCpuTime.invoke(sThreadMxGetter.invokeOnce(null));
    }

    // MemoryMxBean functions

    /** Corresponds to java.lang.management.MemoryUsage */
    public static final class MemoryUsage {
        public final long init;
        public final long used;
        public final long committed;
        public final long max;

        public MemoryUsage(long init, long used, long committed, long max) {
            this.init = init;
            this.used = used;
            this.committed = committed;
            this.max = max;
        }
    }

    // This returns a java.lang.management.MemoryUsage instance.
    private static LazyMethod<Object> sGetHeapMemoryUsage = new LazyMethod<>(sMemoryMxClazz, "getHeapMemoryUsage");
    private static LazyMethod<Long> sGetInitMemory = new LazyMethod<>(sMemoryUsageClazz, "getInit", 0L);
    private static LazyMethod<Long> sGetUsedMemory = new LazyMethod<>(sMemoryUsageClazz, "getUsed", 0L);
    private static LazyMethod<Long> sGetCommittedMemory = new LazyMethod<>(sMemoryUsageClazz, "getCommitted", 0L);
    private static LazyMethod<Long> sGetMaxMemory = new LazyMethod<>(sMemoryUsageClazz, "getMax", 0L);

    /** Corresponds to java.lang.management.MemoryUsage.getHeapMemoryUsage() */
    public static MemoryUsage getHeapUsage() {
        final Object heapUsage = sGetHeapMemoryUsage.invoke(sMemoryMxGetter.invokeOnce(null));

        return new MemoryUsage(
            sGetInitMemory.invoke(heapUsage),
            sGetUsedMemory.invoke(heapUsage),
            sGetCommittedMemory.invoke(heapUsage),
            sGetMaxMemory.invoke(heapUsage)
        );
    }
}
