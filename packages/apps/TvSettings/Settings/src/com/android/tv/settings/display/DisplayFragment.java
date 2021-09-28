/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

package com.android.tv.settings.display;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import androidx.leanback.preference.LeanbackPreferenceFragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;
import android.util.Log;
import android.view.Display;
import android.os.SystemProperties;

import com.android.tv.settings.R;
import com.android.tv.settings.data.ConstData;

public class DisplayFragment extends LeanbackPreferenceFragment {
    private static final String TAG = "DisplayFragment";
    public static final String KEY_MAIN_DISPLAY = "main_display";
    public static final String KEY_SECOND_DISPLAY = "second_display";
    public static final String KEY_DISPLAY_DEVICE_CATEGORY = "display_device_category";
    public static final String HDMI_PLUG_ACTION = "android.intent.action.HDMI_PLUGGED";
    private PreferenceScreen mPreferenceScreen;
    private static String mStrPlatform;
    private static boolean mIsUseDisplayd;

    /**
     * 原生标准显示管理接口,用于DRM显示相关
     */
    private DisplayManager mDisplayManager;
    /**
     * 插拔显示设备监听
     */
    private DisplayListener mDisplayListener;
    /**
     * 主显示
     */
    private Preference mMainDisplayPreference;
    /**
     * 次显示
     */
    private Preference mSecondDisPreference;
    /**
     * HDMI热插拔接收器
     */
    private HDMIReceiver mHdmiReceiver;
    private PreferenceCategory mDisplayDeviceCategory;

    public static DisplayFragment newInstance() {
        return new DisplayFragment();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mIsUseDisplayd = false;//SystemProperties.getBoolean("ro.rk.displayd.enable", true);
        if (!mIsUseDisplayd) {
            setPreferencesFromResource(R.xml.display_drm, null);
        }
        initData();
        rebuildView();
    }


    @Override
    public void onStart() {
        Log.i(TAG, "onStart");
        super.onStart();
    }

    @Override
    public void onResume() {
        Log.i(TAG, "onResume");
        super.onResume();
        registerDisplayListener();
        registerHDMIReceiver();
    }

    @Override
    public void onPause() {
        Log.i(TAG, "onPause");
        super.onPause();
        unRegiserDisplayListener();
        unRegisterHDMIReceiver();
    }

    private void initData() {
        mPreferenceScreen = getPreferenceScreen();
        mMainDisplayPreference = findPreference(KEY_MAIN_DISPLAY);
        mSecondDisPreference = findPreference(KEY_SECOND_DISPLAY);
        mDisplayDeviceCategory = (PreferenceCategory) findPreference(KEY_DISPLAY_DEVICE_CATEGORY);
        mDisplayManager = (DisplayManager) getActivity().getSystemService(Context.DISPLAY_SERVICE);
        mDisplayListener = new DisplayListener();
        mHdmiReceiver = new HDMIReceiver();
        Log.i(TAG, "screenTitle:" + mPreferenceScreen.getTitle());
    }

    /**
     * 注册显示监听
     */
    private void registerDisplayListener() {
        mDisplayManager.registerDisplayListener(mDisplayListener, null);
    }

    /**
     * 取消显示监听
     */
    private void unRegiserDisplayListener() {
        mDisplayManager.unregisterDisplayListener(mDisplayListener);
    }

    /**
     * 注册HDMI接收器
     */
    private void registerHDMIReceiver() {
        IntentFilter filter = new IntentFilter(HDMI_PLUG_ACTION);
        getActivity().registerReceiver(mHdmiReceiver, filter);
    }


    /**
     * 取消注册HDMI接收器
     */
    private void unRegisterHDMIReceiver() {
        getActivity().unregisterReceiver(mHdmiReceiver);
    }

    /**
     * 重新构造页面
     */
    private void rebuildView() {
        mDisplayDeviceCategory.removeAll();
        List<DisplayInfo> displayInfos = getDisplayInfos();
        Log.i(TAG, "rebuildView->displayInfos:" + displayInfos);
        if (displayInfos.size() <= 0) {
            try {
                Log.i(TAG, "sleep for waiting native status update");
                Thread.sleep(1500);
                displayInfos = getDisplayInfos();
                Log.i(TAG, "after sleep rebuildView->displayInfos:" + displayInfos);
            } catch (InterruptedException e) {
            }
        }
        if (displayInfos.size() > 0) {
            for (DisplayInfo displayInfo : displayInfos) {
                Intent intent = new Intent();
                intent.putExtra(ConstData.IntentKey.DISPLAY_INFO, displayInfo);
                getActivity().setIntent(intent);
                if (displayInfo.getDisplayId() == 0) {
                    mMainDisplayPreference.setTitle(displayInfo.getDescription());
                    mDisplayDeviceCategory.addPreference(mMainDisplayPreference);
                } else {
                    mSecondDisPreference.setTitle(displayInfo.getDescription());
                    mDisplayDeviceCategory.addPreference(mSecondDisPreference);
                }
            }
        }
    }

    /**
     * 获取所有外接显示设备信息
     *
     * @param
     * @return
     */
    private List<DisplayInfo> getDisplayInfos() {
        List<DisplayInfo> displayInfos = new ArrayList<DisplayInfo>();
        mIsUseDisplayd = false;//SystemProperties.getBoolean("ro.rk.displayd.enable", true);
        Display[] displays = mDisplayManager.getDisplays();
        if (!mIsUseDisplayd) {
            displayInfos.addAll(DrmDisplaySetting.getDisplayInfoList());
        }
        return displayInfos;
    }


    /**
     * 反射调用相关方法
     *
     * @param object
     * @param methodName
     * @param parameterTypes
     * @param args
     * @return
     */
    private Object invokeMethod(Object object, String methodName, Class<?>[] parameterTypes, Object[] args) {
        Object result = null;
        try {
            Method method = object.getClass().getDeclaredMethod(methodName, parameterTypes);
            method.setAccessible(true);
            result = method.invoke(object, args);
        } catch (Exception e) {
            Log.i(TAG, "invokeMethod->exception:" + e);
        }
        return result;
    }


    /**
     * 转换显示接口
     */
    private void changeDisplayInterface(boolean isHDMIConnect) {
/*        mDisplayOutputManager = null;
        try {
            mDisplayOutputManager = new DisplayOutputManager();
        } catch (Exception e) {
            Log.i(TAG, "new DisplayOutputManger exception:" + e);
        }
        if (!isHDMIConnect) {
            mDisplayOutputManager.setInterface(mDisplayOutputManager.MAIN_DISPLAY, 1, true);
        }*/
    }


    /**
     * 显示设备插拔监听器
     *
     * @author GaoFei
     */
    class DisplayListener implements DisplayManager.DisplayListener {

        @Override
        public void onDisplayAdded(int displayId) {
            Log.i(TAG, "DisplayListener->onDisplayAdded");
            rebuildView();
        }

        @Override
        public void onDisplayRemoved(int displayId) {
            Log.i(TAG, "DisplayListener->onDisplayRemoved");
            rebuildView();
        }

        @Override
        public void onDisplayChanged(int displayId) {
            Log.i(TAG, "DisplayListener->onDisplayChanged");

        }

    }


    /**
     * HDMI 热插拔事件
     *
     * @author GaoFei
     */
    class HDMIReceiver extends BroadcastReceiver {

        @Override
        public void onReceive(Context context, Intent intent) {
            boolean state = intent.getBooleanExtra("state", true);
            //changeDisplayInterface(state);
            Log.i(TAG, "DisplayFragment.java HDMIReceiver->onReceive");
            rebuildView();
        }

    }
}
