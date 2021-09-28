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
 * limitations under the License
 */

package android.accessibility.cts.common;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import android.util.Log;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * A JUnit rule that provides a simplified mechanism to enable and disable {@link
 * InstrumentedAccessibilityService} before and after the duration of your test. It will
 * automatically be disabled after the test completes and any methods annotated with
 * <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html"><code>After</code></a>
 * are finished.
 *
 * <p>Usage:
 *
 * <pre>
 * &#064;Rule
 * public final InstrumentedAccessibilityServiceTestRule<InstrumentedAccessibilityService>
 *         mServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
 *                 InstrumentedAccessibilityService.class, false);
 *
 * &#064;Test
 * public void testWithEnabledAccessibilityService() {
 *     MyService service = mServiceRule.enableService();
 *     //do something
 *     assertTrue("True wasn't returned", service.doSomethingToReturnTrue());
 * }
 * </pre>
 *
 * @param <T> The instrumented accessibility service class under test
 */
public class InstrumentedAccessibilityServiceTestRule<T extends InstrumentedAccessibilityService>
        implements TestRule {

    private static final String TAG = "InstrA11yServiceTestRule";

    private final Class<T> mAccessibilityServiceClass;

    private final boolean mEnableService;

    /**
     * Creates a {@link InstrumentedAccessibilityServiceTestRule} with the specified class of
     * instrumented accessibility service and enable the service automatically.
     *
     * @param clazz The instrumented accessibility service under test. This must be a class in the
     *     instrumentation targetPackage specified in the AndroidManifest.xml
     */
    public InstrumentedAccessibilityServiceTestRule(@NonNull Class<T> clazz) {
        this(clazz, true);
    }

    /**
     * Creates a {@link InstrumentedAccessibilityServiceTestRule} with the specified class of
     * instrumented accessibility service, and enable the service automatically or not according to
     * given {@code enableService}.
     *
     * @param clazz The instrumented accessibility service under test. This must be a class in the
     *     instrumentation targetPackage specified in the AndroidManifest.xml
     * @param enableService true if the service should be enabled once per <a
     *     href="http://junit.org/javadoc/latest/org/junit/Test.html"><code>Test</code></a> method.
     *     It will be enabled before the first <a
     *     href="http://junit.sourceforge.net/javadoc/org/junit/Before.html"><code>Before</code></a>
     *     method, and terminated after the last <a
     *     href="http://junit.sourceforge.net/javadoc/org/junit/After.html"><code>After</code></a>
     *     method.
     */
    public InstrumentedAccessibilityServiceTestRule(@NonNull Class<T> clazz,
            boolean enableService) {
        mAccessibilityServiceClass = clazz;
        mEnableService = enableService;
    }

    /**
     * Enable the instrumented accessibility service under test.
     *
     * <p>Don't call this method directly, unless you explicitly requested not to lazily enable the
     * service manually using the enableService flag in {@link
     * #InstrumentedAccessibilityServiceTestRule(Class, boolean)}.
     *
     * <p>Usage:
     *
     * <pre>
     *    &#064;Test
     *    public void enableAccessibilityService() {
     *        service = mServiceRule.enableService();
     *    }
     * </pre>
     *
     * @return the instrumented accessibility service enabled by this rule.
     */
    @NonNull
    public T enableService() {
        return InstrumentedAccessibilityService.enableService(mAccessibilityServiceClass);
    }

    /**
     * Override this method to do your own service specific clean up or shutdown.
     * The method is called after each test method is executed including any method annotated with
     * <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html"><code>After</code></a>
     * and after necessary calls to stop (or unbind) the service under test were called.
     */
    protected void disableService() {
        callFinishOnServiceSync();
    }

    /**
     * Returns the reference to the instrumented accessibility service instance.
     *
     * <p>If the service wasn't enabled yet or already disabled, {@code null} will be returned.
     */
    @Nullable
    public T getService() {
        final T instance = InstrumentedAccessibilityService.getInstanceForClass(
                mAccessibilityServiceClass,
                InstrumentedAccessibilityService.TIMEOUT_SERVICE_ENABLE);
        if (instance == null) {
            Log.i(TAG, String.format(
                    "Accessibility service %s wasn't enabled yet or already disabled",
                    mAccessibilityServiceClass.getSimpleName()));
        }
        return instance;
    }

    private void callFinishOnServiceSync() {
        final T service = InstrumentedAccessibilityService.getInstanceForClass(
                mAccessibilityServiceClass);
        if (service != null) {
            service.runOnServiceSync(service::disableSelfAndRemove);
        }
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new ServiceStatement(base);
    }

    /**
     * {@link Statement} that executes the service lifecycle methods before and after the execution
     * of the test.
     */
    private class ServiceStatement extends Statement {
        private final Statement base;

        public ServiceStatement(Statement base) {
            this.base = base;
        }

        @Override
        public void evaluate() throws Throwable {
            try {
                if (mEnableService) {
                    enableService();
                }
                base.evaluate();
            } finally {
                disableService();
            }
        }
    }
}
