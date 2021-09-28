/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

public class Main {
  // Workaround for b/18051191.
  class Inner {}

  public static native void assertIsInterpreted();
  public static native void ensureJitCompiled(Class<?> cls, String methodName);

  public static void assertEqual(String expected, String actual) {
    if (!expected.equals(actual)) {
      throw new Error("Assertion failed: " + expected + " != " + actual);
    }
  }

  public static void assertEqual(byte[] expected, String actual) throws Exception {
    String str = new String(expected, "UTF8");
    if (!str.equals(actual)) {
      throw new Error("Assertion failed: " + str + " != " + actual);
    }
  }

  public static void main(String[] args) throws Throwable {
    System.loadLibrary(args[0]);
    Class<?> c = Class.forName("TestCase");
    String testString = "Hello world";
    byte[] testData = testString.getBytes("UTF8");

    {
      Method m = c.getMethod("vregAliasing", byte[].class);
      String result = (String) m.invoke(null, new Object[] { testData });
      assertEqual(testString, result);
    }

    {
      c.getMethod("compareNewInstance").invoke(null, (Object[]) null);
    }

    {
      // If the JIT is enabled, ensure it has compiled the method to force the deopt.
      ensureJitCompiled(c, "deoptimizeNewInstance");
      Method m = c.getMethod("deoptimizeNewInstance", int[].class, byte[].class);
      try {
        m.invoke(null, new Object[] { new int[] { 1, 2, 3 }, testData });
      } catch (InvocationTargetException ex) {
        if (ex.getCause() instanceof ArrayIndexOutOfBoundsException) {
          // Expected.
        } else {
          throw ex.getCause();
        }
      }
    }

    {
      Method m = c.getMethod("removeNewInstance", byte[].class);
      String result = (String) m.invoke(null, new Object[] { testData });
      assertEqual(testString, result);
    }

    {
      Method m = c.getMethod("irreducibleLoopAndStringInit1", byte[].class, boolean.class);
      String result = (String) m.invoke(null, new Object[] { testData, true });
      assertEqual(testString, result);
      result = (String) m.invoke(null, new Object[] { testData, false });
      assertEqual(testString, result);
    }
    {
      Method m = c.getMethod("irreducibleLoopAndStringInit2", byte[].class, boolean.class);
      String result = (String) m.invoke(null, new Object[] { testData, true });
      assertEqual(testString, result);
      result = (String) m.invoke(null, new Object[] { testData, false });
      assertEqual(testString, result);
    }
    {
      Method m = c.getMethod("irreducibleLoopAndStringInit3", byte[].class, boolean.class);
      String result = (String) m.invoke(null, new Object[] { testData, true });
      assertEqual(testString, result);
      result = (String) m.invoke(null, new Object[] { testData, false });
      assertEqual(testString, result);
    }
    {
      Method m = c.getMethod("loopAndStringInit", byte[].class, boolean.class);
      String result = (String) m.invoke(null, new Object[] { testData, true });
      assertEqual(testString, result);
      result = (String) m.invoke(null, new Object[] { testData, false });
      assertEqual(testString, result);
    }
    {
      Method m = c.getMethod("loopAndStringInitAlias", byte[].class, boolean.class);
      String result = (String) m.invoke(null, new Object[] { testData, true });
      assertEqual(testString, result);
      result = (String) m.invoke(null, new Object[] { testData, false });
      assertEqual(testString, result);
    }
    {
      Method m = c.getMethod("loopAndStringInitAndTest", byte[].class, boolean.class);
      String result = (String) m.invoke(null, new Object[] { testData, true });
      assertEqual(testString, result);
      result = (String) m.invoke(null, new Object[] { testData, false });
      assertEqual(testString, result);
    }
    {
      // If the JIT is enabled, ensure it has compiled the method to force the deopt.
      ensureJitCompiled(c, "deoptimizeNewInstanceAfterLoop");
      Method m = c.getMethod(
          "deoptimizeNewInstanceAfterLoop", int[].class, byte[].class, int.class);
      try {
        m.invoke(null, new Object[] { new int[] { 1, 2, 3 }, testData, 0 });
      } catch (InvocationTargetException ex) {
        if (ex.getCause() instanceof ArrayIndexOutOfBoundsException) {
          // Expected.
        } else {
          throw ex.getCause();
        }
      }
    }
    {
      Method m = c.getMethod("loopAndStringInitAndPhi", byte[].class, boolean.class);
      String result = (String) m.invoke(null, new Object[] { testData, true });
      assertEqual(testString, result);
      result = (String) m.invoke(null, new Object[] { testData, false });
      assertEqual(testString, result);
    }
    {
      Method m =
          c.getMethod("loopAndTwoStringInitAndPhi", byte[].class, boolean.class, boolean.class);
      String result = (String) m.invoke(null, new Object[] { testData, false, false });
      assertEqual(testString, result);
      result = (String) m.invoke(null, new Object[] { testData, false, true });
      assertEqual(testString, result);
    }
    {
      Method m = c.getMethod("stringAndCatch", byte[].class, boolean.class);
      String result = (String) m.invoke(null, new Object[] { testData, false });
      assertEqual(testString, result);
    }
    {
      Method m = c.getMethod("stringAndCatch2", byte[].class, boolean.class);
      String result = (String) m.invoke(null, new Object[] { testData, false });
      assertEqual(testString, result);
    }
    {
      Method m = c.getMethod("stringAndCatch3", byte[].class, boolean.class);
      String result = (String) m.invoke(null, new Object[] { testData, false });
      assertEqual(testString, result);
    }
  }

  public static boolean doThrow = false;

  public static Object $noinline$HiddenNull() {
    if (doThrow) { throw new Error(); }
    return null;
  }
}
