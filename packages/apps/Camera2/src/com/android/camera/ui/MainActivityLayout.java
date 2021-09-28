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

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Rect;
import android.provider.MediaStore;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.View.MeasureSpec;
import android.widget.FrameLayout;

import com.android.camera.app.CameraAppUI;
import com.android.camera.debug.Log;
import com.android.camera.util.CameraUtil;
import com.android.camera.widget.FilmstripLayout;
import com.android.camera.widget.ModeOptionsOverlay;
import com.android.camera2.R;

public class MainActivityLayout extends FrameLayout {

    private final Log.Tag TAG = new Log.Tag("MainActivityLayout");
    // Only check for intercepting touch events within first 500ms
    private static final int SWIPE_TIME_OUT = 500;

    private ModeListView mModeList;
    private FilmstripLayout mFilmstripLayout;
    private TopRightWeightedLayout mModeOptionsButtons;
    private ModeOptionsOverlay mModeOptionsOverlay;
    private boolean mCheckToIntercept;
    private MotionEvent mDown;
    private final int mSlop;
    private boolean mRequestToInterceptTouchEvents = false;
    private View mTouchReceiver = null;
    private final boolean mIsCaptureIntent;
    private CameraAppUI.NonDecorWindowSizeChangedListener mNonDecorWindowSizeChangedListener = null;

    private int mWidth;
    private int mHeight;

    // TODO: This can be removed once we come up with a new design for b/13751653.
    @Deprecated
    private boolean mSwipeEnabled = true;

    public MainActivityLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mSlop = ViewConfiguration.get(context).getScaledTouchSlop();

