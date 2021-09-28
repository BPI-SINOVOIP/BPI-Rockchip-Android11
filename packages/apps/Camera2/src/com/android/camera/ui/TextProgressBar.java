package com.android.camera.ui;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.widget.ProgressBar;

import com.android.camera.debug.Log;
import com.android.camera2.R;

public class TextProgressBar extends ProgressBar {
    private String mText;
    private Paint mPaint;

    public TextProgressBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        // TODO Auto-generated constructor stub
        mPaint = new Paint();
        mPaint.setColor(Color.WHITE);
        mPaint.setTextSize(getContext().getResources().
                getDimension(R.dimen.progress_bar_text_size));
    }

    @Override
    public synchronized void setProgress(int progress) {
        // TODO Auto-generated method stub
        super.setProgress(progress);
        mText = progress + "%";
    }

    @Override
    protected synchronized void onDraw(Canvas canvas) {
        // TODO Auto-generated method stub
        super.onDraw(canvas);
        Rect rect = new Rect();
        mPaint.getTextBounds(mText, 0, mText.length(), rect);
        Log.i(new Log.Tag("PhotoModule"), "progress text size = " + rect.toString());
        int x = (getWidth() / 2) - rect.centerX();
        int y = (getHeight() / 2) - rect.centerY(); 
        canvas.drawText(mText, x, y, this.mPaint);
    }

}
