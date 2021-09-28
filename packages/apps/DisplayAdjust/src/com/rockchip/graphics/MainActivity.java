package com.rockchip.graphics;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.RkDisplayOutputManager;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.io.FileNotFoundException;
import java.util.ArrayList;

public class MainActivity extends Activity implements View.OnClickListener, SeekBar.OnSeekBarChangeListener, AdapterView.OnItemSelectedListener {

    private static final int REQ_CODE_PICTURES = 20;
    private static final int LUT_SIZE = 1024;
    private static final String LUT_3D_CONFIG_PATH = "/sdcard/3d.lut";
    private static final String OUTPUT_IMAGE_PATH = "/sdcard/baseparameter.img";
    private static final int[] COLOR_TEMPERATURE_VALUE = {
            3500, 5500, 6500, 7500, 8500, 9500
    };

    private Spinner mSpinner;
    private ImageView mImageView;
    private Button mBtnChangeImage;
    private Button mBtnBrightnessAdd;
    private Button mBtnBrightnessDel;
    private Button mBtnContrastAdd;
    private Button mBtnContrastDel;
    private Button mBtnSaturationAdd;
    private Button mBtnSaturationDel;
    private Button mBtnHueAdd;
    private Button mBtnHueDel;
    private Button mBtnColorTempAdd;
    private Button mBtnColorTempDel;
    private Button mBtnSet3DLut;
    private Button mBtnReset;
    private Button mBtnDump;
    private TextView mTvBrightness;
    private TextView mTvContrast;
    private TextView mTvSaturation;
    private TextView mTvHue;
    private TextView mTvColorTemp;
    private SeekBar mSbBrightness;
    private SeekBar mSbContrast;
    private SeekBar mSbSaturation;
    private SeekBar mSbHue;
    private SeekBar mSbColorTemp;

