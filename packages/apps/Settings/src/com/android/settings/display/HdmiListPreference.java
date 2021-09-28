package com.android.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.ListPreference;

public class HdmiListPreference extends ListPreference {
    public HdmiListPreference(Context context) {
        this(context, null);
    }

    public HdmiListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onClick() {
        //super.onClick();
    }

    public void showClickDialogItem() {
        super.onClick();
    }
}
