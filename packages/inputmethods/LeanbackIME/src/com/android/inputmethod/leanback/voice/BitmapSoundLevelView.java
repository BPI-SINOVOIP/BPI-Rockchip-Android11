/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.inputmethod.leanback.voice;

import com.android.inputmethod.leanback.R;

import android.animation.ObjectAnimator;
import android.animation.TimeAnimator;
import android.animation.TimeAnimator.TimeListener;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;

/**
 * Displays the recording value of the microphone.
 */
public class BitmapSoundLevelView extends View {
    private static final boolean DEBUG = false;
    private static final String TAG = "BitmapSoundLevelsView";

    private static final int MIC_PRIMARY_LEVEL_IMAGE_OFFSET = 3;
    private static final int MIC_LEVEL_GUIDELINE_OFFSET = 13;

    private final Paint mEmptyPaint = new Paint();
    private Rect mDestRect;

    private final int mEnableBackgroundColor;
    private final int mDisableBackgroundColor;

    // Generates clock ticks for the animation using the global animation loop.
    private TimeAnimator mAnimator;

    private int mCurrentVolume;

    // Bitmap for the main level meter, most closely follows the mic.
    private final Bitmap mPrimaryLevel;

    // Bitmap for trailing level meter, shows a peak level.
    private final Bitmap mTrailLevel;

    // The minimum size of the levels, that is the size when volume is 0.
    private final int mMinimumLevelSize;

    // A translation to apply to the center of the levels, allows the levels to be offset from
    // the center of the mView without having to translate the whole mView.
    private final int mCenterTranslationX;
    private final int mCenterTranslationY;

    // Peak level observed, and how many frames left before it starts decaying.
    private int mPeakLevel;
    private int mPeakLevelCountDown;

    // Input level is pulled from here.
    private SpeechLevelSource mLevelSource;

    private Paint mPaint;

    public BitmapSoundLevelView(Context context) {
        this(context, null);
    }

