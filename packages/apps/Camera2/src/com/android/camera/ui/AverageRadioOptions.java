package com.android.camera.ui;

import com.android.camera.CaptureLayoutHelper;
import com.android.camera.debug.Log;
import com.android.camera.util.CameraUtil;
import com.android.camera.util.Gusterpolator;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.LinearLayout;

import com.android.camera2.R;

public class AverageRadioOptions extends RadioOptions {
    private final static Log.Tag TAG = new Log.Tag("AverageRadioOptions");

    private CaptureLayoutHelper mCaptureLayoutHelper = null;
    private float mOptionsHeight;

    private boolean mIsHiddenOrHiding;
    private boolean mShouldShowModeOptionsToggle;
    private View mViewToShowHide;
    private View mViewToOverlay;

    private AnimatorSet mVisibleAnimator;
    private AnimatorSet mHiddenAnimator;

    private static final int SHOW_ANIMATION_TIME = 350;
    private static final int HIDE_ANIMATION_TIME = 200;

    public static final int LEFT_OR_TOP = 0;
    public static final int RIGHT_OR_BOTTOM = 1;

    private boolean mIsPortrait;

    private int mDirection;

    private float mMargin;
    private float mMarginLeft;
    private float mMarginTop;
    private float mMarginRight;
    private float mMarginBottom;
    private int mChildWidth;
    private int mChildHeight;

    public AverageRadioOptions(Context context, AttributeSet attrs) {
        super(context, attrs);
        // TODO Auto-generated constructor stub
        TypedArray a = context.getTheme().obtainStyledAttributes(
            attrs,
            R.styleable.RadioOptions,
            0, 0);
        int heightId = a.getResourceId(R.styleable.RadioOptions_option_height, 0);
        if (heightId > 0) {
            mOptionsHeight = context.getResources()
                .getDimension(heightId);
        }
        mIsHiddenOrHiding = true;
        mShouldShowModeOptionsToggle = true;
    }

    public void setViewToShowHide(View v) {
        mViewToShowHide = v;
    }

    public void setViewToOverlay(View v) {
        mViewToOverlay = v;
    }

    private boolean isOtherViewOverlay() {
        if (mViewToOverlay != null){
            View parent = (View) mViewToOverlay.getParent();
            if (parent != null && parent.getVisibility() == View.VISIBLE)
                return true;
        }
        return false;
    }

    public void setDirection(int direction) {
        mDirection = direction;
    }

    public void setMargin(float margin) {
        mMargin = mMarginLeft = mMarginTop = mMarginRight = mMarginBottom = margin;
    }

