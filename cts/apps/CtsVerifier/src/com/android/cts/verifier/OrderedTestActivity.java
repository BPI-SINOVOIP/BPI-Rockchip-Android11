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
package com.android.cts.verifier;

import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

/**
 * {@link PassFailButtons.Activity} that supports showing a series of tests in order.
 */
public abstract class OrderedTestActivity extends PassFailButtons.Activity {
    private static final String KEY_CURRENT_TEST = "current_test";

    private Test[] mTests;
    private int mTestIndex;

    private Button mNextButton;
    private TextView mInstructions;

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.ordered_test);
        setPassFailButtonClickListeners();

        mTests = getTests();

        mInstructions = findViewById(R.id.txt_instruction);

        mNextButton = findViewById(R.id.btn_next);
        mNextButton.setOnClickListener(v -> {
            if ((mTestIndex >= 0) && (mTestIndex < mTests.length)) {
                mTests[mTestIndex].onNextClick();
            }
        });

        // Don't allow pass until all tests complete.
        findViewById(R.id.pass_button).setVisibility(View.GONE);

        // Figure out if we are in a test or starting from the beginning.
        if (savedInstanceState != null && savedInstanceState.containsKey(KEY_CURRENT_TEST)) {
            mTestIndex = savedInstanceState.getInt(KEY_CURRENT_TEST);
        } else {
            mTestIndex = 0;
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        processCurrentTest();
    }

    /** Returns a list of tests to run in order. */
    protected abstract Test[] getTests();

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putInt(KEY_CURRENT_TEST, mTestIndex);
    }

    protected void succeed() {
        runOnUiThread(() -> {
            mTestIndex++;
            processCurrentTest();
        });
    }

    protected void error(int stringResId) {
        Toast.makeText(this, stringResId, Toast.LENGTH_SHORT).show();
    }

    /**
     * Must be invoked on the main thread
     */
    private void processCurrentTest() {
        if (mTestIndex < mTests.length) {
            mTests[mTestIndex].run();
        } else {
            // On test completion, hide "next" and "fail" buttons, and show "pass" button
            // instead.
            mInstructions.setText(R.string.tests_completed_successfully);
            mNextButton.setVisibility(View.GONE);
            findViewById(R.id.pass_button).setVisibility(View.VISIBLE);
            findViewById(R.id.fail_button).setVisibility(View.GONE);
        }
    }

    protected abstract class Test {
        private final int mStringId;

        public Test(int stringResId) {
            mStringId = stringResId;
        }

        protected void run() {
            showText();
        }

        protected void showText() {
            if (mStringId == 0) {
                return;
            }
            mInstructions.setText(mStringId);
        }

        protected void onNextClick() {
        }
    }

}
