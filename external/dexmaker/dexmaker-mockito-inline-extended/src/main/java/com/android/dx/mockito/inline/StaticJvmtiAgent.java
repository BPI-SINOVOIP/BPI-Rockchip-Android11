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

package com.android.dx.mockito.inline;

import android.os.Build;
import android.os.Debug;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.security.ProtectionDomain;
import java.util.ArrayList;

import dalvik.system.BaseDexClassLoader;

/**
 * Interface to the native jvmti agent in agent.cc
 */
class StaticJvmtiAgent {
    private static final String AGENT_LIB_NAME = "libstaticjvmtiagent.so";

    private static final Object lock = new Object();

    /**
     * Registered byte code transformers
     */
    private final ArrayList<StaticClassTransformer> transformers = new ArrayList<>();

    /**
     * Enable jvmti and load agent.
     * <p><b>If there are more than agent transforming classes the other agent might remove
     * transformations added by this agent.</b>
     *
     * @throws IOException If jvmti could not be enabled or agent could not be loaded
     */
    StaticJvmtiAgent() throws IOException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            throw new IOException("Requires API level " + Build.VERSION_CODES.P + ". API level is "
                    + Build.VERSION.SDK_INT);
        }

        ClassLoader cl = StaticJvmtiAgent.class.getClassLoader();
        if (!(cl instanceof BaseDexClassLoader)) {
            throw new IOException("Could not load jvmti plugin as StaticJvmtiAgent class was not loaded "
                    + "by a BaseDexClassLoader");
        }

        Debug.attachJvmtiAgent(AGENT_LIB_NAME, null, cl);
        nativeRegisterTransformerHook();
    }

    private native void nativeRegisterTransformerHook();

    private native void nativeUnregisterTransformerHook();

    @Override
    protected void finalize() throws Throwable {
        nativeUnregisterTransformerHook();
    }

    /**
     * Ask the agent to trigger transformation of some classes. This will extract the byte code of
     * the classes and the call back the {@link #addTransformer(StaticClassTransformer)
     * transformers} for each individual class.
     *
     * @param classes The classes to transform
     * @throws UnmodifiableClassException If one of the classes can not be transformed
     */
    void requestTransformClasses(Class<?>[] classes) throws UnmodifiableClassException {
        synchronized (lock) {
            try {
                nativeRetransformClasses(classes);
            } catch (RuntimeException e) {
                throw new UnmodifiableClassException(e);
            }
        }
    }

    // called by JNI
    @SuppressWarnings("unused")
    public boolean shouldTransform(Class<?> classBeingRedefined) {
        for (StaticClassTransformer transformer : transformers) {
            if (transformer.shouldTransform(classBeingRedefined)) {
                return true;
            }
        }

        return false;
    }

    /**
     * Register a transformer. These are called for each class when a transformation was triggered
     * via {@link #requestTransformClasses(Class[])}.
     *
     * @param transformer the transformer to add.
     */
    void addTransformer(StaticClassTransformer transformer) {
        transformers.add(transformer);
    }

    // called by JNI
    @SuppressWarnings("unused")
    public byte[] runTransformers(ClassLoader loader, String className,
                                  Class<?> classBeingRedefined, ProtectionDomain protectionDomain,
                                  byte[] classfileBuffer) throws IllegalClassFormatException {
        byte[] transformedByteCode = classfileBuffer;
        for (StaticClassTransformer transformer : transformers) {
            transformedByteCode = transformer.transform(classBeingRedefined, transformedByteCode);
        }

        return transformedByteCode;
    }

    private native void nativeRetransformClasses(Class<?>[] classes);
}
