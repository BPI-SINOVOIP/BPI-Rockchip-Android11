/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.documentsui;

import android.content.Context;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.documentsui.NavigationViewManager.Breadcrumb;
import com.android.documentsui.NavigationViewManager.Environment;
import com.android.documentsui.dirlist.AccessibilityEventRouter;

import java.util.function.Consumer;
import java.util.function.IntConsumer;

/**
 * Horizontal breadcrumb
 */
public final class HorizontalBreadcrumb extends RecyclerView implements Breadcrumb {

    private static final int USER_NO_SCROLL_OFFSET_THRESHOLD = 5;

    private LinearLayoutManager mLayoutManager;
    private BreadcrumbAdapter mAdapter;
    private IntConsumer mClickListener;

    public HorizontalBreadcrumb(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public HorizontalBreadcrumb(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public HorizontalBreadcrumb(Context context) {
        super(context);
    }

    @Override
    public void setup(Environment env,
            com.android.documentsui.base.State state,
            IntConsumer listener) {

        mClickListener = listener;
        mLayoutManager = new HorizontalBreadcrumbLinearLayoutManager(
                getContext(), LinearLayoutManager.HORIZONTAL, false);
        mAdapter = new BreadcrumbAdapter(state, env, this::onKey);
        // Since we are using GestureDetector to detect click events, a11y services don't know which
        // views are clickable because we aren't using View.OnClickListener. Thus, we need to use a
        // custom accessibility delegate to route click events correctly.
        // See AccessibilityClickEventRouter for more details on how we are routing these a11y
        // events.
        setAccessibilityDelegateCompat(
                new AccessibilityEventRouter(this,
                        (View child) -> onAccessibilityClick(child), null));

        setLayoutManager(mLayoutManager);
        addOnItemTouchListener(new ClickListener(getContext(), this::onSingleTapUp));
    }

    @Override
    public void show(boolean visibility) {
        if (visibility) {
            setVisibility(VISIBLE);
            boolean shouldScroll = !hasUserDefineScrollOffset();
            if (getAdapter() == null) {
                setAdapter(mAdapter);
            } else {
                int currentItemCount = mAdapter.getItemCount();
                int lastItemCount = mAdapter.getLastItemSize();
                if (currentItemCount > lastItemCount) {
                    mAdapter.notifyItemRangeInserted(lastItemCount,
                            currentItemCount - lastItemCount);
                    mAdapter.notifyItemChanged(lastItemCount - 1);
                } else if (currentItemCount < lastItemCount) {
                    mAdapter.notifyItemRangeRemoved(currentItemCount,
                            lastItemCount - currentItemCount);
                    mAdapter.notifyItemChanged(currentItemCount - 1);
                } else {
                    mAdapter.notifyItemChanged(currentItemCount - 1);
                }
            }
            if (shouldScroll) {
                mLayoutManager.scrollToPosition(mAdapter.getItemCount() - 1);
            }
        } else {
            setVisibility(GONE);
            setAdapter(null);
        }
        mAdapter.updateLastItemSize();
    }

    private boolean hasUserDefineScrollOffset() {
        final int maxOffset = computeHorizontalScrollRange() - computeHorizontalScrollExtent();
        return (maxOffset - computeHorizontalScrollOffset() > USER_NO_SCROLL_OFFSET_THRESHOLD);
    }

    private boolean onAccessibilityClick(View child) {
        int pos = getChildAdapterPosition(child);
        if (pos != getAdapter().getItemCount() - 1) {
            mClickListener.accept(pos);
            return true;
        }
        return false;
    }

    private boolean onKey(View v, int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_ENTER:
                return onAccessibilityClick(v);
            default:
                return false;
        }
    }

    @Override
    public void postUpdate() {
    }

    private void onSingleTapUp(MotionEvent e) {
        View itemView = findChildViewUnder(e.getX(), e.getY());
        int pos = getChildAdapterPosition(itemView);
        if (pos != mAdapter.getItemCount() - 1 && pos != -1) {
            mClickListener.accept(pos);
        }
    }

    private static final class BreadcrumbAdapter
            extends RecyclerView.Adapter<BreadcrumbHolder> {

        private final Environment mEnv;
        private final com.android.documentsui.base.State mState;
        private final View.OnKeyListener mClickListener;
        // We keep the old item size so the breadcrumb will only re-render views that are necessary
        private int mLastItemSize;

        public BreadcrumbAdapter(com.android.documentsui.base.State state,
                Environment env,
                View.OnKeyListener clickListener) {
            mState = state;
            mEnv = env;
            mClickListener = clickListener;
            mLastItemSize = getItemCount();
        }

        @Override
        public BreadcrumbHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View v = LayoutInflater.from(parent.getContext())
                    .inflate(R.layout.navigation_breadcrumb_item, null);
            return new BreadcrumbHolder(v);
        }

        @Override
        public void onBindViewHolder(BreadcrumbHolder holder, int position) {
            final int padding = (int) holder.itemView.getResources()
                    .getDimension(R.dimen.breadcrumb_item_padding);
            final int enableColor = holder.itemView.getContext().getColor(R.color.primary);
            final boolean isFirst = position == 0;
            // Note that when isFirst is true, there might not be a DocumentInfo on the stack as it
            // could be an error state screen accessible from the root info.
            final boolean isLast = position == getItemCount() - 1;

            holder.mTitle.setText(
                    isFirst ? mEnv.getCurrentRoot().title : mState.stack.get(position).displayName);
            holder.mTitle.setTextColor(isLast ? enableColor : holder.mDefaultTextColor);
            holder.mTitle.setPadding(isFirst ? padding * 3 : padding,
                    padding, isLast ? padding * 2 : padding, padding);
            holder.mArrow.setVisibility(isLast ? View.GONE : View.VISIBLE);

            holder.itemView.setOnKeyListener(mClickListener);
            holder.setLast(isLast);
        }

        @Override
        public int getItemCount() {
            // Don't show recents in the breadcrumb.
            if (mState.stack.isRecents()) {
                return 0;
            }
            // Continue showing the root title in the breadcrumb for cross-profile error screens.
            if (mState.supportsCrossProfile()
                    && mState.stack.size() == 0
                    && mState.stack.getRoot() != null
                    && mState.stack.getRoot().supportsCrossProfile()) {
                return 1;
            }
            return mState.stack.size();
        }

        public int getLastItemSize() {
            return mLastItemSize;
        }

        public void updateLastItemSize() {
            mLastItemSize = getItemCount();
        }
    }

