/* $_FOR_ROCKCHIP_RBOX_$ */
//$_rbox_$_modify_$_zhengyang_20120220: Rbox android display management service

/*
 * Copyright (C) 2011 The Android Open Source Project
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
 * limitations under the License.
 */

package com.android.server;


import android.content.Context;
import android.content.Intent;
import android.os.IRkDisplayDeviceManagementService;
import android.os.SystemProperties;
import android.util.Slog;
import android.util.Log;
import java.util.ArrayList;
import java.util.List;
import android.graphics.Rect;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import android.os.SystemClock;

import com.android.server.rkdisplay.RkDisplayModes;
import com.android.server.rkdisplay.RkDisplayModes.RkConnectorInfo;
import com.android.server.rkdisplay.HdmiReceiver;

/**
 * @hide
 */

class RkDisplayDeviceManagementService extends IRkDisplayDeviceManagementService.Stub {
    private static final String TAG = "RkDisplayDeviceManagementService";
    private static final String ACTION_PLUGGED = "android.intent.action.HDMI_PLUGGED";
    private static final boolean DBG = true;
    private static final String DISPLAYD_TAG = "DisplaydConnector";
    private final RkDisplayModes mdrmModes;
    private final HdmiReceiver mHdmiReceiver;
    private final int MAIN_DISPLAY = 0;
    private final int AUX_DISPLAY = 1;

    public final int DISPLAY_OVERSCAN_X = 0;
    public final int DISPLAY_OVERSCAN_Y = 1;
    public final int DISPLAY_OVERSCAN_LEFT = 2;
    public final int DISPLAY_OVERSCAN_RIGHT = 3;
    public final int DISPLAY_OVERSCAN_TOP = 4;
    public final int DISPLAY_OVERSCAN_BOTTOM = 5;
    public final int DISPLAY_OVERSCAN_ALL = 6;

    private final int DEFAULT_BRIGHTNESS = 50;
    private final int DEFAULT_CONTRAST = 50;
    private final int DEFAULT_SATURATION = 50;
    private final int DEFAULT_HUE = 50;

    private int timeline=0;
    /**
     * Binder context for this service
     */
    private Context mContext;

    private NativeDaemonConnector mConnector;

    class DisplaydResponseCode {
        public static final int InterfaceListResult        = 110;
        public static final int ModeListResult            = 111;
        public static final int CommandOkay                = 200;
        public static final int OperationFailed         = 400;
        public static final int InterfaceChange            = 600;
    }

    /**
     * Constructs a new NetworkManagementService instance
     *
     * @param context  Binder context for this service
     */
    public RkDisplayDeviceManagementService(Context context) {
        mContext = context;
        mdrmModes = new RkDisplayModes();
        mdrmModes.init();
        IntentFilter hdmiFilter = new IntentFilter();
        hdmiFilter.addAction(ACTION_PLUGGED);
        mHdmiReceiver = new HdmiReceiver(mdrmModes);
        mContext.registerReceiver(mHdmiReceiver,hdmiFilter);
    }

    public String[] listInterfaces(int display) {
        String[] ifaces = new String[1];
        ifaces[0] = mdrmModes.getbuild_in(display);
        return ifaces;
    }

    public String getCurrentInterface(int display){
        return mdrmModes.getbuild_in(display);
    }

    public int getDisplayNumbers(){
        return mdrmModes.getNumConnectors();
    }

    public String[] getModelist(int display, String iface){
        List<String> hdmiResoList = mdrmModes.getModeList(display);
        if (hdmiResoList != null)
            return hdmiResoList.toArray(new String[hdmiResoList.size()]);
        else
            return  null;
    }

    public String getMode(int display, String iface){
        String mode = mdrmModes.getCurMode(display);
        Log.d(TAG, "getMode: " + mode + " display " +display);
        return mode;
    }

    public int getDpyConnState(int display){
        return mdrmModes.getConnectionState(display);
    }

    public void setMode(int display, String iface, String mode) {
        boolean isSameProperty = false;
        String lastMode;

        mdrmModes.setMode(display, iface, mode);
    }

    public String[] getSupportCorlorList(int display, String iface){
        List<String> corlorList = mdrmModes.getSupportCorlorList(display);
        if (corlorList != null)
            return corlorList.toArray(new String[corlorList.size()]);
        else
            return null;
    }

    public String getCurColorMode(int display, String iface){
        String colorMode = mdrmModes.getCurColorMode(display);
        return colorMode;
    }

    public void setColorMode(int display, String iface, String format){
        mdrmModes.setColorMode(display, format);
    }

    public void setHdrMode(int display, String iface, int hdrMode){
        mdrmModes.setHdrMode(display, hdrMode);
    }

    public void setScreenScale(int display, int direction, int value){
        mdrmModes.setScreenScale(display, direction, value);
    }

    public void setDisplaySize(int display, int width, int height){
    }

    public int get3DModes(int display, String iface){
        return 0;
    }

    public int getCur3DMode(int display, String iface){
        return 0;
    }

    public void set3DMode(int display, String iface, int mode) {
    }

    public void setBrightness(int display, int brightness) {
        mdrmModes.setBrightness(display, brightness);
    }

    public void setContrast(int display, int contrast)  {
        mdrmModes.setContrast(display, contrast);
    }

    public void setSaturation(int display, int saturation) {
        mdrmModes.setSaturation(display, saturation);
    }

    public void setHue(int display, int degree){
        mdrmModes.setHue(display, degree);
    }

    public int[] getBcsh(int display) {
        int[] bcsh = new int[4];
        bcsh = mdrmModes.getBcsh(display);
        Log.d(TAG, "getBcsh: " +  bcsh[0] + " " + bcsh[1] + " " +  bcsh[2] + " " + bcsh[3]);
        return bcsh;
    }

    public int[] getOverscan(int display) {
        int[] mOverscan = new int[4];
        mOverscan = mdrmModes.getOverscan(display);
        Log.d(TAG, "getOverscan: " +  mOverscan[0] + " " + mOverscan[1] + " " +  mOverscan[2] + " " + mOverscan[3]);
        return mOverscan;
    }

    public int setGamma(int dpy,int size,int[] red, int[] green, int[] blue){
        return mdrmModes.setGamma(dpy, size, red, green, blue);
    }

    public int set3DLut(int dpy,int size,int[] red, int[] green, int[] blue){
        return mdrmModes.set3DLut(dpy, size, red, green, blue);
    }

    public void updateDisplayInfos(){
        mdrmModes.updateDisplayInfos();
    }

    public int saveConfig() {
        mdrmModes.saveConfig();
        return 0;
    }

    public String[] getConnectorInfo(){
        RkConnectorInfo[] info = mdrmModes.getConnectorInfo();
        String[] ret = new String[info.length];
        for(int i = 0; i< info.length; i++){
            ret[i] = "type:" + info[i].type + ",id:" + info[i].id + ",state:" + info[i].state;
        }
        return ret;
    }

    public int updateDispHeader(){
        return mdrmModes.updateDispHeader();
    }

}
