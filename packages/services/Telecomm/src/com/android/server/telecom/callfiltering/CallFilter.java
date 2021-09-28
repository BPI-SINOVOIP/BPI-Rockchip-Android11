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

package com.android.server.telecom.callfiltering;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionStage;

public class CallFilter {
    private List<CallFilter> mDependencies;
    private List<CallFilter> mFollowings;
    private int mIndegree;
    public CallFilteringResult mPriorStageResult;
    public CallFilteringResult result;
    private CompletableFuture<CallFilteringResult> mResultFuture;

    public CallFilter() {
        mDependencies = new ArrayList<>();
        mFollowings = new ArrayList<>();
        mPriorStageResult = null;
    }

    public CompletionStage<CallFilteringResult> startFilterLookup(
            CallFilteringResult priorStageResult) {
        return CompletableFuture.completedFuture(priorStageResult);
    }

    List<CallFilter> getDependencies() {
        return mDependencies;
    }

    void addDependency(CallFilter filter) {
        synchronized (this) {
            mDependencies.add(filter);
            mIndegree = mDependencies.size();
        }
    }

    List<CallFilter> getFollowings() {
        return mFollowings;
    }

    void addFollowings(CallFilter filter) {
        mFollowings.add(filter);
    }

    int decrementAndGetIndegree() {
        synchronized (this) {
            mIndegree--;
            return mIndegree;
        }
    }

    public CallFilteringResult getResult() {
        if (result == null) {
            throw new NullPointerException("Result of this filter is null. This filter hasn't "
            + "finished performing");
        } else {
            return result;
        }
    }
}
