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
package android.car.test.mocks;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doAnswer;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.mockitoSession;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.when;

import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Trace;
import android.os.UserManager;
import android.provider.Settings;
import android.util.Log;
import android.util.Slog;
import android.util.TimingsTraceLog;

import com.android.dx.mockito.inline.extended.StaticMockitoSessionBuilder;
import com.android.internal.util.Preconditions;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;
import org.mockito.MockitoSession;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.quality.Strictness;
import org.mockito.session.MockitoSessionBuilder;
import org.mockito.stubbing.Answer;

import java.lang.annotation.Retention;
import java.lang.annotation.Target;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

/**
 * Base class for tests that must use {@link com.android.dx.mockito.inline.extended.ExtendedMockito}
 * to mock static classes and final methods.
 *
 * <p><b>Note: </b> this class automatically spy on {@link Log} and {@link Slog} and fail tests that
 * all any of their {@code wtf()} methods. If a test is expect to call {@code wtf()}, it should be
 * annotated with {@link ExpectWtf}.
 *
 * <p><b>Note: </b>when using this class, you must include the following
 * dependencies on {@code Android.bp} (or {@code Android.mk}:
 * <pre><code>
    jni_libs: [
        "libdexmakerjvmtiagent",
        "libstaticjvmtiagent",
    ],

   LOCAL_JNI_SHARED_LIBRARIES := \
      libdexmakerjvmtiagent \
      libstaticjvmtiagent \
 *  </code></pre>
 */
public abstract class AbstractExtendedMockitoTestCase {

    private static final String TAG = AbstractExtendedMockitoTestCase.class.getSimpleName();

    private static final boolean TRACE = false;
    private static final boolean VERBOSE = false;

    private final List<Class<?>> mStaticSpiedClasses = new ArrayList<>();

    // Tracks (S)Log.wtf() calls made during code execution, then used on verifyWtfNeverLogged()
    private final List<RuntimeException> mWtfs = new ArrayList<>();

    private MockitoSession mSession;
    private MockSettings mSettings;

    @Nullable
    private final TimingsTraceLog mTracer;

    @Rule
    public final WtfCheckerRule mWtfCheckerRule = new WtfCheckerRule();

    protected AbstractExtendedMockitoTestCase() {
        mTracer = TRACE ? new TimingsTraceLog(TAG, Trace.TRACE_TAG_APP) : null;
    }

    @Before
    public final void startSession() {
        beginTrace("startSession()");

        beginTrace("startMocking()");
        mSession = newSessionBuilder().startMocking();
        endTrace();

        beginTrace("MockSettings()");
        mSettings = new MockSettings();
        endTrace();

        beginTrace("interceptWtfCalls()");
        interceptWtfCalls();
        endTrace();

        endTrace(); // startSession
    }

    @After
    public final void finishSession() {
        beginTrace("finishSession()");
        completeAllHandlerThreadTasks();
        if (mSession != null) {
            beginTrace("finishMocking()");
            mSession.finishMocking();
            endTrace();
        } else {
            Log.w(TAG, getClass().getSimpleName() + ".finishSession(): no session");
        }
        endTrace();
    }

    /**
     * Waits for completion of all pending Handler tasks for all HandlerThread in the process.
     *
     * <p>This can prevent pending Handler tasks of one test from affecting another. This does not
     * work if the message is posted with delay.
     */
    protected void completeAllHandlerThreadTasks() {
        beginTrace("completeAllHandlerThreadTasks");
        Set<Thread> threadSet = Thread.getAllStackTraces().keySet();
        ArrayList<HandlerThread> handlerThreads = new ArrayList<>(threadSet.size());
        Thread currentThread = Thread.currentThread();
        for (Thread t : threadSet) {
            if (t != currentThread && t instanceof HandlerThread) {
                handlerThreads.add((HandlerThread) t);
            }
        }
        ArrayList<SyncRunnable> syncs = new ArrayList<>(handlerThreads.size());
        Log.i(TAG, "will wait for " + handlerThreads.size() + " HandlerThreads");
        for (int i = 0; i < handlerThreads.size(); i++) {
            Handler handler = new Handler(handlerThreads.get(i).getLooper());
            SyncRunnable sr = new SyncRunnable(() -> { });
            handler.post(sr);
            syncs.add(sr);
        }
        beginTrace("waitForComplete");
        for (int i = 0; i < syncs.size(); i++) {
            syncs.get(i).waitForComplete();
        }
        endTrace(); // waitForComplete
        endTrace(); // completeAllHandlerThreadTasks
    }

