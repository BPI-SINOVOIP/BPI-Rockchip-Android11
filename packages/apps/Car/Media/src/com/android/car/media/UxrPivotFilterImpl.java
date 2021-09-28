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
 * limitations under the License
 */

package com.android.car.media;

import android.util.Log;

import androidx.recyclerview.widget.RecyclerView;


public class UxrPivotFilterImpl implements UxrPivotFilter {

    private static final String TAG = "UxrPivotFilterImpl";

    private final RecyclerView.Adapter<?> mAdapter;
    private final int mMaxItems;
    private final int mMaxItemsDiv2;

    private int mUnlimitedCount;
    private int mPivotIndex;
    private final ListRange mRange = new ListRange();
    private final ListRange mSavedRange = new ListRange();


    /**
     * Constructor
     * @param adapter the adapter to notify of changes in {@link #updatePivotIndex}.
     * @param maxItems the maximum number of items to show. When > 0, its value is rounded up to
     *                 the nearest greater odd integer in order to show the active element plus or
     *                 minus maxItems / 2.
     */
    public UxrPivotFilterImpl(RecyclerView.Adapter<?> adapter, int maxItems) {
        mAdapter = adapter;
        if (maxItems <= 0) {
            mMaxItemsDiv2 = 0;
            mMaxItems = 0;
        } else {
            mMaxItemsDiv2 = maxItems / 2;
            mMaxItems = 1 + (mMaxItemsDiv2 * 2);
        }
    }

    @Override
    public String toString() {
        return "UxrPivotFilterImpl{" +
                "mMaxItemsDiv2=" + mMaxItemsDiv2 +
                ", mUnlimitedCount=" + mUnlimitedCount +
                ", mPivotIndex=" + mPivotIndex +
                ", mRange=" + mRange.toString() +
                '}';
    }

    @Override
    public int getFilteredCount() {
        return mRange.mLimitedCount;
    }

    @Override
    public void invalidateMessagePositions() {
        if (mRange.mClampedHead > 0) {
            mAdapter.notifyItemChanged(0);
        }
        if (mRange.mClampedTail > 0) {
            mAdapter.notifyItemChanged(getFilteredCount() - 1);
        }
    }

    @Override
    public void recompute(int newCount, int pivotIndex) {
        if (pivotIndex < 0 || newCount <= pivotIndex) {
            Log.e(TAG, "Invalid pivotIndex: " + pivotIndex + " newCount: " + newCount);
            pivotIndex = 0;
        }
        mUnlimitedCount = newCount;
        mPivotIndex = pivotIndex;
        mRange.mClampedHead = 0;
        mRange.mClampedTail = 0;

        if (newCount <= mMaxItems) {
            // Under the cap case.
            mRange.mStartIndex = 0;
            mRange.mEndIndex = mUnlimitedCount;
            mRange.mLimitedCount = mUnlimitedCount;
        } else if (mMaxItems <= 0) {
            // Zero cap case.
            mRange.mStartIndex = 0;
            mRange.mEndIndex = 0;
            mRange.mLimitedCount = 1; // One limit message
            mRange.mClampedTail = 1;
        } else if (mPivotIndex <= mMaxItemsDiv2) {
            // No need to clamp the head case
            // For example: P = 2, M/2 = 2 => exactly two items before the pivot.
            // Tail has to be clamped or we'd be in the "under the cap" case.
            mRange.mStartIndex = 0;
            mRange.mEndIndex = mMaxItems;
            mRange.mLimitedCount = mMaxItems + 1; // One limit message at the end
            mRange.mClampedTail = 1;
        } else if ((mUnlimitedCount - 1 - mPivotIndex) <= mMaxItemsDiv2) {
            // No need to clamp the tail case
            // For example: C = 5, P = 2 => exactly 2 items after the pivot (count is exclusive).
            // Head has to be clamped or we'd be in the "under the cap" case.
            mRange.mEndIndex = mUnlimitedCount;
            mRange.mStartIndex = mRange.mEndIndex - mMaxItems;
            mRange.mLimitedCount = mMaxItems + 1; // One limit message at the start
            mRange.mClampedHead = 1;
        } else {
            // Both head and tail need clamping
            mRange.mStartIndex = mPivotIndex - mMaxItemsDiv2;
            mRange.mEndIndex = mPivotIndex + mMaxItemsDiv2 + 1;
            mRange.mLimitedCount = mMaxItems + 2; // One limit message at each end.
            mRange.mClampedHead = 1;
            mRange.mClampedTail = 1;
        }
    }

