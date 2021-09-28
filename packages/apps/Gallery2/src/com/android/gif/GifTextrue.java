/************************************************************
 * Copyright ${year} Rockchip MID Comm Corp., Ltd.
 * All rights reserved.
 * FileName      : GifTexture.java
 * Version Number : 1.0
 * Description    : extends BasicTexture to support gif animation
 * Author        : jyzheng
 * Date          : 2012-02-25
 * 
 ************************************************************/
package com.android.gif;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import com.android.gallery3d.glrenderer.BitmapTexture;
import com.android.gallery3d.ui.TileImageView;

// TODO single instance

public class GifTextrue{
    /*
     * rockchips jyzheng Add begin for [support uri gif] 2012-02-25
     */
    private final boolean DEBUG = true;
    private void LOG(String str){
        if(DEBUG){
            Log.v(TAG, str);
        }
    }

    public static boolean isGifStream(InputStream is) {
        String id = "";
        for (int i = 0; i < 6; i++) {
            try {
                id += (char) is.read();
            } catch (IOException e) {

                e.printStackTrace();
                return false;
            }catch(NullPointerException e){
            //  e.printStackTrace();
                return false;
            }
        }
        return id.startsWith("GIF");

    }

    public InputStream getInputStream() {
//      Log.v("GifTextrue", "=====================mFilePath:"+mFilePath);
        try {
            if (null != mUri && null != mContext) {
                return mContext.getContentResolver().openInputStream(mUri);
            }
            return new FileInputStream(new File(mFilePath));
        } catch (FileNotFoundException e) {
            e.printStackTrace();
            return null;
        }
    }

    /* rockchips jyzheng Add end */
    private class GifHandler extends Handler {
        private static final long MIN_DELAY = 50;
        public static final int START = 0;
        public static final int STOP = 2;
        private static final int UPDATE = 1;
        private GifDecoder mGifDecoder = null;

        public GifHandler(Looper looper) {
            super(looper);
        }

        public void handleMessage(Message msg) {
            switch (msg.what) {
            case START:
                InputStream is = getInputStream();
                if (is == null) {
                    Log.e(TAG, "getInputStream returns null");
                    break;
                }
                final boolean[] decodingOK = new boolean[] { true };
                mGifDecoder = new GifDecoder(is, new GifAction() {
                    public void parseOk(boolean parseStatus, int frameIndex) {
                        if (!parseStatus) {
                            Log.e(TAG, "parse gif with error");
                            decodingOK[0] = false;
                        }
                    }
                });
                try{
                   mGifDecoder.run();
                }catch(OutOfMemoryError e){
                    e.printStackTrace();
                    System.gc();
                }

                if (decodingOK[0]) {
                    sendEmptyMessage(UPDATE);
                }

                break;
            case UPDATE:
                GifDecoder decoder = mGifDecoder;
                GifFrame frame = decoder.next();
                if (frame != null && frame.image != null) {
                    mBitmap = frame.image;
                    long delay = MIN_DELAY;
                    if (frame.delay > delay) {
                        delay = frame.delay;
                    }
                    mTileImageView.invalidate();
                    sendEmptyMessageDelayed(UPDATE, delay);
                } else {
                    sendEmptyMessageDelayed(UPDATE, MIN_DELAY);
                }
                break;
            case STOP:
                /** suppose start is called */
                if(mGifDecoder != null){
                   mGifDecoder.free();
                }
                mGifDecoder = null;
                /** looper.quit will be called by thread.quit */
                // this.getLooper().quit();
                Runnable r = (Runnable) msg.obj;
                r.run();
                break;

            }
        }
    }

    /** enable this to dump all frames of gif to jpg files */

    private static final String TAG = "GifTextrue";
    private GifHandler mHandler;
    private String mFilePath;
    private Uri mUri;
    private Context mContext;

    /** the animation will start when decoder is ready */

    private HandlerThread mThread;
    private TileImageView mTileImageView;
    private Bitmap mBitmap;
    private BitmapTexture mBitmapTexture = null;
    public int mImageWidth = 0;
    public int mImageHeight = 0;
    
    public BitmapTexture getBitmapTexture(){
        if(mBitmapTexture == null || mBitmapTexture.getBitmap() != mBitmap){
            if(mBitmap != null && !mBitmap.isRecycled()){
               if(mBitmapTexture != null && mBitmapTexture.mBitmap != null){
                   mBitmapTexture.mBitmap.recycle();
                   mBitmapTexture.mBitmap= null;
               }
               if(mBitmapTexture != null){
                  mBitmapTexture.recycle();
               }
               mBitmapTexture = null;
               mBitmapTexture = new BitmapTexture(mBitmap);
            }
        }
        return mBitmapTexture;
    }

    public GifTextrue(TileImageView t,Bitmap bitmap,String filePath) {
        mTileImageView = t;
        mBitmap = bitmap;
        mFilePath = filePath;

    }

    public GifTextrue(TileImageView t,Bitmap bitmap,Context context,Uri uri) {
        mTileImageView = t;
        mBitmap = bitmap;
        mContext = context;
        mUri = uri;
    }

    public static String THREAD_NAME = "GifThead";

    public void startAnimate() {
        Log.i(TAG, "tryAnimate called for [" + this.mFilePath + "]");
        if (mHandler == null) {
            mThread = new HandlerThread("GifThead");

            mThread.start();
            mHandler = new GifHandler(mThread.getLooper());
        }
        mHandler.sendEmptyMessage(GifHandler.START);
    }

    public void stopAnimate() {
        if (mHandler != null) {
            Message msg = mHandler.obtainMessage(GifHandler.STOP);
            final HandlerThread thread = mThread;
            msg.obj = new Runnable() {
                public void run() {
                    thread.quit();
                }
            };
            mHandler.sendMessage(msg);
            mHandler = null;
            mThread = null;
        }
    }

}
