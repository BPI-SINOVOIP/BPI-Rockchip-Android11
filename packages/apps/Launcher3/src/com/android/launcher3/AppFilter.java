package com.android.launcher3;

import android.content.ComponentName;
import android.content.Context;

import com.android.launcher3.util.ResourceBasedOverride;

public class AppFilter implements ResourceBasedOverride {
    private static boolean mIsEinkProduct = Utilities.isEinkProduct();
    private final String[] EINK_BLACKLIST = new String[] {
        "org.chromium.webview_shell",
    };

    public static AppFilter newInstance(Context context) {
        return Overrides.getObject(AppFilter.class, context, R.string.app_filter_class);
    }

    public boolean shouldShowApp(ComponentName app) {
        if (null != app && mIsEinkProduct) {
            String packageName = app.getPackageName();
            String className = app.getClassName();
            for (String temp : EINK_BLACKLIST) {
                if (temp.equals(packageName)) {
                    return false;
                }
            }
        }
        return true;
    }
}