    /**
     * Sets a capture layout helper to query layout rect from.
     */
    public void setCaptureLayoutHelper(CaptureLayoutHelper helper) {
        mCaptureLayoutHelper = helper;
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mCaptureLayoutHelper == null) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            Log.e(TAG, "Capture layout helper needs to be set first.");
        } else {
            int childCount = getChildCount();
            int orientation = getContext().getResources().getConfiguration().orientation;
            final boolean isPortrait = Configuration.ORIENTATION_PORTRAIT == orientation;
            RectF uncoveredPreviewRect = mCaptureLayoutHelper.getUncoveredPreviewRect();
            if (uncoveredPreviewRect.width() <= 0
                    || uncoveredPreviewRect.height() <= 0) {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
                return;
            }
            int layout_height;
            int layout_width;
            int child_height;
            int child_width;
            if (isPortrait) {
                layout_width = (int) uncoveredPreviewRect.width();
                layout_height = (int) (mOptionsHeight > 0 ? mOptionsHeight : uncoveredPreviewRect.width()/childCount);
                child_width = (int) (layout_width / childCount);
                child_height = layout_height;
            } else {
                layout_width = (int) (mOptionsHeight > 0 ? mOptionsHeight : uncoveredPreviewRect.height()/childCount);
                layout_height = (int) uncoveredPreviewRect.height();
                child_width = layout_width;
                child_height = layout_height / childCount;
            }
            mChildWidth = child_width;
            mChildHeight = child_height;

            super.onMeasure(MeasureSpec.makeMeasureSpec(
                            layout_width, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(layout_height,
                            MeasureSpec.EXACTLY)
            );

            if (!CameraUtil.AUTO_ROTATE_SENSOR) {
                float offset = layout_width - (child_height - (mMargin + mMargin)) * childCount;
                if (offset < 0) {
                    mMarginTop = mMargin + Math.abs(offset / childCount) / 2.0f;
                    mMarginBottom = mMargin + Math.abs(offset / childCount) / 2.0f;
                }
            }

            for (int i = 0; i < childCount; i++) {
                getChildAt(i).measure(MeasureSpec.makeMeasureSpec((int) (child_width -(mMarginLeft + mMarginRight)), MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec((int) (child_height - (mMarginTop + mMarginBottom)), MeasureSpec.EXACTLY));
            }
        }
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        // TODO Auto-generated method stub
        if (changed) {
            mIsPortrait = (getResources().getConfiguration().orientation ==
                           Configuration.ORIENTATION_PORTRAIT);
            setupAnimators();
        }
        super.onLayout(changed, l, t, r, b);

        int childCount = getChildCount();
        int j = 0;
        for (int i = 0; i < childCount; i++) {
            if (getChildAt(i).getVisibility() != View.VISIBLE)
                continue;
            if (mIsPortrait) {
                getChildAt(i).layout((int) (mChildWidth * j + mMarginLeft),
                        (int) mMarginTop, (int) (mChildWidth * (j + 1) - mMarginRight), (int) (mChildHeight - mMarginBottom));
            } else {
                getChildAt(i).layout((int) mMarginLeft,
                        (int) (mChildHeight * j + mMarginTop), (int) (mChildWidth - mMarginRight), (int) (mChildHeight * (j + 1) - mMarginBottom));
            }
            j++;
        }
    }

    @Override
    public void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        if (visibility != VISIBLE && !mIsHiddenOrHiding) {
            // Collapse mode options when window is not visible.
            setVisibility(GONE);
            
            if (mViewToShowHide != null && mShouldShowModeOptionsToggle
                    && !isOtherViewOverlay()) {
                mViewToShowHide.setVisibility(VISIBLE);
            }
            mShouldShowModeOptionsToggle = true;
            mIsHiddenOrHiding = true;
        }
    }

    private void setupAnimators() {
        if (mVisibleAnimator != null) {
            mVisibleAnimator.end();
        }
        if (mHiddenAnimator != null) {
            mHiddenAnimator.end();
        }

        // show
        {
            final ValueAnimator alphaAnimator = ValueAnimator.ofFloat(0.0f, 1.0f);
            alphaAnimator.setDuration(SHOW_ANIMATION_TIME);
            alphaAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    setAlpha((Float) animation.getAnimatedValue());
                    invalidate();
                }
            });
            alphaAnimator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    setAlpha(1.0f);
                    invalidate();
                }
            });

            final ValueAnimator scaleAnimator = ValueAnimator.ofFloat(0.1f, 1.0f);
            scaleAnimator.setDuration(SHOW_ANIMATION_TIME);
            scaleAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {

                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    // TODO Auto-generated method stub
                    setScaleX((Float) animation.getAnimatedValue());
                    setScaleY((Float) animation.getAnimatedValue());
                    invalidate();
                }
            });
            scaleAnimator.addListener(new AnimatorListenerAdapter() {
                
                @Override
                public void onAnimationStart(Animator animation) {
                    // TODO Auto-generated method stub
                    
                }

                @Override
                public void onAnimationEnd(Animator animation) {
                    // TODO Auto-generated method stub
                    setScaleX(1.0f);
                    setScaleY(1.0f);
                    invalidate();
                }
            });

            mVisibleAnimator = new AnimatorSet();
            mVisibleAnimator.setInterpolator(Gusterpolator.INSTANCE);
            mVisibleAnimator.playTogether(alphaAnimator, scaleAnimator);
        }

        // hide
        {
            final ValueAnimator alphaAnimator = ValueAnimator.ofFloat(1.0f, 0.0f);
            alphaAnimator.setDuration(HIDE_ANIMATION_TIME);
            alphaAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    setAlpha((Float) animation.getAnimatedValue());
                    invalidate();
                }
            });
            alphaAnimator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    setVisibility(View.GONE);
                    setAlpha(0.0f);
                    invalidate();
                }
            });

            final ValueAnimator scaleAnimator = ValueAnimator.ofFloat(1.0f, 0.1f);
            scaleAnimator.setDuration(HIDE_ANIMATION_TIME);
            scaleAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {

                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    // TODO Auto-generated method stub
                    setScaleX((Float) animation.getAnimatedValue());
                    setScaleY((Float) animation.getAnimatedValue());
                    invalidate();
                }
            });
            scaleAnimator.addListener(new AnimatorListenerAdapter() {

                @Override
                public void onAnimationStart(Animator animation) {
                    // TODO Auto-generated method stub
                    
                }

                @Override
                public void onAnimationEnd(Animator animation) {
                    // TODO Auto-generated method stub
                    if (mViewToShowHide != null) {
                        if (mShouldShowModeOptionsToggle && !isOtherViewOverlay()) {
                            mViewToShowHide.setVisibility(View.VISIBLE);
                        }
                    }
                    mShouldShowModeOptionsToggle = true;
                    setVisibility(View.GONE);
                    setScaleX(0.1f);
                    setScaleY(0.1f);
                    invalidate();
                }
            });

            mHiddenAnimator = new AnimatorSet();
            mHiddenAnimator.setInterpolator(Gusterpolator.INSTANCE);
            mHiddenAnimator.playTogether(alphaAnimator, scaleAnimator);
        }
    }

    public void animateVisible() {
        if (mIsHiddenOrHiding) {
            if (mViewToShowHide != null) {
                mViewToShowHide.setVisibility(View.INVISIBLE);
            }
            mHiddenAnimator.cancel();
            mVisibleAnimator.end();
            setVisibility(View.VISIBLE);
            if (mDirection == LEFT_OR_TOP) {
                if (mIsPortrait) {
                    setPivotX(0);
                    setPivotY(getHeight());
                } else {
                    setPivotX(getWidth());
                    setPivotY(0);
                }
            } else {
                setPivotX(getWidth());
                setPivotY(getHeight());
            }
            mVisibleAnimator.start();
        }
        mIsHiddenOrHiding = false;
    }

    public void animateHidden(boolean showToggle) {
        if (!mIsHiddenOrHiding) {
            mVisibleAnimator.cancel();
            mHiddenAnimator.end();
            if (mDirection == LEFT_OR_TOP) {
                if (mIsPortrait) {
                    setPivotX(0);
                    setPivotY(getHeight());
                } else {
                    setPivotX(getWidth());
                    setPivotY(0);
                }
            } else {
                setPivotX(getWidth());
                setPivotY(getHeight());
            }
            mShouldShowModeOptionsToggle = showToggle;
            mHiddenAnimator.start();
        }
        mIsHiddenOrHiding = true;
    }

    public void updateUIByOrientation() {
        for (int i = 0; i < getChildCount(); i++) {
            getChildAt(i).setRotation(CameraUtil.mUIRotated);
        }
    }

}
