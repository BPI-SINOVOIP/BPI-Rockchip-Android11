package com.rockchip.graphics;

public class SaveBaseParameterUtil {

    static{
        System.loadLibrary("save_baseparameter_util");
    }

    public static native int outputImage(String path);

}
