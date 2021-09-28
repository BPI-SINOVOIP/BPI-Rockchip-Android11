/*
 * Copyright (C) 2017 The Android Open Source Project
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

import java.lang.invoke.CallSite;
import java.lang.invoke.MethodType;
import java.lang.invoke.MutableCallSite;

public class Main extends TestBase {

    private static void TestUninitializedCallSite() throws Throwable {
        CallSite callSite = new MutableCallSite(MethodType.methodType(int.class));
        try {
            callSite.getTarget().invoke();
            fail();
        } catch (IllegalStateException e) {
            System.out.println("Caught exception from uninitialized call site");
        }

        callSite = new MutableCallSite(MethodType.methodType(String.class, int.class, char.class));
        try {
            callSite.getTarget().invoke(1535, 'd');
            fail();
        } catch (IllegalStateException e) {
            System.out.println("Caught exception from uninitialized call site");
        }
    }

    private static void TestLinkerMethodMultipleArgumentTypes() throws Throwable {
        TestLinkerMethodMultipleArgumentTypes.test(33, 67);
        TestLinkerMethodMultipleArgumentTypes.test(-10000, 1000);
        TestLinkerMethodMultipleArgumentTypes.test(-1000, 10000);
    }

    private static void TestLinkerMethodWithRange() throws Throwable {
        TestLinkerMethodWithRange.test(0, 1, 2, 3, 4, 5);
        TestLinkerMethodWithRange.test(-101, -79, 113, 9, 17, 229);
        TestLinkerMethodWithRange.test(811, 823, 947, 967, 1087, 1093);

        TestLinkerMethodWithRange.test(null, null, null, null, null, null);
        TestLinkerMethodWithRange.test(Double.valueOf(1.0), null, Double.valueOf(3.0), null,
                                       Double.valueOf(37.0), null);
        TestLinkerMethodWithRange.test(null, Double.valueOf(3.0), null,
                                       Double.valueOf(37.0), null, Double.valueOf(113.0));
        TestLinkerMethodWithRange.test(Double.valueOf(1.0), Double.valueOf(2.0),
                                       Double.valueOf(3.0), Double.valueOf(5.0),
                                       Double.valueOf(7.0), Double.valueOf(11.0));
    }

    private static void TestLinkerMethodMinimalArguments() throws Throwable {
        try {
            TestLinkerMethodMinimalArguments.test(
                    TestLinkerMethodMinimalArguments.FAILURE_TYPE_LINKER_METHOD_RETURNS_NULL,
                    10,
                    10);
            assertNotReached();
        } catch (BootstrapMethodError e) {
            assertEquals(e.getCause().getClass(), ClassCastException.class);
        }

        try {
            TestLinkerMethodMinimalArguments.test(
                    TestLinkerMethodMinimalArguments.FAILURE_TYPE_LINKER_METHOD_THROWS, 10, 11);
            assertNotReached();
        } catch (BootstrapMethodError e) {
            assertEquals(e.getCause().getClass(), InstantiationException.class);
        }

        try {
            TestLinkerMethodMinimalArguments.test(
                    TestLinkerMethodMinimalArguments.FAILURE_TYPE_TARGET_METHOD_THROWS, 10, 12);
            assertNotReached();
        } catch (ArithmeticException e) {
        }

        TestLinkerMethodMinimalArguments.test(
                TestLinkerMethodMinimalArguments.FAILURE_TYPE_NONE, 10, 13);
    }

    public static void main(String[] args) throws Throwable {
        TestUninitializedCallSite();
        TestLinkerMethodMinimalArguments();
        TestLinkerMethodMultipleArgumentTypes();
        TestLinkerMethodWithRange();
        TestLinkerUnrelatedBSM.test();
        TestInvokeCustomWithConcurrentThreads.test();
        TestInvocationKinds.test();
        TestDynamicBootstrapArguments.test();
        TestBadBootstrapArguments.test();
        TestVariableArityLinkerMethod.test();
    }
}
