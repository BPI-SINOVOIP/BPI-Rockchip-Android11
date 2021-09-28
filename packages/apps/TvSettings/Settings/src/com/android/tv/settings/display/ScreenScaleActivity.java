package com.android.tv.settings.display;

import android.content.Context;
import android.os.Handler;
import android.R.bool;
import android.R.integer;
import android.annotation.TargetApi;
import android.app.Activity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.WindowManager;
import android.os.Build;
import android.os.Message;
import android.os.RemoteException;
import android.widget.ImageView;
import android.view.View;
import android.graphics.PixelFormat;
import android.graphics.BitmapFactory;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.BitmapFactory;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.widget.ImageView;
import android.widget.TextView;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.widget.RelativeLayout;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.Log;

import com.android.tv.settings.R;
import com.android.tv.settings.data.ConstData;
import com.android.tv.settings.util.ReflectUtils;

import android.os.SystemProperties;

public class ScreenScaleActivity extends Activity {
    private static final String TAG = "ScreenScaleActivity";
    private final int RightButtonPressed = 0;
    private final int LeftButttonPressed = 1;
    private final int UpButtonPressed = 2;
    private final int DownButtonPressed = 3;
    private final int RightButtonResume = 4;
    private final int LeftButtonResume = 5;
    private final int UpButtonResume = 6;
    private final int DownButtonResume = 7;

    private ImageView mDirctionButton;
    private ImageView mRightButton;
    private ImageView mLeftButton;
    private ImageView mUpButton;
    private ImageView mDownButton;

    private static final int MAX_SCALE = 100;
    private static final int MIN_SCALE = 80;

    public static final int SYSTEM_UI_FLAG_SHOW_FULLSCREEN = 0x00000004;

    private int mScreenWidth = 0;
    private int mScreenHeight = 0;
    private int mDensityDpi = 0;
    private int mDefaultDpi = 160;
    private float mDpiRatio = 0;
    private int mkeylast = -1;
    /**
     * 标识平台
     */
    private String mPlatform;

    private boolean mIsUseDisplayd;

    /**
     * 当前设备的显示信息
     */
    private DisplayInfo mDisplayInfo;
    /**
     * 左侧缩放
     */
    private int mLeftScale = 100;
    /**
     * 底部缩放
     */
    private int mBottomScale = 100;

    private void getScreenSize() {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(displayMetrics);
        mScreenWidth = displayMetrics.widthPixels;
        mScreenHeight = displayMetrics.heightPixels;
        mDensityDpi = displayMetrics.densityDpi;
        mDpiRatio = ((float) mDefaultDpi) / (float) displayMetrics.densityDpi;
        Log.d("ScreenSettingActivity", "displayMetrics.densityDpi is: " + mDensityDpi);
        Log.d("ScreenSettingActivity", "displayMetrics.widthPixels is: " + mScreenWidth);
        Log.d("ScreenSettingActivity", "displayMetrics.heightPixels is: " + mScreenHeight);
    }

    private Bitmap bitMapScale(int id) {
        Bitmap map = BitmapFactory.decodeResource(this.getResources(), id);
        float scale = mScreenWidth / 1280f * mDpiRatio;
        int width = (int) ((float) map.getWidth() * scale);
        int height = (int) ((float) map.getHeight() * scale);

        Bitmap resize = Bitmap.createScaledBitmap(map, width, height, true);
        return resize;
    }

