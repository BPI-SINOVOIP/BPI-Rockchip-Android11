/**
 *
 */
package com.android.tv.settings.display;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.Arrays;

import android.util.Log;
import android.view.KeyEvent;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.SimpleAdapter.ViewBinder;
import android.R.integer;
import android.content.Context;
import android.content.SharedPreferences;

import com.android.tv.settings.BaseInputActivity;
import com.android.tv.settings.R;
import com.android.tv.settings.data.ConstData;
import com.android.tv.settings.util.JniCall;
import com.android.tv.settings.util.ReflectUtils;

import android.os.SystemProperties;
import android.view.View;
import android.os.Bundle;

/**
 * @author GaoFei
 */
public class AdvancedDisplaySettingsActivity extends BaseInputActivity
        implements SeekBar.OnSeekBarChangeListener, View.OnClickListener {
    private static final String TAG = "AdvancedDisplaySettingsActivity";
    private int mOldBcshBrightness;
    private int mOldBcshContrast;
    private int mOldBcshStauration;
    private int mOldBcshTone;
    private String mStrPlatform;
    private boolean mIsSupportDRM;
    private boolean isFirstIn = true;
    /**
     * BCSH亮度
     */
    private SeekBar mSeekBarBcshBrightness;
    /**
     * BCSH对比度
     */
    private SeekBar mSeekBarBcshContrast;
    /**
     * BCSH饱和度
     */
    private SeekBar mSeekBarBcshSaturation;
    /**
     * BCSH色调
     */
    private SeekBar mSeekBarBcshTone;
    /**
     * BCSH亮度值
     */
    private TextView mTextBcshBrightnessNum;
    /**
     * BCSH对比度值
     */
    private TextView mTextBcshContrastNum;
    /**
     * BCSH饱和度值
     */
    private TextView mTextBcshStaurationNum;
    /**
     * BCSH色调值
     */
    private TextView mTextBcshToneNum;
    /**
     * 确定按钮
     */
    private Button mBtnOK;
    /**
     * 取消按钮
     */
    private Button mBtnCancel;
    private Button mBtnReset;
    private Button mBtnCold;
    private Button mBtnWarm;
    private Button mBtnSharp;

    /**
     * 最大亮度
     */
    private double mMaxBrightness;
    /**
     * 最小亮度
     */
    private double mMinBrightness;
    /**
     * 亮度数值
     */
    private double mBrightnessNum;
    /**
     * 饱和度数值
     */
    private double mSaturationNum;
    /**
     * 显示ID
     */
    private int mDisplayId;
    /**
     * DRM显示管理
     */
    private Object mRkDisplayManager;

    @Override
    public void init() {
        mStrPlatform = SystemProperties.get("ro.board.platform");
        mIsSupportDRM = true;// !SystemProperties.getBoolean("ro.rk.displayd.enable", true);
        try {
            if (mIsSupportDRM)
                mRkDisplayManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
        } catch (Exception e) {
            // no hnadle
        }
        mDisplayId = getIntent().getIntExtra(ConstData.IntentKey.DISPLAY_ID, 0);
        mSeekBarBcshBrightness = (SeekBar) findViewById(R.id.brightness);
        mSeekBarBcshContrast = (SeekBar) findViewById(R.id.contrast);
        mSeekBarBcshSaturation = (SeekBar) findViewById(R.id.saturation);
        mSeekBarBcshTone = (SeekBar) findViewById(R.id.tone);
        mTextBcshBrightnessNum = (TextView) findViewById(R.id.text_bcsh_brightness_num);
        mTextBcshContrastNum = (TextView) findViewById(R.id.text_bcsh_contrast_num);
        mTextBcshStaurationNum = (TextView) findViewById(R.id.text_bcsh_saturation_num);
        mTextBcshToneNum = (TextView) findViewById(R.id.text_bcsh_tone_num);
        mBtnOK = (Button) findViewById(R.id.btn_ok);
        mBtnCancel = (Button) findViewById(R.id.btn_cancel);
        mBtnReset = (Button) findViewById(R.id.btn_reset);
        mBtnCold = (Button) findViewById(R.id.btn_cold);
        mBtnWarm = (Button) findViewById(R.id.btn_warm);
        mBtnSharp = (Button) findViewById(R.id.btn_sharp);

        mBtnOK.setOnClickListener(this);
        mBtnCancel.setOnClickListener(this);
        mBtnReset.setOnClickListener(this);
        mBtnCold.setOnClickListener(this);
        mBtnWarm.setOnClickListener(this);
        mBtnSharp.setOnClickListener(this);

        SharedPreferences bcshPreferences = getSharedPreferences(ConstData.SharedKey.BCSH_VALUES, Context.MODE_PRIVATE);
        if (!mIsSupportDRM) {
            mSeekBarBcshContrast.setKeyProgressIncrement(20);
            mSeekBarBcshSaturation.setKeyProgressIncrement(20);
            mOldBcshBrightness = bcshPreferences.getInt(ConstData.SharedKey.BCSH_BRIGHTNESS, 32);
            mOldBcshContrast = bcshPreferences.getInt(ConstData.SharedKey.BCSH_CONTRAST, 1000);
            mOldBcshStauration = bcshPreferences.getInt(ConstData.SharedKey.BCSH_STAURATION, 1000);
            mOldBcshTone = bcshPreferences.getInt(ConstData.SharedKey.BCSH_TONE, 30);
        } else {
            mSeekBarBcshBrightness.setMax(100);
            mSeekBarBcshContrast.setMax(100);
            mSeekBarBcshSaturation.setMax(100);
            mSeekBarBcshTone.setMax(100);
            mSeekBarBcshBrightness.setKeyProgressIncrement(1);
            mSeekBarBcshContrast.setKeyProgressIncrement(1);
            mSeekBarBcshSaturation.setKeyProgressIncrement(1);
            mSeekBarBcshTone.setKeyProgressIncrement(1);
            mOldBcshBrightness = (Integer) ReflectUtils.invokeMethod(mRkDisplayManager, "getBrightness",
                    new Class[] { int.class }, new Object[] { mDisplayId });
            mOldBcshContrast = (Integer) ReflectUtils.invokeMethod(mRkDisplayManager, "getContrast",
                    new Class[] { int.class }, new Object[] { mDisplayId });
            mOldBcshStauration = (Integer) ReflectUtils.invokeMethod(mRkDisplayManager, "getSaturation",
                    new Class[] { int.class }, new Object[] { mDisplayId });
            mOldBcshTone = (Integer) ReflectUtils.invokeMethod(mRkDisplayManager, "getHue", new Class[] { int.class },
                    new Object[] { mDisplayId });
        }
        mSeekBarBcshBrightness.setOnSeekBarChangeListener(this);
        mSeekBarBcshContrast.setOnSeekBarChangeListener(this);
        mSeekBarBcshSaturation.setOnSeekBarChangeListener(this);
        mSeekBarBcshTone.setOnSeekBarChangeListener(this);
        mSeekBarBcshBrightness.setProgress(mOldBcshBrightness);
        mSeekBarBcshContrast.setProgress(mOldBcshContrast);
        mSeekBarBcshSaturation.setProgress(mOldBcshStauration);
        mSeekBarBcshTone.setProgress(mOldBcshTone);
        updateBcshValue();
    }

    @Override
    public int getContentLayoutRes() {
        return R.layout.activity_advanced_display_settings;
    }

    @Override
    public String getInputTitle() {
        return getString(R.string.advance_settings);
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (seekBar == mSeekBarBcshBrightness) {
            updateBcshValue();
        } else if (seekBar == mSeekBarBcshContrast) {
            updateBcshValue();
        } else if (seekBar == mSeekBarBcshSaturation) {
            updateBcshValue();
        } else if (seekBar == mSeekBarBcshTone) {
            updateBcshValue();
        }
        Log.i(TAG, "onProgressChanged->progress:" + progress);
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
        Log.i(TAG, "onStartTrackingTouch");
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        Log.i(TAG, "onStopTrackingTouch");
    }

    private void setDefaultBcshValue(int brightness, int contrast, int saturation, int tone) {
        mSeekBarBcshBrightness.setProgress(brightness);
        mSeekBarBcshContrast.setProgress(contrast);
        mSeekBarBcshSaturation.setProgress(saturation);
        mSeekBarBcshTone.setProgress(tone);
        saveNewValue();
        updateBcshValue();
        DrmDisplaySetting.saveConfig();
    }

    private void updateBcshValue() {
        if (isFirstIn)
            return;
        if (mIsSupportDRM && mRkDisplayManager != null) {
            Log.d(TAG, "b:" + mSeekBarBcshBrightness.getProgress() + " c:" + mSeekBarBcshContrast.getProgress() + " s:"
                    + mSeekBarBcshSaturation.getProgress() + " h:" + mSeekBarBcshTone.getProgress());
            ReflectUtils.invokeMethod(mRkDisplayManager, "setBrightness", new Class[] { int.class, int.class },
                    new Object[] { mDisplayId, mSeekBarBcshBrightness.getProgress() });
            ReflectUtils.invokeMethod(mRkDisplayManager, "setContrast", new Class[] { int.class, int.class },
                    new Object[] { mDisplayId, mSeekBarBcshContrast.getProgress() });
            ReflectUtils.invokeMethod(mRkDisplayManager, "setSaturation", new Class[] { int.class, int.class },
                    new Object[] { mDisplayId, mSeekBarBcshSaturation.getProgress() });
            ReflectUtils.invokeMethod(mRkDisplayManager, "setHue", new Class[] { int.class, int.class },
                    new Object[] { mDisplayId, mSeekBarBcshTone.getProgress() });

            return;
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            recoveryOldValue();
        }
        return super.onKeyDown(keyCode, event);
    }

    private void recoveryOldValue() {
        if (mIsSupportDRM && mRkDisplayManager != null) {
            ReflectUtils.invokeMethod(mRkDisplayManager, "setBrightness", new Class[] { int.class, int.class },
                    new Object[] { mDisplayId, mOldBcshBrightness });
            ReflectUtils.invokeMethod(mRkDisplayManager, "setContrast", new Class[] { int.class, int.class },
                    new Object[] { mDisplayId, mOldBcshContrast });
            ReflectUtils.invokeMethod(mRkDisplayManager, "setSaturation", new Class[] { int.class, int.class },
                    new Object[] { mDisplayId, mOldBcshStauration });
            ReflectUtils.invokeMethod(mRkDisplayManager, "setHue", new Class[] { int.class, int.class },
                    new Object[] { mDisplayId, mOldBcshTone });
            return;
        }
    }

    private void saveNewValue() {
        int brightness = mSeekBarBcshBrightness.getProgress();
        int contrast = mSeekBarBcshContrast.getProgress();
        int satuation = mSeekBarBcshSaturation.getProgress();
        int tone = mSeekBarBcshTone.getProgress();
        Log.i(TAG,
                "brightness = " + brightness + "contrast = " + contrast + "satuation = " + satuation + "tone= " + tone);

        SharedPreferences bcshPreferences = getSharedPreferences(ConstData.SharedKey.BCSH_VALUES, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = bcshPreferences.edit();
        editor.putInt(ConstData.SharedKey.BCSH_BRIGHTNESS, mSeekBarBcshBrightness.getProgress());
        editor.putInt(ConstData.SharedKey.BCSH_CONTRAST, mSeekBarBcshContrast.getProgress());
        editor.putInt(ConstData.SharedKey.BCSH_STAURATION, mSeekBarBcshSaturation.getProgress());
        editor.putInt(ConstData.SharedKey.BCSH_TONE, mSeekBarBcshTone.getProgress());
        editor.commit();
        saveValueToPreference(ConstData.SharedKey.MAX_BRIGHTNESS, "" + mMaxBrightness);
        saveValueToPreference(ConstData.SharedKey.MIN_BRIGHTNESS, "" + mMinBrightness);
        saveValueToPreference(ConstData.SharedKey.BRIGHTNESS, "" + mBrightnessNum);
        saveValueToPreference(ConstData.SharedKey.STATURATION, "" + mSaturationNum);
    }

    @Override
    public void onClick(View v) {
        if (v == mBtnCancel) {
            recoveryOldValue();
            finish();
        } else if (v == mBtnOK) {
            saveNewValue();
            DrmDisplaySetting.saveConfig();
            finish();
        } else if (v == mBtnReset) {
            if (mIsSupportDRM)
                setDefaultBcshValue(50, 50, 50, 50);
            else
                setDefaultBcshValue(32, 1000, 1000, 30);
        } else if (v == mBtnCold) {
            if (mIsSupportDRM)
                setDefaultBcshValue(50, 50, 30, 31);
            else
                setDefaultBcshValue(32, 820, 732, 30);
        } else if (v == mBtnWarm) {
            if (mIsSupportDRM)
                setDefaultBcshValue(67, 53, 80, 74);
            else
                setDefaultBcshValue(32, 1278, 1190, 30);
        } else if (v == mBtnSharp) {
            if (mIsSupportDRM)
                setDefaultBcshValue(60, 77, 43, 46);
            else
                setDefaultBcshValue(42, 1617, 630, 30);
        }
    }

    private String getValueFromPreference(String key) {
        return getSharedPreferences(ConstData.SharedKey.HDR_VALUES, Context.MODE_PRIVATE).getString(key, "");
    }

    private void saveValueToPreference(String key, String value) {
        SharedPreferences sharedPreferences = getSharedPreferences(ConstData.SharedKey.HDR_VALUES,
                Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putString(key, value);
        editor.commit();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_base_input_for_advanced_display_settings);
        baseInit();
        init();
        getWindow().getDecorView().getRootView().setBackground(getResources().getDrawable(R.drawable.ramiro));
    }

    @Override
    protected void onResume() {
        super.onResume();
        isFirstIn = false;
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

}
