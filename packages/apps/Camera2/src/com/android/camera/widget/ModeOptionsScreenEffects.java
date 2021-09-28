package com.android.camera.widget;

import com.android.camera.debug.Log;
import com.android.camera.util.Gusterpolator;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

public class ModeOptionsScreenEffects extends LinearLayout {
    private final static Log.Tag TAG = new Log.Tag("ModeOptionsScreenEffects");

    private boolean mIsHiddenOrHiding;
    private AnimatorSet mVisibleAnimator;
    private AnimatorSet mHiddenAnimator;

    private static final int SHOW_ANIMATION_TIME = 350;
    private static final int HIDE_ANIMATION_TIME = 200;

    public ModeOptionsScreenEffects(Context context, AttributeSet attrs) {
        super(context, attrs);
        // TODO Auto-generated constructor stub
        mIsHiddenOrHiding = true;
        setupAnimators();
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        // TODO Auto-generated method stub
        super.onWindowVisibilityChanged(visibility);
        if (visibility != VISIBLE && !mIsHiddenOrHiding) {
            // Collapse mode options when window is not visible.
            setVisibility(GONE);
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
            mHiddenAnimator.cancel();
            mVisibleAnimator.end();
            setVisibility(View.VISIBLE);
//            setPivotX(getWidth() / 2.0f);
//            setPivotY(getHeight() / 2.0f);
            mVisibleAnimator.start();
        }
        mIsHiddenOrHiding = false;
    }

    public void animateHidden() {
        if (!mIsHiddenOrHiding) {
            mVisibleAnimator.cancel();
            mHiddenAnimator.end();
//            setPivotX(getWidth() / 2.0f);
//            setPivotY(getHeight() / 2.0f);
            mHiddenAnimator.start();
        }
        mIsHiddenOrHiding = true;
    }

}