    private void createView() {
        int centerX = mScreenWidth / 2;
        int y = mScreenHeight / 5;

        ImageView touch_up = (ImageView) findViewById(R.id.screen_touch_up);
        Bitmap map = bitMapScale(R.drawable.screen_vertical_reduce);
        RelativeLayout.LayoutParams param = new RelativeLayout.LayoutParams(map.getWidth(), map.getHeight());
        param.addRule(RelativeLayout.ALIGN_PARENT_TOP);
        param.setMargins(centerX - map.getWidth() / 2, y, 0, 0);

        y += map.getHeight() + 10;
        touch_up.setLayoutParams(param);

        mUpButton = (ImageView) findViewById(R.id.button_up);
        map = bitMapScale(R.drawable.button_vertical_up);
        y += map.getHeight() + 10;
        param = new RelativeLayout.LayoutParams(map.getWidth(), map.getHeight());
        param.addRule(RelativeLayout.BELOW, R.id.screen_touch_up);
        param.setMargins(centerX - map.getWidth() / 2, 10, 0, 0);
        mUpButton.setLayoutParams(param);

        ImageView ok = (ImageView) findViewById(R.id.button_ok);
        Bitmap okmap = bitMapScale(R.drawable.ok);
        y += okmap.getHeight() / 2;
        param = new RelativeLayout.LayoutParams(okmap.getWidth(), okmap.getHeight());
        param.addRule(RelativeLayout.BELOW, R.id.button_up);
        param.setMargins(centerX - okmap.getWidth() / 2, 10, 0, 0);
        ok.setLayoutParams(param);

        mDownButton = (ImageView) findViewById(R.id.button_down);
        map = bitMapScale(R.drawable.button_vertical_down);
        param = new RelativeLayout.LayoutParams(map.getWidth(), map.getHeight());
        param.addRule(RelativeLayout.BELOW, R.id.button_ok);
        param.setMargins(centerX - map.getWidth() / 2, 10, 0, 0);
        mDownButton.setLayoutParams(param);

        ImageView touch_down = (ImageView) findViewById(R.id.screen_touch_down);
        map = bitMapScale(R.drawable.screen_vertical_add);
        param = new RelativeLayout.LayoutParams(map.getWidth(), map.getHeight());
        param.addRule(RelativeLayout.BELOW, R.id.button_down);
        param.setMargins(centerX - map.getWidth() / 2, 10, 0, 0);
        touch_down.setLayoutParams(param);

        mLeftButton = (ImageView) findViewById(R.id.button_left);
        map = bitMapScale(R.drawable.button_left);
        int x = -okmap.getWidth() / 2 - 10 - map.getWidth() / 2;
        param = new RelativeLayout.LayoutParams(map.getWidth(), map.getHeight());
        param.addRule(RelativeLayout.ALIGN_LEFT, R.id.button_ok);
        param.setMargins(x, y - map.getHeight() / 2, 0, 0);
        mLeftButton.setLayoutParams(param);

        ImageView left = (ImageView) findViewById(R.id.screen_button_left);
        map = bitMapScale(R.drawable.screen_horizontal_reduce);
        x += -map.getWidth() / 2 - 10;
        RelativeLayout.LayoutParams param0 = new RelativeLayout.LayoutParams(map.getWidth(), map.getHeight());
        param0.addRule(RelativeLayout.ALIGN_LEFT, R.id.button_left);
        param0.setMargins(x, y - map.getHeight() / 2, 0, 0);
        left.setLayoutParams(param0);

        mRightButton = (ImageView) findViewById(R.id.button_right);
        map = bitMapScale(R.drawable.button_right);
        x = okmap.getWidth() / 2 + 10 + map.getWidth() / 2;
        param = new RelativeLayout.LayoutParams(map.getWidth(), map.getHeight());
        param.addRule(RelativeLayout.ALIGN_RIGHT, R.id.button_ok);
        param.setMargins(0, y - map.getHeight() / 2, -x, 0);
        mRightButton.setLayoutParams(param);

        ImageView right = (ImageView) findViewById(R.id.screen_button_right);
        map = bitMapScale(R.drawable.screen_horizontal_add);
        x += 10 + map.getWidth() / 2;
        param = new RelativeLayout.LayoutParams(map.getWidth(), map.getHeight());
        param.addRule(RelativeLayout.ALIGN_RIGHT, R.id.button_right);
        param.setMargins(0, y - map.getHeight() / 2, -x, 0);
        right.setLayoutParams(param);

        mRightButton.setOnClickListener(mOnClick);
        mLeftButton.setOnClickListener(mOnClick);
        mUpButton.setOnClickListener(mOnClick);
        mDownButton.setOnClickListener(mOnClick);
    }

