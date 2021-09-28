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
 * limitations under the License.
 */

package android.systemui.cts;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.CornerPathEffect;
import android.graphics.DashPathEffect;
import android.graphics.Insets;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;

import java.util.ArrayList;
import java.util.List;

public class WindowInsetsPresenterDrawable extends Drawable implements View.OnTouchListener {
    private static final int GAP = 15;
    private final Path mPath;
    private final Paint mMandatorySystemGesturePaint;
    private final Paint mSystemGesturePaint;
    private final Paint mSystemWindowPaint;
    private final Paint mTappableElementPaint;
    private final Paint mTouchPaint;
    private final Paint mTouchTrackPaint;
    private final List<Point> mAllActionPoints;
    private final List<Point> mActionCancelPoints;
    private final List<Point> mActionDownPoints;
    private final List<Point> mActionUpPoints;
    private WindowInsets mWindowInsets;
    private float mLastX;
    private float mLastY;

    /**
     * Initial 4 paints and 1 path for drawing.
     * initial mPointList for recording the touch points.
     */
    public WindowInsetsPresenterDrawable() {
        mMandatorySystemGesturePaint = new Paint();
        mMandatorySystemGesturePaint.setStrokeWidth(2);
        mMandatorySystemGesturePaint.setStyle(Paint.Style.STROKE);
        mMandatorySystemGesturePaint.setColor(Color.RED);
        mMandatorySystemGesturePaint.setPathEffect(new DashPathEffect(new float[]{GAP, GAP * 3},
                0));

        mSystemGesturePaint = new Paint();
        mSystemGesturePaint.setStrokeWidth(2);
        mSystemGesturePaint.setStyle(Paint.Style.STROKE);
        mSystemGesturePaint.setColor(Color.GREEN);
        mSystemGesturePaint.setPathEffect(new DashPathEffect(new float[]{GAP, GAP, GAP * 2},
                0));

        mTappableElementPaint = new Paint();
        mTappableElementPaint.setStrokeWidth(2);
        mTappableElementPaint.setStyle(Paint.Style.STROKE);
        mTappableElementPaint.setColor(Color.BLUE);
        mTappableElementPaint.setPathEffect(new DashPathEffect(new float[]{GAP * 2, GAP, GAP}, 0));

        mSystemWindowPaint = new Paint();
        mSystemWindowPaint.setStrokeWidth(2);
        mSystemWindowPaint.setStyle(Paint.Style.STROKE);
        mSystemWindowPaint.setColor(Color.YELLOW);
        mSystemWindowPaint.setPathEffect(new DashPathEffect(new float[]{0, GAP * 3, GAP}, 0));

        mTouchPaint = new Paint();
        mTouchPaint.setColor(0x4ffff070);
        mTouchPaint.setStrokeWidth(10);
        mTouchPaint.setStyle(Paint.Style.FILL_AND_STROKE);

        mPath = new Path();
        mTouchTrackPaint = new Paint();
        mTouchTrackPaint.setColor(Color.WHITE);
        mTouchTrackPaint.setStrokeWidth(1);
        mTouchTrackPaint.setStyle(Paint.Style.STROKE);
        mTouchTrackPaint.setPathEffect(new CornerPathEffect(5));

        mAllActionPoints = new ArrayList<>();
        mActionCancelPoints = new ArrayList<>();
        mActionDownPoints = new ArrayList<>();
        mActionUpPoints = new ArrayList<>();
    }


    public WindowInsetsPresenterDrawable(WindowInsets windowInsets) {
        this();
        mWindowInsets = windowInsets;
    }

    @Override
    public void setBounds(Rect bounds) {
        super.setBounds(bounds);
    }

    private void drawInset(Canvas canvas, Insets insets, Paint paint) {
        final Rect rect = getBounds();

        if (insets.left > 0) {
            canvas.drawRect(0, 0, insets.left, rect.bottom, paint);
        }
        if (insets.top > 0) {
            canvas.drawRect(0, 0, rect.width(), insets.top, paint);
        }
        if (insets.right > 0) {
            canvas.drawRect(rect.width() - insets.right, 0, rect.width(), rect.bottom,
                    paint);
        }
        if (insets.bottom > 0) {
            canvas.drawRect(0, rect.height() - insets.bottom, rect.width(), rect.height(),
                    paint);
        }
    }

    @Override
    public void draw(Canvas canvas) {
        canvas.save();

        if (mWindowInsets != null) {
            drawInset(canvas, mWindowInsets.getSystemGestureInsets(), mSystemGesturePaint);
            drawInset(canvas, mWindowInsets.getMandatorySystemGestureInsets(),
                    mMandatorySystemGesturePaint);
            drawInset(canvas, mWindowInsets.getTappableElementInsets(), mTappableElementPaint);
            drawInset(canvas, mWindowInsets.getSystemWindowInsets(), mSystemWindowPaint);
        }

        if (mAllActionPoints.size() > 0) {
            canvas.drawPath(mPath, mTouchTrackPaint);

            for (Point p : mAllActionPoints) {
                canvas.drawPoint(p.x, p.y, mTouchPaint);
            }
        }

        canvas.restore();
    }

    @Override
    public void setAlpha(int alpha) {
        /* do noting, only for implement abstract method */
    }

    @Override
    public void setColorFilter(ColorFilter colorFilter) {
        /* do noting, only for implement abstract method */
    }

    @Override
    public int getOpacity() {
        return 255; /* not transparent */
    }

    /**
     * To set the IwndowInsets that is showed on this drawable.
     * @param windowInsets the WindowInsets will show.
     */
    public void setWindowInsets(WindowInsets windowInsets) {
        if (windowInsets != null) {
            mWindowInsets = new WindowInsets.Builder(windowInsets).build();
            invalidateSelf();
        }
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        Point p;
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                mPath.moveTo(event.getX(), event.getY());
                p = new Point((int) event.getX(), (int) event.getY());
                mAllActionPoints.add(p);
                mActionDownPoints.add(p);
                mLastX = event.getX();
                mLastY = event.getY();
                invalidateSelf();

                break;
            case MotionEvent.ACTION_UP:
                mPath.lineTo(event.getX(), event.getY());
                p = new Point((int) event.getX(), (int) event.getY());
                mAllActionPoints.add(p);
                mActionUpPoints.add(p);
                invalidateSelf();
                break;
            case MotionEvent.ACTION_MOVE:
                mPath.quadTo(mLastX, mLastY, event.getX(), event.getY());
                p = new Point((int) event.getX(), (int) event.getY());
                mAllActionPoints.add(p);
                mLastX = event.getX();
                mLastY = event.getY();
                invalidateSelf();
                break;
            case MotionEvent.ACTION_CANCEL:
                mPath.lineTo(event.getX(), event.getY());
                p = new Point((int) event.getX(), (int) event.getY());
                mAllActionPoints.add(p);
                mActionCancelPoints.add(p);
                break;
            default:
                break;
        }
        return false;
    }

    public List<Point> getActionCancelPoints() {
        return mActionCancelPoints;
    }

    public List<Point> getActionDownPoints() {
        return mActionDownPoints;
    }

    public List<Point> getActionUpPoints() {
        return mActionUpPoints;
    }
}