    private int mCurDpy = 0;
    private int mCurBrightness = 50;
    private int mCurContrast = 50;
    private int mCurSaturation = 50;
    private int mCurHue = 50;
    private int mCurColorTempIndex = 2;
    private RkDisplayOutputManager mRkDisplayOutputManager;
    private ArrayList<ConnectorInfo> mConnectorList;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_main);
        mRkDisplayOutputManager = new RkDisplayOutputManager();
        initUI();
        initValue();
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.btn_change_image:
                changeImage();
                break;
            case R.id.btn_brightness_add:
                if (mCurBrightness < 100) {
                    mCurBrightness++;
                mSbBrightness.setProgress(mCurBrightness);
                }
                break;
            case R.id.btn_brightness_del:
                if (mCurBrightness > 0) {
                    mCurBrightness--;
                mSbBrightness.setProgress(mCurBrightness);
                }
                break;
            case R.id.btn_contrast_add:
                if (mCurContrast < 100) {
                    mCurContrast++;
                mSbContrast.setProgress(mCurContrast);
                }
                break;
            case R.id.btn_contrast_del:
                if (mCurContrast > 0) {
                    mCurContrast--;
                mSbContrast.setProgress(mCurContrast);
                }
                break;
            case R.id.btn_saturation_add:
                if (mCurSaturation < 100) {
                    mCurSaturation++;
                mSbSaturation.setProgress(mCurSaturation);
                }
                break;
            case R.id.btn_saturation_del:
                if (mCurSaturation > 0) {
                    mCurSaturation--;
                mSbSaturation.setProgress(mCurSaturation);
                }
                break;
            case R.id.btn_hue_add:
                if (mCurHue < 100) {
                    mCurHue++;
                mSbHue.setProgress(mCurHue);
                }
                break;
            case R.id.btn_hue_del:
                if (mCurHue > 0) {
                    mCurHue--;
                mSbHue.setProgress(mCurHue);
                }
                break;
            case R.id.btn_color_temperature_add:
                if (mCurColorTempIndex < (COLOR_TEMPERATURE_VALUE.length - 1)) {
                    mCurColorTempIndex++;
                mSbColorTemp.setProgress(mCurColorTempIndex);
                }
                break;
            case R.id.btn_color_temperature_del:
                if (mCurColorTempIndex > 0) {
                    mCurColorTempIndex--;
                mSbColorTemp.setProgress(mCurColorTempIndex);
                }
                break;
            case R.id.btn_reset:
                reset();
                break;
            case R.id.btn_set_3d_lut:
                set3DLut();
                break;
            case R.id.btn_dump_baseparameter:
                dumpBaseparameter();
                break;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (REQ_CODE_PICTURES == requestCode && RESULT_OK == resultCode) {
            Uri uri = data.getData();
            ContentResolver cr = this.getContentResolver();
            try {
                Bitmap bitmap = BitmapFactory.decodeStream(cr.openInputStream(uri));
                mImageView.setBackgroundDrawable(new BitmapDrawable(bitmap));
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            }
        }
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
        switch (seekBar.getId()) {
            case R.id.sb_brightness:
                mCurBrightness = i;
                updateBrightness();
                break;
            case R.id.sb_contrast:
                mCurContrast = i;
                updateContrast();
                break;
            case R.id.sb_saturation:
                mCurSaturation = i;
                updateSaturation();
                break;
            case R.id.sb_hue:
                mCurHue = i;
                updateHue();
                break;
            case R.id.sb_color_temperature:
                mCurColorTempIndex = i;
                updateColorTemp();
                break;
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
        mCurDpy = mConnectorList.get(i).getDpy();
    }

    @Override
    public void onNothingSelected(AdapterView<?> adapterView) {

    }

    private void initUI() {
        mSpinner = (Spinner) findViewById(R.id.spinner);
        mSpinner.setOnItemSelectedListener(this);
        mImageView = (ImageView) findViewById(R.id.image);
        mBtnChangeImage = (Button) findViewById(R.id.btn_change_image);
        mBtnChangeImage.setOnClickListener(this);
        mTvBrightness = (TextView) findViewById(R.id.tv_brightness);
        mBtnBrightnessAdd = (Button) findViewById(R.id.btn_brightness_add);
        mBtnBrightnessAdd.setOnClickListener(this);
        mBtnBrightnessDel = (Button) findViewById(R.id.btn_brightness_del);
        mBtnBrightnessDel.setOnClickListener(this);
        mSbBrightness = (SeekBar) findViewById(R.id.sb_brightness);
        mSbBrightness.setOnSeekBarChangeListener(this);

        mTvContrast = (TextView) findViewById(R.id.tv_contrast);
        mBtnContrastAdd = (Button) findViewById(R.id.btn_contrast_add);
        mBtnContrastAdd.setOnClickListener(this);
        mBtnContrastDel = (Button) findViewById(R.id.btn_contrast_del);
        mBtnContrastDel.setOnClickListener(this);
        mSbContrast = (SeekBar) findViewById(R.id.sb_contrast);
        mSbContrast.setOnSeekBarChangeListener(this);

        mTvSaturation = (TextView) findViewById(R.id.tv_saturation);
        mBtnSaturationAdd = (Button) findViewById(R.id.btn_saturation_add);
        mBtnSaturationAdd.setOnClickListener(this);
        mBtnSaturationDel = (Button) findViewById(R.id.btn_saturation_del);
        mBtnSaturationDel.setOnClickListener(this);
        mSbSaturation = (SeekBar) findViewById(R.id.sb_saturation);
        mSbSaturation.setOnSeekBarChangeListener(this);

        mTvHue = (TextView) findViewById(R.id.tv_hue);
        mBtnHueAdd = (Button) findViewById(R.id.btn_hue_add);
        mBtnHueAdd.setOnClickListener(this);
        mBtnHueDel = (Button) findViewById(R.id.btn_hue_del);
        mBtnHueDel.setOnClickListener(this);
        mSbHue = (SeekBar) findViewById(R.id.sb_hue);
        mSbHue.setOnSeekBarChangeListener(this);

        mTvColorTemp = (TextView) findViewById(R.id.tv_color_temperature);
        mBtnColorTempAdd = (Button) findViewById(R.id.btn_color_temperature_add);
        mBtnColorTempAdd.setOnClickListener(this);
        mBtnColorTempDel = (Button) findViewById(R.id.btn_color_temperature_del);
        mBtnColorTempDel.setOnClickListener(this);
        mSbColorTemp = (SeekBar) findViewById(R.id.sb_color_temperature);
        mSbColorTemp.setMax(COLOR_TEMPERATURE_VALUE.length - 1);
        mSbColorTemp.setOnSeekBarChangeListener(this);

        mBtnReset = (Button) findViewById(R.id.btn_reset);
        mBtnReset.setOnClickListener(this);
        mBtnSet3DLut = (Button) findViewById(R.id.btn_set_3d_lut);
        mBtnSet3DLut.setOnClickListener(this);
        mBtnDump = (Button) findViewById(R.id.btn_dump_baseparameter);
        mBtnDump.setOnClickListener(this);
    }

    private void initValue() {
        mRkDisplayOutputManager.updateDispHeader();
        mConnectorList = new ArrayList<ConnectorInfo>();
        String[] info = mRkDisplayOutputManager.getConnectorInfo();
        for(int i = 0; i < info.length; i++){
            ConnectorInfo connectorInfo = new ConnectorInfo(info[i], i);
           if(connectorInfo != null && connectorInfo.getState() == 1){
               mConnectorList.add(connectorInfo);
           }
        }
        mCurDpy = mConnectorList.get(0).getDpy();
        SpinnerItemAdapter adapter = new SpinnerItemAdapter(MainActivity.this, mConnectorList);
        mSpinner.setAdapter(adapter);

        mCurBrightness = mRkDisplayOutputManager.getBrightness(mCurDpy);
        mCurContrast = mRkDisplayOutputManager.getContrast(mCurDpy);
        mCurSaturation = mRkDisplayOutputManager.getSaturation(mCurDpy);
        mCurHue = mRkDisplayOutputManager.getHue(mCurDpy);
        mCurColorTempIndex = getDefaultColorTempIndex(6500);
        updateBrightness();
        updateSaturation();
        updateContrast();
        updateHue();
        //updateColorTemp();
    }

    private void updateBrightness() {
        mSbBrightness.setProgress(mCurBrightness);
        mTvBrightness.setText(getString(R.string.brightness) + " " + mCurBrightness);
        int ret = mRkDisplayOutputManager.setBrightness(mCurDpy, mCurBrightness);
    }

    private void updateContrast() {
        mSbContrast.setProgress(mCurContrast);
        mTvContrast.setText(getString(R.string.contrast) + " " + mCurContrast);
        int ret = mRkDisplayOutputManager.setContrast(mCurDpy, mCurContrast);
    }

    private void updateSaturation() {
        mSbSaturation.setProgress(mCurSaturation);
        mTvSaturation.setText(getString(R.string.saturation) + " " + mCurSaturation);
        int ret = mRkDisplayOutputManager.setSaturation(mCurDpy, mCurSaturation);
    }

    private void updateHue() {
        mSbHue.setProgress(mCurHue);
        mTvHue.setText(getString(R.string.hue) + " " + mCurHue);
        int ret = mRkDisplayOutputManager.setHue(mCurDpy, mCurHue);
    }

    private void updateColorTemp() {
        mSbColorTemp.setProgress(mCurColorTempIndex);
        mTvColorTemp.setText(getString(R.string.color_temperature) + " " + COLOR_TEMPERATURE_VALUE[mCurColorTempIndex] + "K");
        int ret = 0;
        int[][] rgb = ColorTempUtil.colorTemperatureToRGB(LUT_SIZE, COLOR_TEMPERATURE_VALUE[mCurColorTempIndex]);
        ret = mRkDisplayOutputManager.setGamma(mCurDpy, LUT_SIZE, rgb[0], rgb[1], rgb[2]);
    }

    private void changeImage() {
        Intent intent = new Intent();
        intent.setType("image/*");
        intent.setAction(Intent.ACTION_GET_CONTENT);
        startActivityForResult(intent, REQ_CODE_PICTURES);
    }

    private void reset() {
        mCurBrightness = 50;
        mCurContrast = 50;
        mCurSaturation = 50;
        mCurHue = 50;
        mCurColorTempIndex = getDefaultColorTempIndex(6500);
        int ret[][] = CubicLutUtil.get3DLutFromAsset(MainActivity.this, "default.lut");
        if(ret != null){
            mRkDisplayOutputManager.set3DLut(mCurDpy, 729, ret[0], ret[1], ret[2]);
        }
        updateBrightness();
        updateSaturation();
        updateContrast();
        updateHue();
        updateColorTemp();
    }

    private int getDefaultColorTempIndex(int defaultValue) {
        int index = 0;
        for (int i = 0; i < COLOR_TEMPERATURE_VALUE.length; i++) {
            if (defaultValue == COLOR_TEMPERATURE_VALUE[i]) {
                index = i;
            }
        }
        return index;
    }

    private void set3DLut() {
        int ret[][] = CubicLutUtil.get3DLutFromFile(LUT_3D_CONFIG_PATH);
        if(ret != null){
            mRkDisplayOutputManager.set3DLut(mCurDpy, 729, ret[0], ret[1], ret[2]);
        }
    }

    private void dumpBaseparameter() {
        int ret = SaveBaseParameterUtil.outputImage(OUTPUT_IMAGE_PATH);
        if(ret == 0){
            Toast.makeText(MainActivity.this, OUTPUT_IMAGE_PATH + " output image success", Toast.LENGTH_SHORT).show();
       }else {
            Toast.makeText(MainActivity.this, OUTPUT_IMAGE_PATH + " output image fail", Toast.LENGTH_SHORT).show();
       }
    }

}
