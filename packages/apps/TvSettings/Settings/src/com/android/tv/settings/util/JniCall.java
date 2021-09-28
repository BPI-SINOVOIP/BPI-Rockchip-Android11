package com.android.tv.settings.util;

import android.util.Log;

public class JniCall {
    static {
        Log.i("wttt" ,"JNICALL init");
        System.loadLibrary("tvsettings-jni");
    }

    //public static native boolean test();
    //最大亮度，最小亮度曲线
    public static native int[] get(double x, double y);

    //亮度，饱和度曲线
    public static native int[] getOther(double x, double y);

    //电视是否支持HDR
    public static native boolean isSupportHDR();

    //设置电视HDR是否可用
    public static native void setHDREnable(int enable);

    //HDR->SDR曲线1
    public static native int[] getEetf(float maxDst, float minDst);

    //HDR->SDR曲线2
    public static native int[] getOetf(float maxDst, float minDst);

    //HDR->SDR最大，最小亮度生成
    public static native int[] getMaxMin(float maxDst, float minDst);
}