    private static final class ClickListener extends GestureDetector
            implements OnItemTouchListener {

        public ClickListener(Context context, Consumer<MotionEvent> listener) {
            super(context, new SimpleOnGestureListener() {
                @Override
                public boolean onSingleTapUp(MotionEvent e) {
                    listener.accept(e);
                    return true;
                }
            });
        }

        @Override
        public boolean onInterceptTouchEvent(RecyclerView rv, MotionEvent e) {
            onTouchEvent(e);
            return false;
        }

        @Override
        public void onTouchEvent(RecyclerView rv, MotionEvent e) {
            onTouchEvent(e);
        }

        @Override
        public void onRequestDisallowInterceptTouchEvent(boolean disallowIntercept) {
        }
    }

    private static class HorizontalBreadcrumbLinearLayoutManager extends LinearLayoutManager {

        /**
         * Disable predictive animations. There is a bug in RecyclerView which causes views that
         * are being reloaded to pull invalid view holders from the internal recycler stack if the
         * adapter size has decreased since the ViewHolder was recycled.
         */
        @Override
        public boolean supportsPredictiveItemAnimations() {
            return false;
        }

        HorizontalBreadcrumbLinearLayoutManager(
                Context context, int orientation, boolean reverseLayout) {
            super(context, orientation, reverseLayout);
        }
    }
}
