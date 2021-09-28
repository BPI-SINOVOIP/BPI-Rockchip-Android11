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

package android.server.wm;

import static android.server.wm.StateLogger.logAlways;
import static android.server.wm.StateLogger.logE;

import android.os.SystemClock;

import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.function.Supplier;

/**
 * The utility class to wait a condition with customized options.
 * The default retry policy is 5 times with interval 1 second.
 *
 * @param <T> The type of the object to validate.
 *
 * <p>Sample:</p>
 * <pre>
 * // Simple case.
 * if (Condition.waitFor("true value", () -> true)) {
 *     println("Success");
 * }
 * // Wait for customized result with customized validation.
 * String result = Condition.waitForResult(new Condition<String>("string comparison")
 *         .setResultSupplier(() -> "Result string")
 *         .setResultValidator(str -> str.equals("Expected string"))
 *         .setRetryIntervalMs(500)
 *         .setRetryLimit(3)
 *         .setOnFailure(str -> println("Failed on " + str)));
 * </pre>
 */
public class Condition<T> {
    private final String mMessage;

    // TODO(b/112837428): Implement a incremental retry policy to reduce the unnecessary constant
    // time, currently keep the default as 5*1s because most of the original code uses it, and some
    // tests might be sensitive to the waiting interval.
    private long mRetryIntervalMs = TimeUnit.SECONDS.toMillis(1);
    private int mRetryLimit = 5;
    private boolean mReturnLastResult;

    /** It decides whether this condition is satisfied. */
    private BooleanSupplier mSatisfier;
    /**
     * It is used when the condition is not a simple boolean expression, such as the caller may want
     * to get the validated product as the return value.
     */
    private Supplier<T> mResultSupplier;
    /** It validates the result from {@link #mResultSupplier}. */
    private Predicate<T> mResultValidator;
    private Consumer<T> mOnFailure;
    private Runnable mOnRetry;
    private T mLastResult;
    private T mValidatedResult;

    /**
     * When using this constructor, it is expected that the condition will be configured with
     * {@link #setResultSupplier} and {@link #setResultValidator}.
     */
    public Condition(String message) {
        this(message, null /* satisfier */);
    }

    /**
     * Constructs with a simple boolean condition.
     *
     * @param message The message to show what is waiting for.
     * @param satisfier If it returns true, that means the condition is satisfied.
     */
    public Condition(String message, BooleanSupplier satisfier) {
        mMessage = message;
        mSatisfier = satisfier;
    }

    /** Set the supplier which provides the result object to validate. */
    public Condition<T> setResultSupplier(Supplier<T> supplier) {
        mResultSupplier = supplier;
        return this;
    }

    /** Set the validator which tests the object provided by the supplier. */
    public Condition<T> setResultValidator(Predicate<T> validator) {
        mResultValidator = validator;
        return this;
    }

    /**
     * If true, when using {@link #waitForResult(Condition)}, the method will return the last result
     * provided by {@link #mResultSupplier} even it is not valid (by {@link #mResultValidator}).
     */
    public Condition<T> setReturnLastResult(boolean returnLastResult) {
        mReturnLastResult = returnLastResult;
        return this;
    }

    /**
     * Executes the action when the condition does not satisfy within the time limit. The passed
     * object to the consumer will be the last result from the supplier.
     */
    public Condition<T> setOnFailure(Consumer<T> onFailure) {
        mOnFailure = onFailure;
        return this;
    }

    public Condition<T> setOnRetry(Runnable onRetry) {
        mOnRetry = onRetry;
        return this;
    }

    public Condition<T> setRetryIntervalMs(long millis) {
        mRetryIntervalMs = millis;
        return this;
    }

    public Condition<T> setRetryLimit(int limit) {
        mRetryLimit = limit;
        return this;
    }

    /** Build the condition by {@link #mResultSupplier} and {@link #mResultValidator}. */
    private void prepareSatisfier() {
        if (mResultSupplier == null || mResultValidator == null) {
            throw new IllegalArgumentException("No specified condition");
        }

        mSatisfier = () -> {
            final T result = mResultSupplier.get();
            mLastResult = result;
            if (mResultValidator.test(result)) {
                mValidatedResult = result;
                return true;
            }
            return false;
        };
    }

    /**
     * @see #waitFor(Condition)
     * @see #Condition(String, BooleanSupplier)
     */
    public static boolean waitFor(String message, BooleanSupplier satisfier) {
        return waitFor(new Condition<>(message, satisfier));
    }

    /** @return {@code false} if the condition does not satisfy within the time limit. */
    public static <T> boolean waitFor(Condition<T> condition) {
        if (condition.mSatisfier == null) {
            condition.prepareSatisfier();
        }

        final long startTime = SystemClock.elapsedRealtime();
        for (int i = 1; i <= condition.mRetryLimit; i++) {
            if (condition.mSatisfier.getAsBoolean()) {
                return true;
            } else {
                SystemClock.sleep(condition.mRetryIntervalMs);
                logAlways("***Waiting for " + condition.mMessage + " ... retry=" + i
                        + " elapsed=" + (SystemClock.elapsedRealtime() - startTime) + "ms");
                if (condition.mOnRetry != null && i < condition.mRetryLimit) {
                    condition.mOnRetry.run();
                }
            }
        }
        if (condition.mSatisfier.getAsBoolean()) {
            return true;
        }

        if (condition.mOnFailure == null) {
            logE("Condition is not satisfied: " + condition.mMessage);
        } else {
            condition.mOnFailure.accept(condition.mLastResult);
        }
        return false;
    }

    /** @see #waitForResult(Condition) */
    public static <T> T waitForResult(String message, Consumer<Condition<T>> setup) {
        final Condition<T> condition = new Condition<>(message);
        setup.accept(condition);
        return waitForResult(condition);
    }

    /**
     * @return {@code null} if the condition does not satisfy within the time limit or the result
     *         supplier returns {@code null}.
     */
    public static <T> T waitForResult(Condition<T> condition) {
        condition.mLastResult = condition.mValidatedResult = null;
        condition.prepareSatisfier();
        waitFor(condition);
        if (condition.mValidatedResult != null) {
            return condition.mValidatedResult;
        }
        return condition.mReturnLastResult ? condition.mLastResult : null;
    }
}