    /**
     * Adds key-value(int) pair in mocked Settings.Global and Settings.Secure
     */
    protected void putSettingsInt(@NonNull String key, int value) {
        mSettings.insertObject(key, value);
    }

    /**
     * Gets value(int) from mocked Settings.Global and Settings.Secure
     */
    protected int getSettingsInt(@NonNull String key) {
        return mSettings.getInt(key);
    }

    /**
     * Adds key-value(String) pair in mocked Settings.Global and Settings.Secure
     */
    protected void putSettingsString(@NonNull String key, @NonNull String value) {
        mSettings.insertObject(key, value);
    }

    /**
     * Gets value(String) from mocked Settings.Global and Settings.Secure
     */
    protected String getSettingsString(@NonNull String key) {
        return mSettings.getString(key);
    }

    /**
     * Asserts that the giving settings was not set.
     */
    protected void assertSettingsNotSet(String key) {
        mSettings.assertDoesNotContainsKey(key);
    }

    /**
     * Subclasses can use this method to initialize the Mockito session that's started before every
     * test on {@link #startSession()}.
     *
     * <p>Typically, it should be overridden when mocking static methods.
     */
    protected void onSessionBuilder(@NonNull CustomMockitoSessionBuilder session) {
        if (VERBOSE) Log.v(TAG, getLogPrefix() + "onSessionBuilder()");
    }

    /**
     * Changes the value of the session created by
     * {@link #onSessionBuilder(CustomMockitoSessionBuilder)}.
     *
     * <p>By default it's set to {@link Strictness.LENIENT}, but subclasses can overwrite this
     * method to change the behavior.
     */
    @NonNull
    protected Strictness getSessionStrictness() {
        return Strictness.LENIENT;
    }

    /**
     * Mocks a call to {@link ActivityManager#getCurrentUser()}.
     *
     * @param userId result of such call
     *
     * @throws IllegalStateException if class didn't override {@link #newSessionBuilder()} and
     * called {@code spyStatic(ActivityManager.class)} on the session passed to it.
     */
    protected final void mockGetCurrentUser(@UserIdInt int userId) {
        if (VERBOSE) Log.v(TAG, getLogPrefix() + "mockGetCurrentUser(" + userId + ")");
        assertSpied(ActivityManager.class);

        beginTrace("mockAmGetCurrentUser-" + userId);
        AndroidMockitoHelper.mockAmGetCurrentUser(userId);
        endTrace();
    }

    /**
     * Mocks a call to {@link UserManager#isHeadlessSystemUserMode()}.
     *
     * @param mode result of such call
     *
     * @throws IllegalStateException if class didn't override {@link #newSessionBuilder()} and
     * called {@code spyStatic(UserManager.class)} on the session passed to it.
     */
    protected final void mockIsHeadlessSystemUserMode(boolean mode) {
        if (VERBOSE) Log.v(TAG, getLogPrefix() + "mockIsHeadlessSystemUserMode(" + mode + ")");
        assertSpied(UserManager.class);

        beginTrace("mockUmIsHeadlessSystemUserMode");
        AndroidMockitoHelper.mockUmIsHeadlessSystemUserMode(mode);
        endTrace();
    }

    /**
     * Starts a tracing message.
     *
     * <p>MUST be followed by a {@link #endTrace()} calls.
     *
     * <p>Ignored if {@value #VERBOSE} is {@code false}.
     */
    protected final void beginTrace(@NonNull String message) {
        if (mTracer == null) return;

        Log.d(TAG, getLogPrefix() + message);
        mTracer.traceBegin(message);
    }

    /**
     * Ends a tracing call.
     *
     * <p>MUST be called after {@link #beginTrace(String)}.
     *
     * <p>Ignored if {@value #VERBOSE} is {@code false}.
     */
    protected final void endTrace() {
        if (mTracer == null) return;

        mTracer.traceEnd();
    }

    private void interceptWtfCalls() {
        doAnswer((invocation) -> {
            return addWtf(invocation);
        }).when(() -> Log.wtf(anyString(), anyString()));
        doAnswer((invocation) -> {
            return addWtf(invocation);
        }).when(() -> Log.wtf(anyString(), anyString(), notNull()));
        doAnswer((invocation) -> {
            return addWtf(invocation);
        }).when(() -> Slog.wtf(anyString(), anyString()));
        doAnswer((invocation) -> {
            return addWtf(invocation);
        }).when(() -> Slog.wtf(anyString(), anyString(), notNull()));
    }

