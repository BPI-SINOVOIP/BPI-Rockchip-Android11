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

package android.appsecurity.cts;

import static org.hamcrest.CoreMatchers.both;
import static org.junit.Assert.assertThat;

import org.hamcrest.CoreMatchers;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Assert;

import java.util.function.Function;

/**
 * {@link Matcher} factories and {@link Assert#assertThat assertion} utilities.
 */
public class MatcherUtils {
    private MatcherUtils() {}

    /**
     * Creates a matcher based on whether object's given property property satisfies
     * the given matcher.
     *
     * This is often preferable over just retrieving the property directly and asserting on it, as
     * it ensures the enclosing object is included in the error message, providing better context.
     *
     * E.g.:
     * {@code
     *   Matcher<Intent> isExplicit =
     *           propertyMatches("component", Intent::getComponent, notNullValue());
     *   assertThat(myIntent, isExplicit);
     *   // ^ properly includes myIntent in error message, should match fail
     * }
     *
     * @param propName property name to be used in error message
     * @param propGetter retriever of the property value used for evaluation
     * @param propCondition condition a property is asserted to satisfy
     * @param <RECEIVER> type that has the given property
     * @param <PROP> type of the given property
     * @return a mather on {@code RECEIVER} type
     */
    public static <RECEIVER, PROP> Matcher<RECEIVER> propertyMatches(
            String propName,
            Function<? super RECEIVER, ? extends PROP> propGetter,
            Matcher<? extends PROP> propCondition) {
        return new TypeSafeMatcher<RECEIVER>() {
            @Override
            protected boolean matchesSafely(RECEIVER item) {
                return propCondition.matches(propGetter.apply(item));
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("has ")
                        .appendText(propName)
                        .appendText(" that ")
                        .appendDescriptionOf(propCondition);
            }
        };
    }

    /**
     * A more type-safe version of: {@link CoreMatchers#instanceOf} && {@code cond},
     * with {@code cond} being parametrized on the subtype being tested for as the first step.
     */
    public static <SUPER, SUB extends SUPER> Matcher<SUPER> instanceOf(
            Class<SUB> type, Matcher<? super SUB> cond) {
        return (Matcher<SUPER>) both(CoreMatchers.instanceOf(type)).and((Matcher) cond);
    }

    /**
     * {@link Throwable} matcher based on whether its {@link Throwable::getMessage message} is
     * matching {@code condition}.
     */
    public static Matcher<Throwable> hasMessageThat(Matcher<? super String> condition) {
        return propertyMatches("message", Throwable::getMessage, condition);
    }

    /**
     * Runs {@code action}, and asserts that it throws an exception matching {@code exceptionCond}.
     */
    public static void assertThrows(
            Matcher<? super Throwable> exceptionCond, ThrowingRunnable action) {
        Throwable thrown;
        try {
            action.run();
            thrown = null;
        } catch (Throwable t) {
            thrown = t;
        }
        assertOrRethrow(thrown, exceptionCond);
    }

    /**
     * Asserts that {@code exception} matches the given {@code condition}, or else
     * throws the resulting assertion error, with the original exception attached as a cause.
     */
    public static <E extends Throwable> void assertOrRethrow(
            E exception, Matcher<? super E> condition) throws AssertionError {
        try {
            assertThat(exception, condition);
        } catch (AssertionError err) {
            throw ExceptionUtils.appendCause(err, exception);
        }
    }
}
