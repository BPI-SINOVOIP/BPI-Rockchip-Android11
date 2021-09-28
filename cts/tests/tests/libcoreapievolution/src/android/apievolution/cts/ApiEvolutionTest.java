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

package android.apievolution.cts;

import android.telephony.CellIdentity;
import android.telephony.CellIdentityNr;
import android.telephony.CellInfoNr;
import android.telephony.CellSignalStrength;
import android.telephony.CellSignalStrengthNr;

import org.junit.Test;

import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.DoubleBuffer;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.LongBuffer;
import java.nio.ShortBuffer;
import java.text.ParseException;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

/**
 * A test to ensure that platform bytecode is as expected to enable API evolution.
 */
public class ApiEvolutionTest {

    /**
     * Tests for the presence of a synthetic overload for a subclass method override
     * that has a more specific (sub-) return type, but doesn't carry any annotation.
     */
    @Test
    public void testCovariantReturnTypeMethods_specializingSubclass() throws Exception {
        // Exceptions are not required to be identical in this case because the synthetic method
        // mirrors the superclass version.
        assertSyntheticMethodOverloadExists(
                Sub.class, "myMethod", new Class[] { Integer.class },
                String.class, Object.class,
                false /* requireIdenticalExceptions */);
    }

    /**
     * Tests for the presence of a synthetic overload for {@link ConcurrentHashMap#keySet}
     * that must be introduced by the platform build tools in response to the presence of a
     * {@link dalvik.annotation.codegen.CovariantReturnType} annotation. http://b/28099367
     */
    @Test
    public void testCovariantReturnTypeMethods_annotation_concurrentHashMap() throws Exception {
        assertSyntheticMethodOverloadExists(ConcurrentHashMap.class, "keySet", null, Set.class,
                ConcurrentHashMap.KeySetView.class, true /* requireIdenticalExceptions */);
    }

    @Test public void testCovariantReturnTypeMethods_annotation_byteBuffer() throws Exception {
        assertSyntheticBufferMethodOverloadsExists(ByteBuffer.class);
    }

    @Test public void testCovariantReturnTypeMethods_annotation_charBuffer() throws Exception {
        assertSyntheticBufferMethodOverloadsExists(CharBuffer.class);
    }

    @Test public void testCovariantReturnTypeMethods_annotation_doubleBuffer() throws Exception {
        assertSyntheticBufferMethodOverloadsExists(DoubleBuffer.class);
    }

    @Test public void testCovariantReturnTypeMethods_annotation_floatBuffer() throws Exception {
        assertSyntheticBufferMethodOverloadsExists(FloatBuffer.class);
    }

    @Test public void testCovariantReturnTypeMethods_annotation_intBuffer() throws Exception {
        assertSyntheticBufferMethodOverloadsExists(IntBuffer.class);
    }

    @Test public void testCovariantReturnTypeMethods_annotation_longBuffer() throws Exception {
        assertSyntheticBufferMethodOverloadsExists(LongBuffer.class);
    }

    @Test public void testCovariantReturnTypeMethods_annotation_shortBuffer() throws Exception {
        assertSyntheticBufferMethodOverloadsExists(ShortBuffer.class);
    }

    /**
     * Ensures that {@link CellInfoNr#getCellIdentity()} returns a {@link CellIdentityNr}.
     * This is not a libcore/ API but testing it here avoids duplicating test support code.
     */
    @Test public void testCellIdentityNr_Override() throws Exception {
        assertSyntheticMethodOverloadExists(CellInfoNr.class, "getCellIdentity", null,
            CellIdentity.class, CellIdentityNr.class, true /* requireIdenticalExceptions */);
    }

    /**
     * Ensures that {@link CellInfoNr#getCellSignalStrength()} returns a {@link
     * CellSignalStrengthNr}. This is not a libcore/ API but testing it here avoids duplicating
     * test support code.
     */
    @Test public void testCellCellSignalStrength_Override() throws Exception {
        assertSyntheticMethodOverloadExists(CellInfoNr.class, "getCellSignalStrength", null,
            CellSignalStrength.class, CellSignalStrengthNr.class,
            true /* requireIdenticalExceptions */);
    }

    /**
     * Asserts the presence of synthetic methods overloads for methods that return {@code this} on
     * {@link Buffer} subclasses, and which are annotated with {@code @CovariantReturnType}.
     * In OpenJDK 9 the return types were changed from {@link Buffer} to be the subclass's type
     * instead. http://b/71597787
     */
    private static void assertSyntheticBufferMethodOverloadsExists(Class<? extends Buffer> c)
            throws Exception {
        assertSyntheticBufferMethodOverloadExists(c, "position", new Class[] { Integer.TYPE });
        assertSyntheticBufferMethodOverloadExists(c, "limit", new Class[] { Integer.TYPE });
        assertSyntheticBufferMethodOverloadExists(c, "mark", null);
        assertSyntheticBufferMethodOverloadExists(c, "reset", null);
        assertSyntheticBufferMethodOverloadExists(c, "clear", null);
        assertSyntheticBufferMethodOverloadExists(c, "flip", null);
        assertSyntheticBufferMethodOverloadExists(c, "rewind", null);
    }

