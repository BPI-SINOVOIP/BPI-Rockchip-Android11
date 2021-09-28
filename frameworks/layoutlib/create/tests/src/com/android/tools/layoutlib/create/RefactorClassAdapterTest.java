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

package com.android.tools.layoutlib.create;

import com.android.tools.layoutlib.create.dataclass.OuterClass;
import com.android.tools.layoutlib.create.dataclass.UsageClass;

import org.junit.Test;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;

import java.io.IOException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

public class RefactorClassAdapterTest {
    private static final String OUTER_CLASS_NAME = OuterClass.class.getName();
    private static final String USAGE_CLASS_NAME = UsageClass.class.getName();

    @Test
    public void testRefactorWithLambdas() throws Exception {
        Map<String, String> refactorMap = new HashMap<>();
        String originalClassName = OUTER_CLASS_NAME.replace('.', '/');
        String newClassName = originalClassName + "Refactored";
        refactorMap.put(originalClassName, newClassName);

        ClassWriter outerCw = new ClassWriter(0 /*flags*/);
        RefactorClassAdapter cv = new RefactorClassAdapter(outerCw, refactorMap);
        ClassReader cr = new ClassReader(OUTER_CLASS_NAME);
        cr.accept(cv, 0 /* flags */);

        ClassWriter usageCw = new ClassWriter(0 /*flags*/);
        cv = new RefactorClassAdapter(usageCw, refactorMap);
        cr = new ClassReader(USAGE_CLASS_NAME);
        cr.accept(cv, 0 /* flags */);

        ClassLoader2 cl2 = new ClassLoader2() {
            @Override
            public void testModifiedInstance() throws Exception {
                Class<?> clazz2 = loadClass(USAGE_CLASS_NAME);
                Object usage = clazz2.newInstance();
                assertNotNull(usage);
                assertEquals(17, callUsage(usage));
            }
        };
        cl2.add(OUTER_CLASS_NAME + "Refactored", outerCw);
        cl2.add(USAGE_CLASS_NAME, usageCw);
        cl2.testModifiedInstance();
    }

    //-------

    /**
     * A class loader than can define and instantiate our modified classes.
     * <p/>
     * Trying to do so in the original class loader generates all sort of link issues because
     * there are 2 different definitions of the same class name. This class loader will
     * define and load the class when requested by name and provide helpers to access the
     * instance methods via reflection.
     */
    private abstract static class ClassLoader2 extends ClassLoader {

        private final Map<String, byte[]> mClassDefs = new HashMap<>();

        public ClassLoader2() {
            super(null);
        }

        private void add(String className, ClassWriter rewrittenClass) {
            mClassDefs.put(className, rewrittenClass.toByteArray());
        }

        private Set<Entry<String, byte[]>> getByteCode() {
            return mClassDefs.entrySet();
        }

        @Override
        protected Class<?> findClass(String name) {
            try {
                return super.findClass(name);
            } catch (ClassNotFoundException e) {

                byte[] def = mClassDefs.get(name);
                if (def != null) {
                    // Load the modified class from its bytes representation.
                    return defineClass(name, def, 0, def.length);
                }

                try {
                    // Load everything else from the original definition into the new class loader.
                    ClassReader cr = new ClassReader(name);
                    ClassWriter cw = new ClassWriter(0);
                    cr.accept(cw, 0);
                    byte[] bytes = cw.toByteArray();
                    return defineClass(name, bytes, 0, bytes.length);

                } catch (IOException ioe) {
                    throw new RuntimeException(ioe);
                }
            }
        }

        /**
         * Accesses {@link UsageClass#doSomething} via reflection.
         */
        public int callUsage(Object instance) throws Exception {
            Method m = instance.getClass().getMethod("doSomething");

            Object result = m.invoke(instance);
            return (Integer) result;
        }

        public abstract void testModifiedInstance() throws Exception;
    }
}
