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
package android.platform.test.option;

import android.os.Bundle;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * A base class whose implementations encompass options for a scenario test.
 *
 * <p>Each option is defined by calling its constructor with its argument string and then optionally
 * calling a number of setters to set the default value and whether it is required. They need to be
 * defined as JUnit test rules using the {@code Rule} annotation as this ensures that they are
 * initialized during test setup.
 *
 * <p>Using {@link StringOption} as an example, an option is defined as the following:
 *
 * <pre>
 * @Rule public StringOption sampleOption =
 *          new StringOption("sample-option").setRequired(true).setDefault("sample-value");
 * </pre>
 *
 * <p>To supply value for the above option, use {@code -e sample-option some_value} in the
 * instrumentation command.
 */
public abstract class TestOption<T> implements TestRule {
    private String mOptionName;
    private T mDefaultValue;
    private boolean mIsRequired;
    private T mValue;

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // An option with a default value is effectively not required.
                if (mDefaultValue != null) {
                    mIsRequired = false;
                }
                // Populate the option value before the test starts, and throw if the option is
                // missing when required or illegal.
                Bundle arguments = getArguments();
                if (!arguments.containsKey(mOptionName)) {
                    if (mIsRequired) {
                        throw new IllegalArgumentException(
                                String.format(
                                        "Missing argument for required option: %s", mOptionName));
                    }
                    mValue = mDefaultValue;
                } else {
                    // All `am instrument -e` arguments are provided as Strings.
                    try {
                        mValue = parseValueFromString(arguments.getString(mOptionName));
                    } catch (Exception e) {
                        throw new RuntimeException(
                                String.format(
                                        "Error parsing option %s for test %s.",
                                        TestOption.this, description),
                                e);
                    }
                }

                // Run the underlying statement which includes the test, if the above pass.
                base.evaluate();
            }
        };
    }

    /** Creates a required option with the provided name. */
    public TestOption(String optionName) {
        mOptionName = optionName;
        mIsRequired = true;
    }

    /**
     * Sets whether the option is required when no default is provided. Options are considered
     * required by default.
     */
    public <S extends TestOption<T>> S setRequired(boolean required) {
        mIsRequired = required;
        return (S) this;
    }

    /** Sets a default value for an option. A null value is not a proper default value. */
    public <S extends TestOption<T>> S setDefault(T defaultValue) {
        if (defaultValue == null) {
            throw new IllegalArgumentException(
                    String.format("Default value for option %s cannot be null.", this));
        }
        mDefaultValue = defaultValue;
        return (S) this;
    }

    /**
     * Returns the value of the option which had been initialized during test setup. Can return null
     * if the option is not set to required.
     */
    public T get() {
        if ((mValue == null) && mIsRequired) {
            // This should never happen.
            throw new IllegalStateException(
                    String.format(
                            "Value for required option %s had not been initialized during setup.",
                            mOptionName));
        }
        // A non-required option can return null.
        return mValue;
    }

    /**
     * Returns the set value based on registered arguments or the default value if not.
     *
     * <p>Note: this should override any defaults already provided by the {@link Bundle}.
     */
    protected abstract T parseValueFromString(String value);

    @VisibleForTesting
    Bundle getArguments() {
        return InstrumentationRegistry.getArguments();
    }

    public String toString() {
        return String.format("%s \"%s\"", this.getClass().getSimpleName(), mOptionName);
    }
}
