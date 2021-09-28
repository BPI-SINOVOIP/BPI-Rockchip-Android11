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

package android.theme.app;

import android.content.Context;
import android.os.Handler;
import android.util.Pair;

import java.util.ArrayList;
import java.util.function.Consumer;
import java.util.function.Supplier;

/**
 * Runnable that re-posts itself on a handler until either all of the conditions are satisfied
 * or a retry threshold is exceeded.
 */
class ConditionCheck implements Runnable {
    private static final int MAX_RETRIES = 3;
    private static final int RETRY_DELAY = 500;

    private final Handler mHandler;
    private final Runnable mOnSuccess;
    private final Consumer<String> mOnFailure;
    private final ArrayList<Pair<String, Supplier<Boolean>>> mConditions = new ArrayList<>();

    private ArrayList<Pair<String, Supplier<Boolean>>> mRemainingConditions = new ArrayList<>();
    private int mRemainingRetries;

    ConditionCheck(Context context, Runnable onSuccess, Consumer<String> onFailure) {
        mHandler = new Handler(context.getMainLooper());
        mOnSuccess = onSuccess;
        mOnFailure = onFailure;
    }

    public ConditionCheck addCondition(String summary, Supplier<Boolean> condition) {
        mConditions.add(new Pair<>(summary, condition));
        return this;
    }

    public void start() {
        mRemainingConditions = new ArrayList<>(mConditions);
        mRemainingRetries = 0;

        mHandler.removeCallbacks(this);
        mHandler.post(this);
    }

    public void cancel() {
        mHandler.removeCallbacks(this);
    }

    @Override
    public void run() {
        mRemainingConditions.removeIf(condition -> condition.second.get());
        if (mRemainingConditions.isEmpty()) {
            mOnSuccess.run();
        } else if (mRemainingRetries < MAX_RETRIES) {
            mRemainingRetries++;
            mHandler.removeCallbacks(this);
            mHandler.postDelayed(this, RETRY_DELAY);
        } else {
            final StringBuffer buffer = new StringBuffer("Failed conditions:");
            mRemainingConditions.forEach(condition ->
                    buffer.append("\n").append(condition.first));
            mOnFailure.accept(buffer.toString());
        }
    }
}