    /**
     * Computes the new restrictions when the pivot changes but the list remains the same.
     * Notifications are done from the end to the beginning of the list so we don't have to mess
     * with the indices as we go. Beyond the addition or removal of head and tail messages, the
     * method boils down to intersecting two segments and determining which elements to remove and
     * which to add to go from the old one to the new one.<p/>
     * The diagram below illustrates all the cases with [S1, E1[ the old range and [S2, E2[ the
     * new one. The =, + and - signs identify identical, inserted and removed elements.<p/>
     * <pre>
     *                             S1                      E1
     *                            |........................|
     *                            |                        |
     *             S2             |      E2                |
     *             +++++++++++++++|======------------------|
     *                            |                        |
     *                            |             S2         |           E2
     *                            |             ===========|+++++++++++
     *                            |                        |
     *                            |     S2         E2      |
     *                            |-----===========--------|
     *                            |                        |
     *                  S2        |                        |                E2
     *                  ++++++++++|========================|++++++++++++++++
     * <pre/>
     */
    @Override
    public void updatePivotIndex(int pivotIndex) {
        if (mPivotIndex == pivotIndex) return;

        mSavedRange.copyFrom(mRange);

        recompute(mUnlimitedCount, pivotIndex);

        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "updatePivotIndex pivot: " + pivotIndex + " saved: " + mSavedRange
                    + " new: " + mRange);
        }

        if (mSavedRange.intersects(mRange)) {
            if (mSavedRange.mClampedTail < mRange.mClampedTail) {
                // Add a tail message, inserting it after the last element of the restricted list.
                mAdapter.notifyItemInserted(mSavedRange.mLimitedCount);
            }
            if (mSavedRange.mClampedTail > mRange.mClampedTail) {
                // Remove a tail message which was shown as the last element of the restricted list.
                mAdapter.notifyItemRemoved(mSavedRange.mLimitedCount - 1);
            }

            // Add or remove items at the end
            if (mSavedRange.mEndIndex < mRange.mEndIndex) {
                int insertPos = mSavedRange.indexToPosition(mSavedRange.mEndIndex);
                int insertCount = mRange.mEndIndex - mSavedRange.mEndIndex;
                mAdapter.notifyItemRangeInserted(insertPos, insertCount);
            }
            if (mSavedRange.mEndIndex > mRange.mEndIndex) {
                int delPos = mSavedRange.indexToPosition(mRange.mEndIndex);
                int delCount = mSavedRange.mEndIndex - mRange.mEndIndex;
                mAdapter.notifyItemRangeRemoved(delPos, delCount);
            }

            // Add or remove items at the start
            if (mSavedRange.mStartIndex > mRange.mStartIndex) {
                int insertPos = mSavedRange.indexToPosition(mSavedRange.mStartIndex);
                int insertCount = mSavedRange.mStartIndex - mRange.mStartIndex;
                mAdapter.notifyItemRangeInserted(insertPos, insertCount);
            }
            if (mSavedRange.mStartIndex < mRange.mStartIndex) {
                int delPos = mSavedRange.indexToPosition(mSavedRange.mStartIndex);
                int delCount = mRange.mStartIndex - mSavedRange.mStartIndex;
                mAdapter.notifyItemRangeRemoved(delPos, delCount);
            }

            // Add or remove the head message
            if (mSavedRange.mClampedHead < mRange.mClampedHead) {
                mAdapter.notifyItemInserted(0);
            }
            if (mSavedRange.mClampedHead > mRange.mClampedHead) {
                mAdapter.notifyItemRemoved(0);
            }
        } else {
            // No element is the same, invalidate all.
            mAdapter.notifyDataSetChanged();
        }
    }

    @Override
    public int indexToPosition(int index) {
        if ((mRange.mStartIndex <= index) && (index < mRange.mEndIndex)) {
            return mRange.indexToPosition(index);
        } else {
            return INVALID_POSITION;
        }
    }

    @Override
    public int positionToIndex(int position) {
        return mRange.positionToIndex(position);
    }


    /** A portion of the unfiltered list. */
    private static class ListRange {

        /** In original data, inclusive. */
        private int mStartIndex;
        /** In original data, exclusive. */
        private int mEndIndex;

        /** 1 when clamped, otherwise 0. */
        private int mClampedHead;
        /** 1 when clamped, otherwise 0. */
        private int mClampedTail;

        /** The count of the resulting elements, including the truncation message(s). */
        private int mLimitedCount;

        public void copyFrom(ListRange range) {
            mStartIndex = range.mStartIndex;
            mEndIndex = range.mEndIndex;
            mClampedHead = range.mClampedHead;
            mClampedTail = range.mClampedTail;
            mLimitedCount = range.mLimitedCount;
        }

        @Override
        public String toString() {
            return "ListRange{" +
                    "mStartIndex=" + mStartIndex +
                    ", mEndIndex=" + mEndIndex +
                    ", mClampedHead=" + mClampedHead +
                    ", mClampedTail=" + mClampedTail +
                    ", mLimitedCount=" + mLimitedCount +
                    '}';
        }

        public boolean intersects(ListRange range) {
            return ((range.mEndIndex > mStartIndex) && (mEndIndex > range.mStartIndex));
        }

        /** Unchecked index needed by {@link #updatePivotIndex}. */
        public int indexToPosition(int index) {
            return index - mStartIndex + mClampedHead;
        }

        public int positionToIndex(int position) {
            int index = position - mClampedHead + mStartIndex;
            if ((index < mStartIndex) || (mEndIndex <= index)) {
                return INVALID_INDEX;
            } else {
                return index;
            }
        }
    }
}
