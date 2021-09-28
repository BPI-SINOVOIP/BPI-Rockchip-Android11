package com.android.camera.ui;

import com.android.camera.debug.Log;
import com.android.camera.debug.Log.Tag;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

public class ModeOptionsToggle extends LinearLayout {
    private final static Tag TAG = new Log.Tag("ModeOptionsToggle");

    public interface OnVisibilityOrLayoutChangedListener {
        void onLayoutChanged();
        void onVisibilityChanged();
    }
    private OnVisibilityOrLayoutChangedListener mListener;

    public void setOnVisibilityOrLayoutChangedListener(OnVisibilityOrLayoutChangedListener listener) {
        mListener = listener;
    }

    public ModeOptionsToggle(Context context, AttributeSet attrs) {
        super(context, attrs);
        // TODO Auto-generated constructor stub
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        // TODO Auto-generated method stub
        super.onLayout(changed, l, t, r, b);
        if (mListener != null)
            mListener.onLayoutChanged();
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        // TODO Auto-generated method stub
        super.onVisibilityChanged(changedView, visibility);
        if (mListener != null)
            mListener.onVisibilityChanged();
    }

}
