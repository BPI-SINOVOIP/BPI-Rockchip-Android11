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

package util.build;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestResult;
import junit.textui.TestRunner;

public class JUnitTestCollector {

    public final int testClassCnt;
    public final int testMethodsCnt;

    /**
     * Map collection all found tests.
     *
     * using a linked hashmap to keep the insertion order for iterators.
     * the junit suite/tests adding order is used to generate the order of the
     * report.
     * a map. key: fully qualified class name, value: a list of test methods for
     * the given class
     */
    public final LinkedHashMap<String, List<String>> map =
            new LinkedHashMap<String, List<String>>();

    public JUnitTestCollector(ClassLoader loader) {
        Test test;
        try {
            Class<?> allTestsClass = loader.loadClass("dot.junit.AllTests");
            Method suiteMethod = allTestsClass.getDeclaredMethod("suite");
            test = (Test)suiteMethod.invoke(null);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        final Counters counters = new Counters();
        new TestRunner() {
            @Override
            protected TestResult createTestResult() {
                return new TestResult() {
                    @Override
                    protected void run(TestCase test) {
                        String packageName = test.getClass().getPackage().getName();
                        packageName = packageName.substring(packageName.lastIndexOf('.'));


                        String method = test.getName(); // e.g. testVFE2
                        String fqcn = test.getClass().getName(); // e.g.
                        // dxc.junit.opcodes.iload_3.Test_iload_3

                        counters.a++;
                        List<String> li = map.get(fqcn);
                        if (li == null) {
                            counters.b++;
                            li = new ArrayList<String>();
                            map.put(fqcn, li);
                        }
                        li.add(method);
                    }

                };
            }
        }.doRun(test);
        testMethodsCnt = counters.a;
        testClassCnt = counters.b;
    }

    private static class Counters {
        int a = 0;
        int b = 0;
    }
}
