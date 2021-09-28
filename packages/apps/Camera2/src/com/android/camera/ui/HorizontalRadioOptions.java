package com.android.camera.ui;

import java.util.ArrayList;
import java.util.List;

import com.android.camera.ui.RadioOptions.OnOptionClickListener;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;

import com.android.camera2.R;

public class HorizontalRadioOptions extends LinearLayout {
    /**
     * Listener for responding to {@link android.view.View} click events.
     */
    public interface OnOptionClickListener {
        /**
         * Override to respond to  {@link android.view.View} click events.
         * @param v {@link android.view.View} that was clicked.
         */
        public void onOptionClicked(View v);
    }

    private Drawable mBackground;
    private OnOptionClickListener mOnOptionClickListener;

    /**
     * Set the OnOptionClickListener.
     * @params listener The listener to set.
     */
    public void setOnOptionClickListener(OnOptionClickListener listener) {
        mOnOptionClickListener = listener;
    }

    public HorizontalRadioOptions(Context context, AttributeSet attrs) {
        super(context, attrs);
        // TODO Auto-generated constructor stub
        TypedArray a = context.getTheme().obtainStyledAttributes(
            attrs,
            R.styleable.RadioOptions,
            0, 0);
        int drawableId = a.getResourceId(R.styleable.RadioOptions_selected_drawable, 0);
        if (drawableId > 0) {
            mBackground = context.getResources()
                .getDrawable(drawableId);
        }
    }

    @Override
    protected void onFinishInflate() {
        // TODO Auto-generated method stub
        super.onFinishInflate();
        Configuration configuration = getContext().getResources().getConfiguration();
        checkOrientation(configuration.orientation);
        updateListeners();
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        // TODO Auto-generated method stub
        super.onConfigurationChanged(newConfig);
        checkOrientation(newConfig.orientation);
    }

    /**
     * Update each child {@link android.view.View}'s {@link android.view.View.OnClickListener}.
     * Call this if the child views are added after the OnOptionClickListener,
     * e.g. if the child views are added programatically.
     */
    public void updateListeners() {
        View.OnClickListener onClickListener = new View.OnClickListener() {
            @Override
            public void onClick(View button) {
                setSelectedOptionByView(button);
            }
        };

        for (int i = 0; i < getChildCount(); i++) {
            View button = getChildAt(i);
            button.setOnClickListener(onClickListener);
        }
    }

    /**
     * Sets a child {@link android.view.View} as selected by tag.
     * @param tag Tag that identifies a child {@link android.view.View}. No effect if view not found.
     */
    public void setSelectedOptionByTag(Object tag) {
        View button = findViewWithTag(tag);
        setSelectedOptionByView(button);
    }

    /**
     * Sets a child {@link android.view.View} as selected by id.
     * @param id Resource ID  that identifies a child {@link android.view.View}. No effect if view not found.
     */
    public void setSeletedOptionById(int id) {
        View button = findViewById(id);
        setSelectedOptionByView(button);
    }

    private void setSelectedOptionByView(View view) {
        if (view != null) {
            // Reset all button states.
            for (int i = 0; i < getChildCount(); i++) {
                getChildAt(i).setBackground(null);
            }

            // Highlight the appropriate button.
            view.setBackground(mBackground);
            if (mOnOptionClickListener != null) {
                mOnOptionClickListener.onOptionClicked(view);
            }
        }
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
