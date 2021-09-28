package com.android.camera.ui;

import java.util.Locale;

import com.android.camera.debug.Log;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.content.Context;
import android.graphics.RectF;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.Toast;

import com.android.camera2.R;

public class BurstCountDownView extends FrameLayout {
    private static final Log.Tag TAG = new Log.Tag("CountDownView");

    private TextView mRemainingSecondsView;
    private TextProgressBar mBurstSavingProgress;
    private TextView mBurstSavingMessage;
    private float mBurstSavingMessageTopMargin;
    private int mRemainingSecs = -1;
    private final RectF mPreviewArea = new RectF();

    private final Handler mHandler = new Handler();

    public BurstCountDownView(Context context, AttributeSet attrs) {
        super(context, attrs);
        // TODO Auto-generated constructor stub
        mBurstSavingMessageTopMargin = context.getResources().getDimension(R.dimen.burst_saving_message_top_margin);
    }

    /**
     * Responds to preview area change by centering th countdown UI in the new
     * preview area.
     */
    public void onPreviewAreaChanged(RectF previewArea) {
        mPreviewArea.set(previewArea);
        int progressBarWidth = mBurstSavingProgress.getMeasuredWidth();
        int progressBarHeight = mBurstSavingProgress.getMeasuredHeight();
        mBurstSavingProgress.setTranslationX((int) (mPreviewArea.centerX() - progressBarWidth / 2.0f));
        mBurstSavingProgress.setTranslationY((int) (mPreviewArea.centerY() - progressBarHeight / 2.0f));
        int messageWidth = mBurstSavingMessage.getMeasuredWidth();
        int messageHeight = mBurstSavingMessage.getMeasuredHeight();
        mBurstSavingMessage.setTranslationX((int) (mPreviewArea.centerX() - messageWidth / 2.0f));
        mBurstSavingMessage.setTranslationY((int) (mPreviewArea.centerY() + progressBarHeight / 2.0f
                + mBurstSavingMessageTopMargin));
    }

    private void remainingSecondsChanged(int newVal) {
        mRemainingSecs = newVal;
        if (newVal < 0) {
            // Countdown has finished.
            if (mBurstSavingProgress.getProgress() == mBurstSavingProgress.getMax()) {
                mBurstSavingProgress.setVisibility(View.INVISIBLE);
                setVisibility(View.INVISIBLE);
            }
            mRemainingSecondsView.setVisibility(View.INVISIBLE);
        } else {
            if (mRemainingSecondsView.getVisibility() != View.VISIBLE) {
                mRemainingSecondsView.setVisibility(View.VISIBLE);
                mBurstSavingProgress.setVisibility(View.INVISIBLE);
            }

            if (newVal == 1) {
                mHandler.postDelayed(new Runnable() {
                    
                    @Override
                    public void run() {
                        // TODO Auto-generated method stub
                        remainingSecondsChanged(0);
                    }
                }, 200);
            } else if (newVal == 0) {
                mHandler.postDelayed(new Runnable() {
                    
                    @Override
                    public void run() {
                        // TODO Auto-generated method stub
                        remainingSecondsChanged(-1);
                    }
                }, 800);
            }
            Locale locale = getResources().getConfiguration().locale;
            String localizedValue = String.format(locale, "%d", newVal);
            mRemainingSecondsView.setText(localizedValue);
            startShow();
        }
    }