    // This snippet hides the system bars.
    @TargetApi(19)
    private void hideSystemUI() {
        // Set the IMMERSIVE flag.
        // Set the content to appear under the system bars so that the content
        // doesn't resize when the system bars hide and show.
        if (Build.VERSION.SDK_INT >= 19) {
            getWindow().setFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION,
                    WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
            getWindow().setFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS,
                    WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav
                                                                                                          // bar
                            | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
                            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }

    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        initData();
        // full screen
        hideSystemUI();

        setContentView(R.layout.screen_setting);

        getWindow().setFormat(PixelFormat.RGBA_8888);
        // BitmapFactory.setDefaultConfig(Bitmap.Config.ARGB_8888);
        getScreenSize();
        createView();
        /*
         * mRightButton = (ImageView)findViewById(R.id.button_right); mLeftButton =
         * (ImageView)findViewById(R.id.button_left); mUpButton =
         * (ImageView)findViewById(R.id.button_up); mDownButton =
         * (ImageView)findViewById(R.id.button_down);
         *
         * mRightButton.setOnClickListener(mOnClick);
         * mLeftButton.setOnClickListener(mOnClick);
         * mUpButton.setOnClickListener(mOnClick);
         * mDownButton.setOnClickListener(mOnClick);
         */

    }

    private View.OnClickListener mOnClick = new View.OnClickListener() {
        public void onClick(View v) {
            int id = v.getId();
            int scalevalue;
            switch (id) {
            case R.id.button_right:
                LOGD("button_right");
                rightClick(true);
                break;
            case R.id.button_left:
                LOGD("button_left");
                leftClick(true);
                break;
            case R.id.button_up:
                LOGD("touch_up");
                upClick(true);
                break;
            case R.id.button_down:
                LOGD("touch_down");
                downClick(true);
                break;
            }
        }
    };

