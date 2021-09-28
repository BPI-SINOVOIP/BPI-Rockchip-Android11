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

package com.android.car.uxr;

import android.content.Context;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.XmlRes;

import java.util.Map;

/**
 * A container class for app specific Car User Experience Restriction override configurations.
 *
 * <p>{@link #getInstance(Context, int)} will returned a lazily populated cached reference to the
 * configurations object that is read using
 * {@link CarUxRestrictionsAppConfigParser#parseConfig(Context, int)}.
 *
 * <p>{@link #getMapping()} can be used to access the mapping of component IDs to configurations
 * specific to that component.
 */
public class CarUxRestrictionsAppConfig {

    private final Map<Integer, ListConfig> mMapping;
    private static CarUxRestrictionsAppConfig sInstance;

    CarUxRestrictionsAppConfig(Map<Integer, ListConfig> mapping) {
        mMapping = mapping;
    }

    /**
     * Returns a cached reference to the {@link CarUxRestrictionsAppConfig} object
     * resulting from parsing the contents of {@code xmlRes} xml resource.
     *
     * @param context - the app context
     * @param xmlRes  - the xml resource that contains the UXR override configs.
     */
    public static CarUxRestrictionsAppConfig getInstance(Context context, @XmlRes int xmlRes) {
        if (sInstance == null) {
            sInstance = CarUxRestrictionsAppConfigParser.parseConfig(context, xmlRes);
        }

        return sInstance;
    }

    /**
     * Returns a {@link Map} of Resource Ids as ints to {@link ListConfig} objects.
     */
    public Map<Integer, ListConfig> getMapping() {
        return mMapping;
    }

    /**
     * A class representing Car User Experience Restriction override configurations for a list UI
     * component.
     */
    public static class ListConfig {
        @IdRes
        private final int mId;
        private final Integer mContentLimit;
        @StringRes
        private final Integer mScrollingLimitedMessageResId;

        private ListConfig(@IdRes int id, @Nullable Integer contentLimit,
                @StringRes Integer scrollingLimitedMessageResId) {
            mId = id;
            mContentLimit = contentLimit;
            mScrollingLimitedMessageResId = scrollingLimitedMessageResId;
        }

        /**
         * Returns a {@code Builder} that can be used to build a {@link ListConfig} object for a
         * component identified with the provided {@code id}.
         *
         * @param id - an identifier for the component whose behavior needs to be overridden with
         *           the configurations specified in the resulting {@link ListConfig} object.
         */
        public static Builder builder(@IdRes int id) {
            return new Builder(id);
        }

        /**
         * Returns the identifier for the component whose behavior needs to be overridden by this
         * config object.
         */
        @IdRes
        public int getId() {
            return mId;
        }

        /**
         * Returns the item limit to impose on the contents of the corresponding list component.
         */
        @Nullable
        public Integer getContentLimit() {
            return mContentLimit;
        }

        /**
         * Returns the string resource ID to use when educating users about why the content in the
         * list they're browsing has been limited.
         */
        @Nullable
        @StringRes
        public Integer getScrollingLimitedMessageResId() {
            return mScrollingLimitedMessageResId;
        }

        /**
         * A Builder for {@link ListConfig}.
         */
        public static class Builder {
            @IdRes
            private final int mId;
            private Integer mContentLimit;
            @StringRes
            private Integer mScrollingLimitedMessageResId;


            /**
             * Constructs a {@code Builder} that can be used to build a {@link ListConfig} object
             * for a component identified with the provided {@code id}.
             *
             * @param id - an identifier for the component whose behavior needs to be overridden
             *           with the configurations specified in the resulting {@link ListConfig}
             *           object.
             */
            private Builder(@IdRes int id) {
                mId = id;
            }

            /**
             * Sets the item limit to impose on the contents of the corresponding list component.
             *
             * @param contentLimit - the item limit
             * @return this {@code Builder} object to facilitate chaining.
             */
            public Builder setContentLimit(int contentLimit) {
                mContentLimit = contentLimit;
                return this;
            }

            /**
             * Sets the string resource ID to use when educating users about why the content in the
             * * list they're browsing has been limited.
             *
             * @param scrollingLimitedMessageResId - an educational message string resource ID
             * @return this {@code Builder} object to facilitate chaining.
             */
            public Builder setScrollingLimitedMessageResId(
                    @StringRes int scrollingLimitedMessageResId) {
                mScrollingLimitedMessageResId = scrollingLimitedMessageResId;
                return this;
            }

            /**
             * Build and return a {@link ListConfig} object with the values provided to this
             * {@code Builder} object.
             */
            public ListConfig build() {
                return new ListConfig(mId, mContentLimit, mScrollingLimitedMessageResId);
            }
        }
    }
}
