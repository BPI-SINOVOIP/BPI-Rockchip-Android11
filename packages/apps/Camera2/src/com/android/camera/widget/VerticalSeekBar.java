package com.android.camera.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewParent;
import android.widget.SeekBar;

public class VerticalSeekBar extends SeekBar {
    private boolean mIsDragging;
    private float mTouchDownY;
    private int mScaledTouchSlop;
    private boolean isInScrollingContainer = false;

    private onViewHideListener mOnViewHideListener;
    public interface onViewHideListener {
        public void onViewHide();
    }

    public boolean isInScrollingContainer() {
        return isInScrollingContainer;
    }

    public void setInScrollingContainer(boolean isInScrollingContainer) {
        this.isInScrollingContainer = isInScrollingContainer;
    }

    /**
     * On touch, this offset plus the scaled value from the position of the
     * touch will form the progress value. Usually 0.
     */
    float mTouchProgressOffset;

    public VerticalSeekBar(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mScaledTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();

    }

    public VerticalSeekBar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public VerticalSeekBar(Context context) {
        super(context);
    }

    public void setOnViewHideListener(onViewHideListener listener) {
        mOnViewHideListener = listener;
    }

    @Override
    public void setVisibility(int v) {
        // TODO Auto-generated method stub
        if (v != View.VISIBLE) {
            if (mOnViewHideListener != null) {
                mOnViewHideListener.onViewHide();
            }
        }
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (h > w)
            super.onSizeChanged(h, w, oldh, oldw);
        else
            super.onSizeChanged(w, h, oldw, oldh);

    }

    @Override
    protected synchronized void onMeasure(int widthMeasureSpec,
            int heightMeasureSpec) {
        if (heightMeasureSpec > widthMeasureSpec) {
            super.onMeasure(heightMeasureSpec, widthMeasureSpec);
            setMeasuredDimension(getMeasuredHeight(), getMeasuredWidth());
        } else {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected synchronized void onDraw(Canvas canvas) {
        if (getHeight() > getWidth()) {
            canvas.rotate(-90);
            canvas.translate(-getHeight(), 0);
        }
        super.onDraw(canvas);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (getHeight() < getWidth())
            return super.onTouchEvent(event);
        
        if (!isEnabled()) {
            return false;
        }

        switch (event.getAction()) {
        case MotionEvent.ACTION_DOWN:
            if (isInScrollingContainer()) {

                mTouchDownY = event.getY();
            } else {
                setPressed(true);

                invalidate();
                onStartTrackingTouch();
                trackTouchEvent(event);
                attemptClaimDrag();

                onSizeChanged(getWidth(), getHeight(), 0, 0);
            }
            break;

        case MotionEvent.ACTION_MOVE:
            if (mIsDragging) {
                trackTouchEvent(event);

            } else {
                final float y = event.getY();
                if (Math.abs(y - mTouchDownY) > mScaledTouchSlop) {
                    setPressed(true);

                    invalidate();
                    onStartTrackingTouch();
                    trackTouchEvent(event);
                    attemptClaimDrag();

                }
            }
            onSizeChanged(getWidth(), getHeight(), 0, 0);
            break;

        case MotionEvent.ACTION_UP:
            if (mIsDragging) {
                trackTouchEvent(event);
                onStopTrackingTouch();
                setPressed(false);

            } else {
                // Touch up when we never crossed the touch slop threshold
                // should
                // be interpreted as a tap-seek to that location.
                onStartTrackingTouch();
                trackTouchEvent(event);
                onStopTrackingTouch();

            }
            onSizeChanged(getWidth(), getHeight(), 0, 0);
            // ProgressBar doesn't know to repaint the thumb drawable
            // in its inactive state when the touch stops (because the
            // value has not apparently changed)
            invalidate();
            break;
        }
        return true;

    }

    private void trackTouchEvent(MotionEvent event) {
        final int height = getHeight();
        final int top = getPaddingTop();
        final int bottom = getPaddingBottom();
        final int available = height - top - bottom;

        int y = (int) event.getY();

        float scale;
        float progress = 0;

        if (y > height - bottom) {
            scale = 0.0f;
        } else if (y < top) {
            scale = 1.0f;
        } else {
            scale = (float) (available - y + top) / (float) available;
            progress = mTouchProgressOffset;
        }

        final int max = getMax();
        progress += scale * max;

        setProgress((int) progress);

    }

    /**
     * This is called when the user has started touching this widget.
     */
    void onStartTrackingTouch() {
        mIsDragging = true;
    }

    /**
     * This is called when the user either releases his touch or the touch is
     * canceled.
     */
    void onStopTrackingTouch() {
        mIsDragging = false;
    }

    private void attemptClaimDrag() {
        ViewParent p = getParent();
        if (p != null) {
            p.requestDisallowInterceptTouchEvent(true);
        }
    }

    @Override
    public synchronized void setProgress(int progress) {

        super.setProgress(progress);
        if (getHeight() > getWidth())
            onSizeChanged(getWidth(), getHeight(), 0, 0);
    }

}
