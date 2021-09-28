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
package com.android.cts.mockime;

import android.app.UiAutomation;
import android.content.Context;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.cts.mockime.ImeSettings.Builder;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * Custom JUnit4 rule to automatically open and close a {@link MockImeSession}.
 */
public final class MockImeSessionRule implements TestRule {

    private static final String TAG = MockImeSessionRule.class.getSimpleName();
    private final Context mContext;
    private final Builder mImeSettings;
    private final UiAutomation mUiAutomation;
    private MockImeSession mMockImeSession;

    public MockImeSessionRule() {
        this(InstrumentationRegistry.getInstrumentation().getTargetContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder());
    }

    public MockImeSessionRule(@NonNull Context context, @NonNull UiAutomation uiAutomation,
            @NonNull ImeSettings.Builder imeSettings) {
        mContext = context;
        mUiAutomation = uiAutomation;
        mImeSettings = imeSettings;
    }

    @Override
    public Statement apply(@NonNull Statement base, @NonNull Description description) {
        return new Statement() {

            @Override
            public void evaluate() throws Throwable {
                if (Log.isLoggable(TAG, Log.VERBOSE)) {
                    Log.v(TAG, "Creating MockImeSession on " + description.getDisplayName());
                }
                final String errorMsg = MockImeSession.getUnavailabilityReason(mContext);
                if (errorMsg != null) {
                    Log.w(TAG, "Mock IME not available: " + errorMsg);
                } else {
                    mMockImeSession = MockImeSession.create(mContext, mUiAutomation, mImeSettings);
                }
                try {
                    base.evaluate();
                } finally {
                    if (mMockImeSession != null) {
                        if (Log.isLoggable(TAG, Log.VERBOSE)) {
                            Log.v(TAG, "Closing MockImeSession on " + description.getDisplayName());
                        }
                        mMockImeSession.close();
                    }
                }
            }
        };

    }

    @Nullable
    public MockImeSession getMockImeSession() {
        return mMockImeSession;
    }
}