        Activity activity = (Activity) context;
        Intent intent = activity.getIntent();
        String action = intent.getAction();
        mIsCaptureIntent = (MediaStore.ACTION_IMAGE_CAPTURE.equals(action)
                || MediaStore.ACTION_IMAGE_CAPTURE_SECURE.equals(action)
                || MediaStore.ACTION_VIDEO_CAPTURE.equals(action));
    }

    /**
     * Enables or disables the swipe for modules not supporting the new swipe
     * logic yet.
     */
    @Deprecated
    public void setSwipeEnabled(boolean enabled) {
        mSwipeEnabled = enabled;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mCheckToIntercept = true;
            mDown = MotionEvent.obtain(ev);
            mTouchReceiver = null;
            mRequestToInterceptTouchEvents = false;
            return false;
        } else if (mRequestToInterceptTouchEvents) {
            mRequestToInterceptTouchEvents = false;
            onTouchEvent(mDown);
            return true;
        } else if (ev.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN) {
            // Do not intercept touch once child is in zoom mode
            mCheckToIntercept = false;
            return false;
        } else {
            // TODO: This can be removed once we come up with a new design for b/13751653.
            if (!mCheckToIntercept) {
                return false;
            }
            if (ev.getEventTime() - ev.getDownTime() > SWIPE_TIME_OUT) {
                return false;
            }
            if (mIsCaptureIntent || !mSwipeEnabled) {
                return false;
            }
            int deltaX = (int) (ev.getX() - mDown.getX());
            int deltaY = (int) (ev.getY() - mDown.getY());
            if (!CameraUtil.AUTO_ROTATE_SENSOR) {
                if (CameraUtil.mScreenOrientation
                        == ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE) {
                    deltaX = (int) (ev.getY() - mDown.getY());
                    deltaY = (int) (mDown.getX() - ev.getX());
                } else if (CameraUtil.mScreenOrientation
                        == ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT) {
                    deltaX = (int) (mDown.getX() - ev.getX());
                    deltaY = (int) (mDown.getY() - ev.getY());
                } else if (CameraUtil.mScreenOrientation
                        == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE) {
                    deltaX = (int) (mDown.getY() - ev.getY());
                    deltaY = (int) (ev.getX() - mDown.getX());
                }
            }
            if (ev.getActionMasked() == MotionEvent.ACTION_MOVE
                    && Math.abs(deltaX) > mSlop) {
                // Intercept right/left by ModeOptionsButton
                if (mModeOptionsButtons.isScrollable() 
                        && Math.abs(deltaX) >= Math.abs(deltaY) * 2
                        && mModeOptionsButtons.getVisibility() == View.VISIBLE) {
                    Rect r = new Rect();
                    mModeOptionsButtons.getGlobalVisibleRect(r);
                    if (r.contains((int) mDown.getX(), (int) mDown.getY())){
                        Log.i(TAG, "Intercept right/left by ModeOptionsButton");
                        mTouchReceiver = mModeOptionsButtons;
                        onTouchEvent(mDown);
                        mModeOptionsOverlay.closeManualFocusBar();
                        return true;
                    }
                }
                // Intercept right swipe
                if (deltaX >= Math.abs(deltaY) * 2) {
                    Log.i(TAG, "Intercept right swipe");
                    mTouchReceiver = mModeList;
                    onTouchEvent(mDown);
                    mModeOptionsOverlay.closeManualFocusBar();
                    return true;
                }
                // Intercept left swipe
                else if (deltaX < -Math.abs(deltaY) * 2) {
                    Log.i(TAG, "Intercept left swipe");
                    mTouchReceiver = mFilmstripLayout;
                    onTouchEvent(mDown);
                    mModeOptionsOverlay.closeManualFocusBar();
                    return true;
                }
            }
            if (ev.getActionMasked() == MotionEvent.ACTION_MOVE
                    && Math.abs(deltaY) > mSlop) {
                // Intercept up/down by ModeOptionsButton
                if (mModeOptionsButtons.isScrollable() 
                        && Math.abs(deltaY) >= Math.abs(deltaX) * 2
                        && mModeOptionsButtons.getVisibility() == View.VISIBLE) {
                    Rect r = new Rect();
                    mModeOptionsButtons.getGlobalVisibleRect(r);
                    if (r.contains((int) mDown.getX(), (int) mDown.getY())){
                        Log.i(TAG, "Intercept up/down by ModeOptionsButton");
                        mTouchReceiver = mModeOptionsButtons;
                        onTouchEvent(mDown);
                        mModeOptionsOverlay.closeManualFocusBar();
                        return true;
                    }
                }
            }
        }
        return false;
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        if (ev.getActionMasked() == MotionEvent.ACTION_UP) {
            if (mTouchUpListener != null)
                mTouchUpListener.onTouch(null, ev);
        }
        if (mTouchReceiver != null) {
            mTouchReceiver.setVisibility(VISIBLE);
            if (!CameraUtil.AUTO_ROTATE_SENSOR) {
                if (mTouchReceiver == mModeList || mTouchReceiver == mFilmstripLayout) {
                    if (CameraUtil.mScreenOrientation
                            == ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE) {
                        ev.setLocation(ev.getY(), mWidth - ev.getX());
                    } else if (CameraUtil.mScreenOrientation
                            == ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT) {
                        ev.setLocation(mWidth - ev.getX(), mHeight - ev.getY());
                    } else if (CameraUtil.mScreenOrientation
                            == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE) {
                        ev.setLocation(mHeight - ev.getY(), ev.getX());
                    }
                }
            }
            return mTouchReceiver.dispatchTouchEvent(ev);
        }
        return false;
    }

    private View.OnTouchListener mTouchUpListener;
    public void setTouchUpListener(View.OnTouchListener listener) {
        mTouchUpListener = listener;
    }

    @Override
    public void onFinishInflate() {
        mModeList = (ModeListView) findViewById(R.id.mode_list_layout);
        mFilmstripLayout = (FilmstripLayout) findViewById(R.id.filmstrip_layout);
        mModeOptionsButtons = (TopRightWeightedLayout) findViewById(R.id.mode_options_buttons);
        mModeOptionsOverlay
            = (ModeOptionsOverlay) findViewById(R.id.mode_options_overlay);
    }

    public void redirectTouchEventsTo(View touchReceiver) {
        if (touchReceiver == null) {
            Log.e(TAG, "Cannot redirect touch to a null receiver.");
            return;
        }
        mTouchReceiver = touchReceiver;
        mRequestToInterceptTouchEvents = true;
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mNonDecorWindowSizeChangedListener != null) {
            mNonDecorWindowSizeChangedListener.onNonDecorWindowSizeChanged(
                    MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.getSize(heightMeasureSpec),
                    CameraUtil.getDisplayRotation());
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        mWidth = MeasureSpec.getSize(widthMeasureSpec);
        mHeight = MeasureSpec.getSize(heightMeasureSpec);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right,
            int bottom) {
        // TODO Auto-generated method stub
        super.onLayout(changed, left, top, right, bottom);
        if (!CameraUtil.AUTO_ROTATE_SENSOR && mModeList != null) {
            updateModeListViewByOrientation();
        }
    }

    /**
     * Sets a listener that gets notified when the layout size is changed. This
     * size is the size of main activity layout. It is also the size of the window
     * excluding the system decor such as status bar and nav bar.
     */
    public void setNonDecorWindowSizeChangedListener(
            CameraAppUI.NonDecorWindowSizeChangedListener listener) {
        mNonDecorWindowSizeChangedListener = listener;
        if (mNonDecorWindowSizeChangedListener != null) {
            mNonDecorWindowSizeChangedListener.onNonDecorWindowSizeChanged(
                    getMeasuredWidth(), getMeasuredHeight(),
                    CameraUtil.getDisplayRotation());
        }
    }

    private void updateModeListViewByOrientation() {
        int rootViewWidth = getWidth();
        int rootViewHeight = getHeight();
        if (rootViewWidth > rootViewHeight) {
            int tmp = rootViewWidth;
            rootViewWidth = rootViewHeight;
            rootViewHeight = tmp;
        }
        if (CameraUtil.mIsPortrait) {
            mModeList.measure(MeasureSpec.makeMeasureSpec(rootViewWidth, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(rootViewHeight, MeasureSpec.EXACTLY));
            mModeList.layout(0, 0, rootViewWidth, rootViewHeight);
            mModeList.setTranslationX(0);
            mModeList.setTranslationY(0);
        } else {
            mModeList.measure(MeasureSpec.makeMeasureSpec(rootViewHeight, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(rootViewWidth, MeasureSpec.EXACTLY));
            mModeList.layout(0, 0, rootViewHeight, rootViewWidth);
            mModeList.setTranslationX((rootViewWidth - rootViewHeight) / 2.0f);
            mModeList.setTranslationY((rootViewHeight - rootViewWidth) / 2.0f);
        }
        mModeList.setRotation(CameraUtil.mUIRotated);
    }

}