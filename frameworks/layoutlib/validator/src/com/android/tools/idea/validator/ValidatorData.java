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

import com.android.tools.layoutlib.annotations.NotNull;
import com.android.tools.layoutlib.annotations.Nullable;

import java.util.EnumSet;
import java.util.HashSet;

import com.google.android.apps.common.testing.accessibility.framework.AccessibilityHierarchyCheck;

/**
 * Data used for layout validation.
 */
public class ValidatorData {

    /**
     * Category of validation
     */
    public enum Type {
        ACCESSIBILITY,
        RENDER,
        INTERNAL_ERROR
    }

    /**
     * Level of importance
     */
    public enum Level {
        ERROR,
        WARNING,
        INFO,
        /** The test not ran or suppressed. */
        VERBOSE,
    }

    /**
     * Determine what types and levels of validation to run.
     */
    public static class Policy {
        @NotNull public final EnumSet<Type> mTypes;
        @NotNull public final EnumSet<Level> mLevels;
        @NotNull public final HashSet<AccessibilityHierarchyCheck> mChecks = new HashSet();

        public Policy(@NotNull EnumSet<Type> types, @NotNull EnumSet<Level> levels) {
            mTypes = types;
            mLevels = levels;
        }
    }

    /**
     * Suggested fix to the user or to the studio.
     */
    public static class Fix {
        @NotNull public final String mFix;

        public Fix(String fix) {
            mFix = fix;
        }
    }

    /**
     * Issue describing the layout problem.
     */
    public static class Issue {
        @NotNull
        public final Type mType;
        @NotNull
        public final String mMsg;
        @NotNull
        public final Level mLevel;
        @Nullable
        public final Long mSrcId;
        @Nullable
        public final Fix mFix;
        @NotNull
        public final String mSourceClass;
        @Nullable
        public final String mHelpfulUrl;

        private Issue(
                @NotNull Type type,
                @NotNull String msg,
                @NotNull Level level,
                @Nullable Long srcId,
                @Nullable Fix fix,
                @NotNull String sourceClass,
                @Nullable String helpfulUrl) {
            mType = type;
            mMsg = msg;
            mLevel = level;
            mSrcId = srcId;
            mFix = fix;
            mSourceClass = sourceClass;
            mHelpfulUrl = helpfulUrl;
        }

        public static class IssueBuilder {
            private Type mType = Type.ACCESSIBILITY;
            private String mMsg;
            private Level mLevel;
            private Long mSrcId;
            private Fix mFix;
            private String mSourceClass;
            private String mHelpfulUrl;

            public IssueBuilder setType(Type type) {
                mType = type;
                return this;
            }

            public IssueBuilder setMsg(String msg) {
                mMsg = msg;
                return this;
            }

            public IssueBuilder setLevel(Level level) {
                mLevel = level;
                return this;
            }

            public IssueBuilder setSrcId(Long srcId) {
                mSrcId = srcId;
                return this;
            }

            public IssueBuilder setFix(Fix fix) {
                mFix = fix;
                return this;
            }

            public IssueBuilder setSourceClass(String sourceClass) {
                mSourceClass = sourceClass;
                return this;
            }

            public IssueBuilder setHelpfulUrl(String url) {
                mHelpfulUrl = url;
                return this;
            }

            public Issue build() {
                assert(mType != null);
                assert(mMsg != null);
                assert(mLevel != null);
                assert(mSourceClass != null);
                return new Issue(mType, mMsg, mLevel, mSrcId, mFix, mSourceClass, mHelpfulUrl);
            }
        }
    }
}