    private void startShow() {
        int textWidth = mRemainingSecondsView.getMeasuredWidth();
        int textHeight = mRemainingSecondsView.getMeasuredHeight();
        mRemainingSecondsView.setScaleX(1f);
        mRemainingSecondsView.setScaleY(1f);
        mRemainingSecondsView.setTranslationX(mPreviewArea.centerX() - textWidth / 2);
        mRemainingSecondsView.setTranslationY(mPreviewArea.centerY() - textHeight / 2);
        mRemainingSecondsView.setPivotX(textWidth / 2);
        mRemainingSecondsView.setPivotY(textHeight / 2);
        mRemainingSecondsView.setAlpha(1f);
        if (mRemainingSecs == 0) {
            float endScale = 0.4f;
            mRemainingSecondsView.animate().scaleX(endScale).scaleY(endScale)
                .alpha(0f).setDuration(800).start();
            //Toast.makeText(getContext(), R.string.pref_burst_saving, Toast.LENGTH_SHORT).show();
            setBurstSavingMessage(R.string.pref_burst_saving, false);
            mRemainingSecondsView.animate().setListener(new AnimatorListener() {

                @Override
                public void onAnimationStart(Animator animation) {
                    // TODO Auto-generated method stub
                }

                @Override
                public void onAnimationRepeat(Animator animation) {
                    // TODO Auto-generated method stub
                }

                @Override
                public void onAnimationEnd(Animator animation) {
                    // TODO Auto-generated method stub
                    Log.i(new Log.Tag("PhotoModule"), "show saving burst progress");
                    if (mBurstSavingProgress.getProgress() != mBurstSavingProgress.getMax())
                        mBurstSavingProgress.setVisibility(View.VISIBLE);
                    else
                        mBurstSavingProgress.setVisibility(View.INVISIBLE);
                }

                @Override
                public void onAnimationCancel(Animator animation) {
                    // TODO Auto-generated method stub
                }
            });
        }

        int progressBarWidth = mBurstSavingProgress.getMeasuredWidth();
        int progressBarHeight = mBurstSavingProgress.getMeasuredHeight();
        mBurstSavingProgress.setTranslationX((int) (mPreviewArea.centerX() - progressBarWidth / 2.0f));
        mBurstSavingProgress.setTranslationY((int) (mPreviewArea.centerY() - progressBarHeight / 2.0f));
        int messageWidth = mBurstSavingMessage.getMeasuredWidth();
        int messageHeight = mBurstSavingMessage.getMeasuredHeight();
        mBurstSavingMessage.setTranslationX((int) (mPreviewArea.centerX() - messageWidth / 2.0f));
        mBurstSavingMessage.setTranslationY((int) (mPreviewArea.centerY() + progressBarHeight / 2.0f
                + mBurstSavingMessageTopMargin));
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mRemainingSecondsView = (TextView) findViewById(R.id.burst_remaining_seconds);
        mBurstSavingProgress = (TextProgressBar) findViewById(R.id.burst_save_progress);
        mBurstSavingMessage = (TextView) findViewById(R.id.burst_save_message);
    }

    /**
     * Starts showing countdown in the UI.
     *
     * @param sec duration of the countdown, in seconds
     */
    public void setCountDown(int sec) {
        if (sec <= 0) {
            Log.w(TAG, "Invalid input for countdown: " + sec);
            return;
        }
        if (getVisibility() != View.VISIBLE)
            setVisibility(View.VISIBLE);
        remainingSecondsChanged(sec);
    }

    /**
     * Cancels the on-going countdown in the UI, if any.
     */
    public void cancelCountDown() {
        if (mRemainingSecs >= 0) {
            mRemainingSecs = -1;
            setVisibility(View.INVISIBLE);
        }
    }

    public void setBurstSavingProgressVisible(boolean visible) {
        if (visible) {
            if (getVisibility() != View.VISIBLE)
                setVisibility(View.VISIBLE);
            mBurstSavingProgress.setVisibility(View.VISIBLE);
            mBurstSavingMessage.setVisibility(View.VISIBLE);
        } else {
            mBurstSavingProgress.setVisibility(View.INVISIBLE);
            mBurstSavingMessage.setVisibility(View.INVISIBLE);
            if (getVisibility() == View.VISIBLE)
                setVisibility(View.INVISIBLE);
        }
    }

    public void setBurstSavingProgress(float progress) {
        int percentage = (int) (progress * mBurstSavingProgress.getMax());
        Log.i(new Log.Tag("PhotoModule"), "setBurstSavingProgress = " + percentage);
        mBurstSavingProgress.setProgress(percentage);
    }

    public void setBurstSavingMessage(int message, boolean delayhide) {
        int measureWidth = MeasureSpec.makeMeasureSpec(mBurstSavingMessage.
                getLayoutParams().width, MeasureSpec.AT_MOST);
        int measureHeight = MeasureSpec.makeMeasureSpec(mBurstSavingMessage.
                getLayoutParams().height, MeasureSpec.AT_MOST);
        if (delayhide) {
            mBurstSavingMessage.setText(message);
            mBurstSavingMessage.measure(measureWidth, measureHeight);
            int messageWidth = mBurstSavingMessage.getMeasuredWidth();
            int messageHeight = mBurstSavingMessage.getMeasuredHeight();
            mBurstSavingMessage.setTranslationX((int) (mPreviewArea.centerX() - messageWidth / 2.0f));
            mBurstSavingMessage.setTranslationY((int) (mPreviewArea.centerY() +
                    mBurstSavingProgress.getMeasuredHeight() / 2.0f
                    + mBurstSavingMessageTopMargin));
            mHandler.postDelayed(mMessageDelayHide, 1000);
        } else {
            mHandler.removeCallbacks(mMessageDelayHide);
            mBurstSavingMessage.setVisibility(View.VISIBLE);
            mBurstSavingMessage.setText(message);
            mBurstSavingMessage.measure(measureWidth, measureHeight);
        }
    }

    private Runnable mMessageDelayHide = new Runnable() {

        @Override
        public void run() {
            // TODO Auto-generated method stub
            mBurstSavingMessage.setVisibility(View.INVISIBLE);
        }
    };
}