    public BitmapSoundLevelView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public BitmapSoundLevelView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.BitmapSoundLevelView,
                defStyleAttr, 0);
        mEnableBackgroundColor = a.getColor(R.styleable.BitmapSoundLevelView_enabledBackgroundColor,
                Color.parseColor("#66FFFFFF"));

        mDisableBackgroundColor = a.getColor(
                R.styleable.BitmapSoundLevelView_disabledBackgroundColor,
                Color.WHITE);

        boolean primaryLevelEnabled = false;
        boolean peakLevelEnabled = false;
        int primaryLevelId = 0;
        if (a.hasValue(R.styleable.BitmapSoundLevelView_primaryLevels)) {
            primaryLevelId = a.getResourceId(
                    R.styleable.BitmapSoundLevelView_primaryLevels, R.drawable.vs_reactive_dark);
            primaryLevelEnabled = true;
        }

        int trailLevelId = 0;
        if (a.hasValue(R.styleable.BitmapSoundLevelView_trailLevels)) {
            trailLevelId = a.getResourceId(
                    R.styleable.BitmapSoundLevelView_trailLevels, R.drawable.vs_reactive_light);
            peakLevelEnabled = true;
        }

        mCenterTranslationX = a.getDimensionPixelOffset(
                R.styleable.BitmapSoundLevelView_levelsCenterX, 0);

        mCenterTranslationY = a.getDimensionPixelOffset(
                R.styleable.BitmapSoundLevelView_levelsCenterY, 0);

        mMinimumLevelSize = a.getDimensionPixelOffset(
                R.styleable.BitmapSoundLevelView_minLevelRadius, 0);

        a.recycle();

        if (primaryLevelEnabled) {
            mPrimaryLevel = BitmapFactory.decodeResource(getResources(), primaryLevelId);
        } else {
            mPrimaryLevel = null;
        }

        if (peakLevelEnabled) {
            mTrailLevel = BitmapFactory.decodeResource(getResources(), trailLevelId);
        } else {
            mTrailLevel = null;
        }

        mPaint = new Paint();

        mDestRect = new Rect();

        mEmptyPaint.setFilterBitmap(true);

        // Safe source, replaced with system one when attached.
        mLevelSource = new SpeechLevelSource();
        mLevelSource.setSpeechLevel(0);

        // This animator generates ticks that invalidate the
        // mView so that the animation is synced with the global animation loop.
        mAnimator = new TimeAnimator();
        mAnimator.setRepeatCount(ObjectAnimator.INFINITE);
        mAnimator.setTimeListener(new TimeListener() {
            @Override
            public void onTimeUpdate(TimeAnimator animation, long totalTime, long deltaTime) {
                invalidate();
            }
        });
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        updateAnimatorState();
    }

    private void updateAnimatorState() {
        if (isEnabled()) {
            startAnimator();
        } else {
            stopAnimator();
        }
    }

    private void startAnimator() {
        if (DEBUG) Log.d(TAG, "startAnimator()");
        if (!mAnimator.isStarted()) {
            mAnimator.start();
        }
    }

    private void stopAnimator() {
        if (DEBUG) Log.d(TAG, "stopAnimator()");
        mAnimator.cancel();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        updateAnimatorState();
    }

    @Override
    protected void onDetachedFromWindow() {
        stopAnimator();
        super.onDetachedFromWindow();
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (hasWindowFocus) {
            updateAnimatorState();
        } else {
            stopAnimator();
        }
    }

    public void setLevelSource(SpeechLevelSource source) {
        if (DEBUG) {
            Log.d(TAG, "Speech source set");
        }
        mLevelSource = source;
    }

    @Override
    public void onDraw(Canvas canvas) {
        if (isEnabled()) {
            canvas.drawColor(mEnableBackgroundColor);

            int level = mLevelSource.getSpeechLevel();

            // Set the peak level for the trailing circle, goes to a peak, waits there for
            // some frames, then starts to decay.
            if (level > mPeakLevel) {
                mPeakLevel = level;
                mPeakLevelCountDown = 25;
            } else {
                if (mPeakLevelCountDown == 0) {
                    mPeakLevel = Math.max(0, mPeakLevel - 2);
                } else {
                    mPeakLevelCountDown--;
                }
            }

            // Either ease towards the target level, or decay away from it depending on whether
            // its higher or lower than the current.
            if (level > mCurrentVolume) {
                mCurrentVolume = mCurrentVolume + ((level - mCurrentVolume) / 4);
            } else {
                mCurrentVolume = (int) (mCurrentVolume * 0.95f);
            }

            int centerX = mCenterTranslationX + (getWidth() / 2);
            int centerY = mCenterTranslationY + (getWidth() / 2);
            if (mTrailLevel != null) {
                int size = ((centerX - mMinimumLevelSize) * mPeakLevel) / 100 + mMinimumLevelSize;

                mDestRect.set(
                        centerX - size,
                        centerY - size,
                        centerX + size,
                        centerY + size);
                canvas.drawBitmap(mTrailLevel, null, mDestRect, mEmptyPaint);
            }

            if (mPrimaryLevel != null) {
                int size =
                        ((centerX - mMinimumLevelSize) * mCurrentVolume) / 100 + mMinimumLevelSize;

                mDestRect.set(
                        centerX - size,
                        centerY - size,
                        centerX + size,
                        centerY + size);
                canvas.drawBitmap(mPrimaryLevel, null, mDestRect, mEmptyPaint);
                mPaint.setColor(getResources().getColor(R.color.search_mic_background));
                mPaint.setStyle(Paint.Style.FILL);
                canvas.drawCircle(centerX, centerY, mMinimumLevelSize -
                        MIC_PRIMARY_LEVEL_IMAGE_OFFSET, mPaint);
            }
            if(mTrailLevel != null && mPrimaryLevel != null) {
                mPaint.setColor(getResources().getColor(R.color.search_mic_levels_guideline));
                mPaint.setStyle(Paint.Style.STROKE);
                canvas.drawCircle(centerX, centerY, centerX - MIC_LEVEL_GUIDELINE_OFFSET, mPaint);
            }
        } else {
            canvas.drawColor(mDisableBackgroundColor);
        }
    }
}
