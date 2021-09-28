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

package android.uirendering.cts.testclasses.view;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

/**
 * Test View used to verify a View's custom alpha implementation logic
 * in conjunction with {@link android.view.ViewPropertyAnimator}
 */
public class AlphaTestView extends View {

    private Paint mPaint = new Paint();
    private int mStartColor = Color.RED;
    private int mEndColor = Color.BLUE;

    public AlphaTestView(Context context) {
        super(context);
    }

    public AlphaTestView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public AlphaTestView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public AlphaTestView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
    }

    public void setStartColor(int startColor) {
        mStartColor = startColor;
        mPaint.setColor(mStartColor);
    }

    public void setEndColor(int endColor) {
        mEndColor = endColor;
    }

    @Override
    protected boolean onSetAlpha(int alpha) {
        mPaint.setColor(blendColor(mStartColor, mEndColor, 1.0f - alpha / 255.0f));
        return true;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        canvas.drawRect(getLeft(), getTop(), getRight(), getBottom(), mPaint);
    }

    public int getBlendedColor() {
        return mPaint.getColor();
    }

    private int blendColor(int color1, int color2, float ratio) {
        float inverseRatio = 1 - ratio;
        float a = (float) Color.alpha(color1) * inverseRatio + (float) Color.alpha(color2) * ratio;
        float r = (float) Color.red(color1) * inverseRatio + (float) Color.red(color2) * ratio;
        float g = (float) Color.green(color1) * inverseRatio + (float) Color.green(color2) * ratio;
        float b = (float) Color.blue(color1) * inverseRatio + (float) Color.blue(color2) * ratio;
        return Color.argb((int) a, (int) r, (int) g, (int) b);
    }
}