    private Object addWtf(InvocationOnMock invocation) {
        String message = "Called " + invocation;
        Log.d(TAG, message); // Log always, as some test expect it
        mWtfs.add(new IllegalStateException(message));
        return null;
    }

    private void verifyWtfLogged() {
        Preconditions.checkState(!mWtfs.isEmpty(), "no wtf() called");
    }

    private void verifyWtfNeverLogged() {
        int size = mWtfs.size();

        switch (size) {
            case 0:
                return;
            case 1:
                throw mWtfs.get(0);
            default:
                StringBuilder msg = new StringBuilder("wtf called ").append(size).append(" times")
                        .append(": ").append(mWtfs);
                throw new AssertionError(msg.toString());
        }
    }

    @NonNull
    private MockitoSessionBuilder newSessionBuilder() {
        // TODO (b/155523104): change from mock to spy
        StaticMockitoSessionBuilder builder = mockitoSession()
                .strictness(getSessionStrictness())
                .mockStatic(Settings.Global.class)
                .mockStatic(Settings.System.class)
                .mockStatic(Settings.Secure.class);

        CustomMockitoSessionBuilder customBuilder =
                new CustomMockitoSessionBuilder(builder, mStaticSpiedClasses)
                    .spyStatic(Log.class)
                    .spyStatic(Slog.class);

        onSessionBuilder(customBuilder);

        if (VERBOSE) Log.v(TAG, "spied classes" + customBuilder.mStaticSpiedClasses);

        return builder.initMocks(this);
    }

    /**
     * Gets a prefix for {@link Log} calls
     */
    protected String getLogPrefix() {
        return getClass().getSimpleName() + ".";
    }

    /**
     * Asserts the given class is being spied in the Mockito session.
     */
    protected void assertSpied(Class<?> clazz) {
        Preconditions.checkArgument(mStaticSpiedClasses.contains(clazz),
                "did not call spyStatic() on %s", clazz.getName());
    }

    /**
     * Custom {@code MockitoSessionBuilder} used to make sure some pre-defined mock stations
     * (like {@link AbstractExtendedMockitoTestCase#mockGetCurrentUser(int)} fail if the test case
     * didn't explicitly set it to spy / mock the required classes.
     *
     * <p><b>NOTE: </b>for now it only provides simple {@link #spyStatic(Class)}, but more methods
     * (as provided by {@link StaticMockitoSessionBuilder}) could be provided as needed.
     */
    public static final class CustomMockitoSessionBuilder {
        private final StaticMockitoSessionBuilder mBuilder;
        private final List<Class<?>> mStaticSpiedClasses;

        private CustomMockitoSessionBuilder(StaticMockitoSessionBuilder builder,
                List<Class<?>> staticSpiedClasses) {
            mBuilder = builder;
            mStaticSpiedClasses = staticSpiedClasses;
        }

        /**
         * Same as {@link StaticMockitoSessionBuilder#spyStatic(Class)}.
         */
        public <T> CustomMockitoSessionBuilder spyStatic(Class<T> clazz) {
            Preconditions.checkState(!mStaticSpiedClasses.contains(clazz),
                    "already called spyStatic() on " + clazz);
            mStaticSpiedClasses.add(clazz);
            mBuilder.spyStatic(clazz);
            return this;
        }
    }

    private final class WtfCheckerRule implements TestRule {

        @Override
        public Statement apply(Statement base, Description description) {
            return new Statement() {
                @Override
                public void evaluate() throws Throwable {
                    String testName = description.getMethodName();
                    if (VERBOSE) Log.v(TAG, "running " + testName);
                    beginTrace("evaluate-" + testName);
                    base.evaluate();
                    endTrace();

                    Method testMethod = AbstractExtendedMockitoTestCase.this.getClass()
                            .getMethod(testName);
                    ExpectWtf expectWtfAnnotation = testMethod.getAnnotation(ExpectWtf.class);

                    beginTrace("verify-wtfs");
                    try {
                        if (expectWtfAnnotation != null) {
                            if (VERBOSE) Log.v(TAG, "expecting wtf()");
                            verifyWtfLogged();
                        } else {
                            if (VERBOSE) Log.v(TAG, "NOT expecting wtf()");
                            verifyWtfNeverLogged();
                        }
                    } finally {
                        endTrace();
                    }
                }
            };
        }
    }

    // TODO (b/155523104): Add log
    // TODO (b/156033195): Clean settings API
    private static final class MockSettings {
        private static final int INVALID_DEFAULT_INDEX = -1;
        private HashMap<String, Object> mSettingsMapping = new HashMap<>();

