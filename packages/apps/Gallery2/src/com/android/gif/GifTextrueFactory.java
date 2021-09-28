/************************************************************
 * Copyright ${year} Rockchip Mobile Comm Corp., Ltd.
 * All rights reserved.
 * FileName      : MediaItemTextureFactory.java
 * Version Number : 1.0
 * Description    : This factory can  return origin MediaItemTexture or
 *                GifTexture
 * Author        : caijq
 * Date          : 2011-04-19
 * History        :
 * 
 ************************************************************/
package com.android.gif;

import android.util.Log;

public class GifTextrueFactory {
    private static GifTextrue sActiveGif;
    private static final String TAG = "GifTextrueFactory";
    private static Object mLock = new Object();

    public static void freezeAllGif() {
        synchronized (mLock) {
            if (sActiveGif != null) {
                sActiveGif.stopAnimate();
            }
            sActiveGif = null;
            Log.i(TAG, "freezeAllGif");
        }

    }

    public static void startOnly(GifTextrue g) {
        synchronized (mLock) {
            if (g != sActiveGif) {
                if (sActiveGif != null) {
                    sActiveGif.stopAnimate();
                }
                g.startAnimate();
                sActiveGif = g;

            }
            Log.i(TAG, "startOnly");
        }

    }

    public static GifTextrue getGifTextrue() {
        synchronized (mLock) {
            return sActiveGif;
        }
    }
}
