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

package com.android.tools.idea.validator;

import com.android.tools.idea.validator.ValidatorData.Issue;
import com.android.tools.idea.validator.ValidatorData.Level;
import com.android.tools.layoutlib.annotations.NotNull;

import android.view.View;

import java.util.ArrayList;
import java.util.List;

import com.google.common.collect.BiMap;
import com.google.common.collect.HashBiMap;
import com.google.common.collect.ImmutableBiMap;

/**
 * Results of layout validation.
 */
public class ValidatorResult {

    @NotNull private final ImmutableBiMap<Long, View> mSrcMap;
    @NotNull private final ArrayList<Issue> mIssues;
    @NotNull private final Metric mMetric;

    /**
     * Please use {@link Builder} for creating results.
     */
    private ValidatorResult(BiMap<Long, View> srcMap, ArrayList<Issue> issues, Metric metric) {
        mSrcMap = ImmutableBiMap.<Long, View>builder().putAll(srcMap).build();
        mIssues = issues;
        mMetric = metric;
    }

    /**
     * @return the source map of all the Views.
     */
    public ImmutableBiMap<Long, View> getSrcMap() {
        return mSrcMap;
    }

    /**
     * @return list of issues.
     */
    public List<Issue> getIssues() {
        return mIssues;
    }

    /**
     * @return metric for validation.
     */
    public Metric getMetric() {
        return mMetric;
    }

    @Override
    public String toString() {
        StringBuilder builder = new StringBuilder()
                .append("Result containing ")
                .append(mIssues.size())
                .append(" issues:\n");

        for (Issue issue : mIssues) {
            if (issue.mLevel == Level.ERROR) {
                builder.append(" - [")
                        .append(issue.mLevel.name())
                        .append("] ")
                        .append(issue.mMsg)
                        .append("\n");
            }
        }
        return builder.toString();
    }

    public static class Builder {
        @NotNull public final BiMap<Long, View> mSrcMap = HashBiMap.create();
        @NotNull public final ArrayList<Issue> mIssues = new ArrayList<>();
        @NotNull public final Metric mMetric = new Metric();

        public ValidatorResult build() {
            return new ValidatorResult(mSrcMap, mIssues, mMetric);
        }
    }

    /**
     * Contains metric specific data.
     */
    public static class Metric {
        /** Error message. If null no error was thrown. */
        public String mErrorMessage = null;

        /** Records how long validation took */
        public long mElapsedMs = 0;

        /** How many new memories (bytes) validator creates for images. */
        public long mImageMemoryBytes = 0;

        private long mStart;

        private Metric() { }

        public void startTimer() {
            mStart = System.currentTimeMillis();
        }

        public void endTimer() {
            mElapsedMs = System.currentTimeMillis() - mStart;
        }

        @Override
        public String toString() {
            return "Validation result metric: { elapsed=" + mElapsedMs +
                    "ms, image memory=" + readableBytes() + " }";
        }

        private String readableBytes() {
            if (mImageMemoryBytes > 1000000000) {
                return mImageMemoryBytes / 1000000000 + "gb";
            }
            else if (mImageMemoryBytes > 1000000) {
                return mImageMemoryBytes / 1000000 + "mb";
            }
            else if (mImageMemoryBytes > 1000) {
                return mImageMemoryBytes / 1000 + "kb";
            }
            return mImageMemoryBytes + "bytes";
        }
    }
}
