package com.android.camera.settings;

import android.content.Context;
import android.content.DialogInterface;
import android.preference.DialogPreference;
import android.util.AttributeSet;
import android.util.Log;

public class MyDialogPreference extends DialogPreference {
    private final String TAG = "MyDialogPreference";
    
    private DialogInterface.OnClickListener mPositiveButtonListener;

    public MyDialogPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // TODO Auto-generated constructor stub
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        // TODO Auto-generated method stub
        super.onClick(dialog, which);
        Log.i(TAG,"DialogInterface.OnClick");
        if (mPositiveButtonListener != null && which == DialogInterface.BUTTON_POSITIVE) {
            mPositiveButtonListener.onClick(dialog, which);
        }
    }

    public void setPositiveButtonListener(DialogInterface.OnClickListener listener) {
        mPositiveButtonListener = listener;
    }

}
