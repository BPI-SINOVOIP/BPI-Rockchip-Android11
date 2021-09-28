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

package com.android.car.media;


/**
 * Interface for helper objects that hide elements from lists that are too long. The limiting
 * happens around a pivot element that can be anywhere in the list. Elements near that pivot will
 * be visible, while elements at the head and / or tail of the list will be replaced by a message
 * telling the user about the truncation.
 * When no restrictions are in effect, the {@link #PASS_THROUGH} instance should be used.
 */
public interface UxrPivotFilter {

    int INVALID_INDEX = -1;
    int INVALID_POSITION = -1;

    /**
     * Computes new restrictions when the list (and optionally) the pivot have changed.
     * The implementation doesn't send any notification.
     */
    void recompute(int newCount, int pivotIndex);

    /**
     * Computes new restrictions when only the pivot has changed.
     * The implementation must send notification changes (ideally incremental ones).
     */
    void updatePivotIndex(int pivotIndex);

    /** Returns the number of elements in the resulting list, including the message(s). */
    int getFilteredCount();

    /**
     * Converts an index in the unfiltered data set to a RV position in the filtered UI in the
     * 0 .. {@link #getFilteredCount} range which includes the limits message(s).
     * Returns INVALID_POSITION if that element has been filtered out.
     */
    int indexToPosition(int index);

    /**
     * Converts a RV position in the filtered UI to an index in the unfiltered data set.
     * Returns INVALID_INDEX if a message is shown at that position.
     */
    int positionToIndex(int position);

    /** Send notification changes for the restriction message(s) if there are any. */
    void invalidateMessagePositions();


    /**
     * A trivial implementation that doesn't do any filtering (simplifies the filter's code).
     */
    UxrPivotFilter PASS_THROUGH = new UxrPivotFilter() {
        private int mCount;

        @Override
        public void recompute(int newCount, int pivotIndex) {
            mCount = newCount;
        }

        @Override
        public void updatePivotIndex(int pivotIndex) {
        }

        @Override
        public int getFilteredCount() {
            return mCount;
        }

        @Override
        public int indexToPosition(int index) {
            return index;
        }

        @Override
        public int positionToIndex(int position) {
            return position;
        }

        @Override
        public void invalidateMessagePositions() {
        }
    };
}
