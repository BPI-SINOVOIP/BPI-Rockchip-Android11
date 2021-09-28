/*
 * Copyright (C) 2013 The Android Open Source Project
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

package com.android.camera.ui;

import android.content.Context;
import android.content.res.Configuration;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import java.util.ArrayList;
import java.util.List;

/**
 * TopRightWeightedLayout is a LinearLayout that reorders its
 * children such that the right most child is the top most child
 * on an orientation change.
 */
public class TopRightWeightedLayout extends LinearLayout {
    public TopRightWeightedLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        Configuration configuration = getContext().getResources().getConfiguration();
        checkOrientation(configuration.orientation);
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        checkOrientation(configuration.orientation);
        scrollTo(0, 0);
    }

    private int mInvisibleLenght;
    private float mLastX;
    private float mLastY;
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // TODO Auto-generated method stub
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mLastX = event.getX();
            mLastY = event.getY();
        } else {
            if (event.getActionMasked() == MotionEvent.ACTION_MOVE) {
                int direction = getOrientation();
                if (direction == LinearLayout.HORIZONTAL) {
                    if (event.getX() - mLastX >= 0) {//right
                        int scrollX = getScrollX();
                        float scrollTo = scrollX - (event.getX() - mLastX);
                        float scrollBy = Math.abs(scrollTo) > mInvisibleLenght / 2
                                ? (Math.abs(scrollTo) - mInvisibleLenght / 2) - (event.getX() - mLastX)
                                : - (event.getX() - mLastX);
                        if (Math.abs(scrollBy) > 2.0f)
                            scrollBy((int) scrollBy, 0);
                    } else if (event.getX() - mLastX < 0) {//left
                        int scrollX = getScrollX();
                        float scrollTo = scrollX - (event.getX() - mLastX);
                        float scrollBy = Math.abs(scrollTo) > mInvisibleLenght / 2
                                ? (event.getX() - mLastX) + (Math.abs(scrollTo) - mInvisibleLenght / 2)
                                : - (event.getX() - mLastX);
                        if (Math.abs(scrollBy) > 2.0f)
                            scrollBy((int) scrollBy, 0);
                    }
                } else if (direction == LinearLayout.VERTICAL) {
                    if (event.getY() - mLastY >= 0) {//down
                        int scrollY = getScrollY();
                        float scrollTo = scrollY - (event.getY() - mLastY);
                        float scrollBy = Math.abs(scrollTo) > mInvisibleLenght / 2
                                ? (Math.abs(scrollTo) - mInvisibleLenght / 2) - (event.getY() - mLastY)
                                : - (event.getY() - mLastY);
                        if (Math.abs(scrollBy) > 2.0f)
                            scrollBy(0, (int) scrollBy);
                    } else if (event.getY() - mLastY < 0) {//up
                        int scrollY = getScrollY();
                        float scrollTo = scrollY - (event.getY() - mLastY);
                        float scrollBy = Math.abs(scrollTo) > mInvisibleLenght / 2
                                ? (event.getY() - mLastY) + (Math.abs(scrollTo) - mInvisibleLenght / 2)
                                : - (event.getY() - mLastY);
                        if (Math.abs(scrollBy) > 2.0f)
                            scrollBy(0, (int) scrollBy);
                    }
                }
            }
            mLastX = event.getX();
            mLastY = event.getY();
        }
        return super.onTouchEvent(event);
    }
    
    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        // TODO Auto-generated method stub
        super.onLayout(changed, l, t, r, b);
        computeScrollZone(getOrientation());
    }

    private void computeScrollZone(int direction) {
        if (direction == LinearLayout.HORIZONTAL) {
            int VisibleChildViewLenght = 0;
            int VisibleLenght = getWidth();
            for (int i = getChildCount() - 1; i >= 0; i--) {
                View child = getChildAt(i);
                if (child.getVisibility() == View.VISIBLE) {
                    LinearLayout.LayoutParams lp = (LinearLayout.LayoutParams) child.getLayoutParams();
                    VisibleChildViewLenght += (child.getWidth() + lp.leftMargin + lp.rightMargin);
                }
            }
            mInvisibleLenght = VisibleChildViewLenght - VisibleLenght;
        } else if (direction == LinearLayout.VERTICAL) {
            int VisibleChildViewLenght = 0;
            int VisibleLenght = getHeight();
            for (int i = getChildCount() - 1; i >= 0; i--) {
                View child = getChildAt(i);
                if (child.getVisibility() == View.VISIBLE) {
                    LinearLayout.LayoutParams lp = (LinearLayout.LayoutParams) child.getLayoutParams();
                    VisibleChildViewLenght += (child.getHeight() + lp.topMargin + lp.bottomMargin);
                }
            }
            mInvisibleLenght = VisibleChildViewLenght - VisibleLenght;
        }
    }

    public boolean isScrollable() {
        if (mInvisibleLenght > 0)
            return true;
        return false;
    }

    /**
     * Set the orientation of this layout if it has changed,
     * and center the elements based on the new orientation.
     */
    public void checkOrientation(int orientation) {
        final boolean isHorizontal = LinearLayout.HORIZONTAL == getOrientation();
        final boolean isPortrait = Configuration.ORIENTATION_PORTRAIT == orientation;
        if (isPortrait && !isHorizontal) {
            // Portrait orientation is out of sync, setting to horizontal
            // and reversing children
            fixGravityAndPadding(LinearLayout.HORIZONTAL);
            setOrientation(LinearLayout.HORIZONTAL);
            reverseChildren();
            requestLayout();
        } else if (!isPortrait && isHorizontal) {
            // Landscape orientation is out of sync, setting to vertical
            // and reversing children
            fixGravityAndPadding(LinearLayout.VERTICAL);
            setOrientation(LinearLayout.VERTICAL);
            reverseChildren();
            requestLayout();
        }
    }

    /**
     * Reverse the ordering of the children in this layout.
     * Note: bringChildToFront produced a non-deterministic ordering.
     */
    private void reverseChildren() {
        List<View> children = new ArrayList<View>();
        for (int i = getChildCount() - 1; i >= 0; i--) {
            children.add(getChildAt(i));
        }
        for (View v : children) {
            bringChildToFront(v);
        }
    }

    /**
     * Swap gravity:
     * left for bottom
     * right for top
     * center horizontal for center vertical
     * etc
     *
     * also swap left|right padding for bottom|top
     */
    private void fixGravityAndPadding(int direction) {
        for (int i = 0; i < getChildCount(); i++) {
            // gravity swap
            View v = getChildAt(i);
            LinearLayout.LayoutParams layoutParams = (LinearLayout.LayoutParams) v.getLayoutParams();
            int gravity = layoutParams.gravity;

            if (direction == LinearLayout.VERTICAL) {
                if ((gravity & Gravity.LEFT) != 0) {   // if gravity left is set . . .
                    gravity &= ~Gravity.LEFT;          // unset left
                    gravity |= Gravity.BOTTOM;         // and set bottom
                }
            } else {
                if ((gravity & Gravity.BOTTOM) != 0) { // etc
                    gravity &= ~Gravity.BOTTOM;
                    gravity |= Gravity.LEFT;
                }
            }

            if (direction == LinearLayout.VERTICAL) {
                if ((gravity & Gravity.RIGHT) != 0) {
                    gravity &= ~Gravity.RIGHT;
                    gravity |= Gravity.TOP;
                }
            } else {
                if ((gravity & Gravity.TOP) != 0) {
                    gravity &= ~Gravity.TOP;
                    gravity |= Gravity.RIGHT;
                }
            }

            // don't mess with children that are centered in both directions
            if ((gravity & Gravity.CENTER) != Gravity.CENTER) {
                if (direction == LinearLayout.VERTICAL) {
                    if ((gravity & Gravity.CENTER_VERTICAL) != 0) {
                        gravity &= ~ Gravity.CENTER_VERTICAL;
                        gravity |= Gravity.CENTER_HORIZONTAL;
                    }
                } else {
                    if ((gravity & Gravity.CENTER_HORIZONTAL) != 0) {
                        gravity &= ~ Gravity.CENTER_HORIZONTAL;
                        gravity |= Gravity.CENTER_VERTICAL;
                    }
                }
            }

            layoutParams.gravity = gravity;

            // padding swap
            int paddingLeft = v.getPaddingLeft();
            int paddingTop = v.getPaddingTop();
            int paddingRight = v.getPaddingRight();
            int paddingBottom = v.getPaddingBottom();
            v.setPadding(paddingBottom, paddingRight, paddingTop, paddingLeft);
        }
    }
}