        MockSettings() {

            Answer<Object> insertObjectAnswer =
                    invocation -> insertObjectFromInvocation(invocation, 1, 2);
            Answer<Integer> getIntAnswer = invocation ->
                    getAnswer(invocation, Integer.class, 1, 2);
            Answer<String> getStringAnswer = invocation ->
                    getAnswer(invocation, String.class, 1, INVALID_DEFAULT_INDEX);

            when(Settings.Global.putInt(any(), any(), anyInt())).thenAnswer(insertObjectAnswer);

            when(Settings.Global.getInt(any(), any(), anyInt())).thenAnswer(getIntAnswer);

            when(Settings.Secure.putIntForUser(any(), any(), anyInt(), anyInt()))
                    .thenAnswer(insertObjectAnswer);

            when(Settings.Secure.getIntForUser(any(), any(), anyInt(), anyInt()))
                    .thenAnswer(getIntAnswer);

            when(Settings.Secure.putStringForUser(any(), anyString(), anyString(), anyInt()))
                    .thenAnswer(insertObjectAnswer);

            when(Settings.Global.putString(any(), any(), any()))
                    .thenAnswer(insertObjectAnswer);

            when(Settings.Global.getString(any(), any())).thenAnswer(getStringAnswer);

            when(Settings.System.putIntForUser(any(), any(), anyInt(), anyInt()))
                    .thenAnswer(insertObjectAnswer);

            when(Settings.System.getIntForUser(any(), any(), anyInt(), anyInt()))
                    .thenAnswer(getIntAnswer);

            when(Settings.System.putStringForUser(any(), any(), anyString(), anyInt()))
                    .thenAnswer(insertObjectAnswer);
        }

        private Object insertObjectFromInvocation(InvocationOnMock invocation,
                int keyIndex, int valueIndex) {
            String key = (String) invocation.getArguments()[keyIndex];
            Object value = invocation.getArguments()[valueIndex];
            insertObject(key, value);
            return null;
        }

        private void insertObject(String key, Object value) {
            if (VERBOSE) Log.v(TAG, "Inserting Setting " + key + ": " + value);
            mSettingsMapping.put(key, value);
        }

        private <T> T getAnswer(InvocationOnMock invocation, Class<T> clazz,
                int keyIndex, int defaultValueIndex) {
            String key = (String) invocation.getArguments()[keyIndex];
            T defaultValue = null;
            if (defaultValueIndex > INVALID_DEFAULT_INDEX) {
                defaultValue = safeCast(invocation.getArguments()[defaultValueIndex], clazz);
            }
            return get(key, defaultValue, clazz);
        }

        @Nullable
        private <T> T get(String key, T defaultValue, Class<T> clazz) {
            if (VERBOSE) {
                Log.v(TAG, "get(): key=" + key + ", default=" + defaultValue + ", class=" + clazz);
            }
            Object value = mSettingsMapping.get(key);
            if (value == null) {
                if (VERBOSE) Log.v(TAG, "not found");
                return defaultValue;
            }

            if (VERBOSE) Log.v(TAG, "returning " + value);
            return safeCast(value, clazz);
        }

        private static <T> T safeCast(Object value, Class<T> clazz) {
            if (value == null) {
                return null;
            }
            Preconditions.checkArgument(value.getClass() == clazz,
                    "Setting value has class %s but requires class %s",
                    value.getClass(), clazz);
            return clazz.cast(value);
        }

        private String getString(String key) {
            return get(key, null, String.class);
        }

        public int getInt(String key) {
            return get(key, null, Integer.class);
        }

        public void assertDoesNotContainsKey(String key) {
            if (mSettingsMapping.containsKey(key)) {
                throw new AssertionError("Should not have key " + key + ", but has: "
                        + mSettingsMapping.get(key));
            }
        }
    }

    /**
     * Annotation used on test methods that are expect to call {@code wtf()} methods on {@link Log}
     * or {@link Slog} - if such methods are not annotated with this annotation, they will fail.
     */
    @Retention(RUNTIME)
    @Target({METHOD})
    public static @interface ExpectWtf {
    }

    private static final class SyncRunnable implements Runnable {
        private final Runnable mTarget;
        private volatile boolean mComplete = false;

        private SyncRunnable(Runnable target) {
            mTarget = target;
        }

        @Override
        public void run() {
            mTarget.run();
            synchronized (this) {
                mComplete = true;
                notifyAll();
            }
        }

        private void waitForComplete() {
            synchronized (this) {
                while (!mComplete) {
                    try {
                        wait();
                    } catch (InterruptedException e) {
                    }
                }
            }
        }
    }
}
