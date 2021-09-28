package com.android.camera.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewDebug.ExportedProperty;
import android.widget.TextView;

public class AutoMarqueeTextView extends TextView {

    public AutoMarqueeTextView(Context context, AttributeSet attrs) {
        super(context, attrs);
        // TODO Auto-generated constructor stub
    }

    @Override
    @ExportedProperty(category = "focus")
    public boolean isFocused() {
        // TODO Auto-generated method stub
        return true;
    }

}