    private static void assertSyntheticBufferMethodOverloadExists(
            Class<? extends Buffer> bufferClass, String methodName, Class[] parameterTypes)
            throws Exception {
        assertSyntheticMethodOverloadExists(bufferClass, methodName, parameterTypes,
                Buffer.class /* originalReturnType */,
                bufferClass /* syntheticReturnType */,
                true  /* requireIdenticalExceptions */);
    }

    private static void assertSyntheticMethodOverloadExists(
            Class<?> clazz, String methodName, Class[] parameterTypes,
            Class<?> originalReturnType, Class<?> syntheticReturnType,
            boolean requireIdenticalExceptions) throws Exception {

        if (parameterTypes == null) {
            parameterTypes = new Class[0];
        }
        String fullMethodName = clazz + "." + methodName;

        // Assert we find the original, non-synthetic version using getDeclaredMethod().
        Method declaredMethod = clazz.getDeclaredMethod(methodName, parameterTypes);
        assertEquals(originalReturnType, declaredMethod.getReturnType());

        // Assert both versions of the method are returned from getDeclaredMethods().
        Method original = null;
        Method synthetic = null;
        for (Method method : clazz.getDeclaredMethods()) {
            if (methodMatches(methodName, parameterTypes, method)) {
                if (method.getReturnType().equals(syntheticReturnType)) {
                    synthetic = method;
                } else if (method.getReturnType().equals(originalReturnType)) {
                    original = method;
                }
            }
        }
        assertNotNull("Unable to find original signature: " + fullMethodName
                + ", returning " + originalReturnType, original);
        assertNotNull("Unable to find synthetic signature: " + fullMethodName
                + ", returning " + syntheticReturnType, synthetic);

        // Check modifiers are as expected.
        assertFalse(original.isSynthetic());
        assertFalse(original.isBridge());
        assertTrue(synthetic.isSynthetic());
        assertTrue(synthetic.isBridge());

        int originalModifiers = original.getModifiers();
        int syntheticModifiers = synthetic.getModifiers();

        // These masks aren't in the public API but are defined in the dex spec.
        int syntheticMask = 0x00001000;
        int bridgeMask = 0x00000040;
        int mask = syntheticMask | bridgeMask;
        assertEquals("Method modifiers for " + fullMethodName
                        + " are expected to be identical except for SYNTHETIC and BRIDGE."
                        + " original=" + Modifier.toString(originalModifiers)
                        + ", synthetic=" + Modifier.toString(syntheticModifiers),
                originalModifiers | mask,
                syntheticModifiers | mask);

        // Exceptions are not required at method resolution time but we check they're the same in
        // most cases for completeness.
        if (requireIdenticalExceptions) {
            assertArrayEquals("Exceptions for " + fullMethodName + " must be compatible",
                    original.getExceptionTypes(), synthetic.getExceptionTypes());
        }

        // Android doesn't support runtime type annotations so nothing to do for them.

        // Type parameters are *not* copied because they're not needed at method resolution time.
        assertEquals(0, synthetic.getTypeParameters().length);

        // Check method annotations.
        Annotation[] annotations = original.getDeclaredAnnotations();
        assertArrayEquals("Annotations differ between original and synthetic versions of "
                + fullMethodName, annotations, synthetic.getDeclaredAnnotations());
        Annotation[][] parameterAnnotations = original.getParameterAnnotations();
        // Check parameter annotations.
        assertArrayEquals("Annotations differ between original and synthetic versions of "
                + fullMethodName, parameterAnnotations, synthetic.getParameterAnnotations());
    }

    private static boolean methodMatches(String methodName, Class[] parameterTypes, Method method) {
        return method.getName().equals(methodName)
                && Arrays.equals(parameterTypes, method.getParameterTypes());
    }

    /** Annotation used in return type specialization tests. */
    @Retention(RetentionPolicy.RUNTIME)
    private @interface TestAnnotation {}

    /** Base class for return type specialization tests. */
    private static class Base {
        protected Object myMethod(Integer p1) throws Exception {
            return null;
        }
    }

    /** Sub class for return type specialization tests. */
    private static class Sub extends Base {
        @TestAnnotation
        @Override
        protected String myMethod(@TestAnnotation Integer p1) throws ParseException {
            return null;
        }
    }
}