    public boolean dispatchKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        LOGD("keyCode = " + keyCode);
        int scalevalue;
        if (mkeylast != keyCode) {
            mkeylast = keyCode;
            switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                rightClick(false);
                break;
            case KeyEvent.KEYCODE_DPAD_LEFT:
                leftClick(false);
                break;
            case KeyEvent.KEYCODE_DPAD_UP:
                upClick(false);
                break;
            case KeyEvent.KEYCODE_DPAD_DOWN:
                downClick(false);
                break;
            }
        } else {
            mkeylast = -1;
        }

        return super.dispatchKeyEvent(event);
    }

    private Handler mHandler = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.what) {
            case RightButtonResume:
                mRightButton.setImageResource(R.drawable.button_vertical_right);
                break;
            case LeftButtonResume:
                mLeftButton.setImageResource(R.drawable.button_vertical_left);
                break;
            case UpButtonResume:
                mUpButton.setImageResource(R.drawable.button_up);
                break;
            case DownButtonResume:
                mDownButton.setImageResource(R.drawable.button_down);
                break;
            }
        }
    };

    private void LOGD(String msg) {
        if (true)
            Log.d("ScreenSettingActivity", msg);
    }

    private void rightClick(boolean isClick) {
        if (!mIsUseDisplayd) {
            mLeftScale += 1;
            if (mLeftScale > MAX_SCALE)
                mLeftScale = MAX_SCALE;
            setOverScanProperty(ConstData.ScaleDirection.RIGHT);
        }
        if (!isClick) {
            mRightButton.setImageResource(R.drawable.button_right_pressed);
            mHandler.removeMessages(RightButtonResume);
            mHandler.sendEmptyMessageDelayed(RightButtonResume, 100);
        }
    }

    private void leftClick(boolean isClick) {
        if (!mIsUseDisplayd) {
            mLeftScale -= 1;
            if (mLeftScale < MIN_SCALE)
                mLeftScale = MIN_SCALE;
            setOverScanProperty(ConstData.ScaleDirection.LEFT);
        }
        if (!isClick) {
            mLeftButton.setImageResource(R.drawable.button_left_pressed);
            mHandler.removeMessages(LeftButtonResume);
            mHandler.sendEmptyMessageDelayed(LeftButtonResume, 100);
        }
    }

    private void upClick(boolean isClick) {
        // add code here
        if (!mIsUseDisplayd) {
            mBottomScale -= 1;
            if (mBottomScale < MIN_SCALE)
                mBottomScale = MIN_SCALE;
            setOverScanProperty(ConstData.ScaleDirection.TOP);
        }
        if (!isClick) {
            mUpButton.setImageResource(R.drawable.button_vertical_up_pressed);
            mHandler.removeMessages(UpButtonResume);
            mHandler.sendEmptyMessageDelayed(UpButtonResume, 100);
        }
    }

    private void downClick(boolean isClick) {
        if (!mIsUseDisplayd) {
            mBottomScale += 1;
            if (mBottomScale > MAX_SCALE)
                mBottomScale = MAX_SCALE;
            setOverScanProperty(ConstData.ScaleDirection.BOTTOM);
        }
        if (!isClick) {
            mDownButton.setImageResource(R.drawable.button_vertical_down_pressed);
            mHandler.removeMessages(DownButtonResume);
            mHandler.sendEmptyMessageDelayed(DownButtonResume, 100);
        }
    }

    /**
     * 初始化数据
     */
    public void initData() {
        mPlatform = getIntent().getStringExtra(ConstData.IntentKey.PLATFORM);
        mIsUseDisplayd = false;// SystemProperties.getBoolean("ro.rk.displayd.enable", true);
        mDisplayInfo = (DisplayInfo) getIntent().getSerializableExtra(ConstData.IntentKey.DISPLAY_INFO);
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
        } catch (Exception e) {
            e.printStackTrace();
        }
        if(rkDisplayOutputManager == null) return;
        Rect overScan;
        if (!mIsUseDisplayd) {
            overScan = (Rect)ReflectUtils.invokeMethod(rkDisplayOutputManager, "getOverScan",
            new Class[] { int.class }, new Object[] { mDisplayInfo.getDisplayId() });
            if (overScan == null) return;
            try {
                mLeftScale = overScan.left;
                mBottomScale = overScan.bottom;
                Log.i(TAG, "mLeftScale = " + mLeftScale + ", mBottomScale = " + mBottomScale);
            } catch (Exception e) {
                e.printStackTrace();
                Log.e(TAG, "mLeft = " + mLeftScale + ",mBottom = " + mBottomScale);
            }
        }
    }

    private void setOverScanProperty(int direction) {
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
        } catch (Exception e) {
            e.printStackTrace();
        }
        if (rkDisplayOutputManager == null)
            return;
        Log.i(TAG, "setOverScanProperty->time1:" + System.currentTimeMillis());
        switch (direction) {
        case ConstData.ScaleDirection.LEFT:
        case ConstData.ScaleDirection.RIGHT:
            ReflectUtils.invokeMethod(rkDisplayOutputManager, "setOverScan",
                    new Class[] { int.class, int.class, int.class },
                    new Object[] { mDisplayInfo.getDisplayId(), 0, mLeftScale });
            ReflectUtils.invokeMethod(rkDisplayOutputManager, "setOverScan",
                    new Class[] { int.class, int.class, int.class },
                    new Object[] { mDisplayInfo.getDisplayId(), 2, mLeftScale });
            break;
        case ConstData.ScaleDirection.TOP:
        case ConstData.ScaleDirection.BOTTOM:
            ReflectUtils.invokeMethod(rkDisplayOutputManager, "setOverScan",
                    new Class[] { int.class, int.class, int.class },
                    new Object[] { mDisplayInfo.getDisplayId(), 1, mBottomScale });
            ReflectUtils.invokeMethod(rkDisplayOutputManager, "setOverScan",
                    new Class[] { int.class, int.class, int.class },
                    new Object[] { mDisplayInfo.getDisplayId(), 3, mBottomScale });
            break;
        default:
            break;
        }
        Log.i(TAG, "setOverScanProperty->time2:" + System.currentTimeMillis());

        // save config
        ReflectUtils.invokeMethod(rkDisplayOutputManager, "saveConfig", new Class[] {}, new Object[] {});
    }
}
