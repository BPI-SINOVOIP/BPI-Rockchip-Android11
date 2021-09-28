/* $_FOR_ROCKCHIP_RBOX_$ */
//$_rbox_$_modify_$_zhengyang_20120220: Rbox android display manager class

/*
 * Copyright (C) 2007 The Android Open Source Project
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

package android.os;

import android.content.Context;
import android.os.IBinder;
import android.os.IRkDisplayDeviceManagementService;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.util.Log;
import android.view.IWindowManager;
import android.view.Display;
import android.graphics.Rect;

/**
 * @hide
 */
public class RkDisplayOutputManager {
    private static final String TAG = "RkDisplayOutputManager";
    private final boolean DBG = true;
    public final int MAIN_DISPLAY = 0;
    public final int AUX_DISPLAY = 1;

    public final int DRM_MODE_CONNECTOR_Unknown = 0;
    public final int DRM_MODE_CONNECTOR_VGA = 1;
    public final int DRM_MODE_CONNECTOR_DVII = 2;
    public final int DRM_MODE_CONNECTOR_DVID =	3;
    public final int DRM_MODE_CONNECTOR_DVIA =	4;
    public final int DRM_MODE_CONNECTOR_Composite =	5;
    public final int DRM_MODE_CONNECTOR_SVIDEO =	6;
    public final int DRM_MODE_CONNECTOR_LVDS = 7;
    public final int DRM_MODE_CONNECTOR_Component = 8;
    public final int DRM_MODE_CONNECTOR_9PinDIN	= 9;
    public final int DRM_MODE_CONNECTOR_DisplayPort = 10;
    public final int DRM_MODE_CONNECTOR_HDMIA = 11;
    public final int DRM_MODE_CONNECTOR_HDMIB = 12;
    public final int DRM_MODE_CONNECTOR_TV = 13;
    public final int DRM_MODE_CONNECTOR_eDP = 14;
    public final int DRM_MODE_CONNECTOR_VIRTUAL = 15;
    public final int DRM_MODE_CONNECTOR_DSI = 16;

    private final String DISPLAY_TYPE_UNKNOW = "UNKNOW";
    private final String DISPLAY_TYPE_VGA = "VGA";
    private final String DISPLAY_TYPE_DVII = "DVII";
    private final String DISPLAY_TYPE_DVID = "DVID";
    private final String DISPLAY_TYPE_DVIA = "DVIA";
    private final String DISPLAY_TYPE_Composite = "Composite";
    private final String DISPLAY_TYPE_LVDS = "LVDS";
    private final String DISPLAY_TYPE_Component = "Component";
    private final String DISPLAY_TYPE_9PinDIN = "9PinDIN";
    //private final String DISPLAY_TYPE_YPbPr = "YPbPr";
    private final String DISPLAY_TYPE_DP = "DP";
    private final String DISPLAY_TYPE_HDMIA = "HDMIA";
    private final String DISPLAY_TYPE_HDMIB = "HDMIB";
    private final String DISPLAY_TYPE_TV = "TV";
    private final String DISPLAY_TYPE_EDP = "EDP";
    private final String DISPLAY_TYPE_VIRTUAL = "VIRTUAL";
    private final String DISPLAY_TYPE_DSI = "DSI";
    public final String COLOR_FORMAT_RGB = "RGB";

    public final String DISPLAY_TYPE_HDMI = "HDMI";
    public final int DISPLAY_OVERSCAN_X = 0;
    public final int DISPLAY_OVERSCAN_Y = 1;
    public final int DISPLAY_OVERSCAN_LEFT = 2;
    public final int DISPLAY_OVERSCAN_RIGHT = 3;
    public final int DISPLAY_OVERSCAN_TOP = 4;
    public final int DISPLAY_OVERSCAN_BOTTOM = 5;
    public final int DISPLAY_OVERSCAN_ALL = 6;

    public final int DISPLAY_3D_NONE = -1;
    public final int DISPLAY_3D_FRAME_PACKING = 0;
    public final int DISPLAY_3D_TOP_BOTTOM = 6;
    public final int DISPLAY_3D_SIDE_BY_SIDE_HALT = 8;

    public final int DRM_MODE_CONNECTED         = 1;
    public final int DRM_MODE_DISCONNECTED      = 2;
    public final int DRM_MODE_UNKNOWNCONNECTION = 3;

    public final int HDR_CLOSE = 0;
    public final int HDR_OPEN = 1;
    public final int HDR_AUTO = 2;

    private int m_main_iface[] = null;
    private int m_aux_iface[] = null;

    private IRkDisplayDeviceManagementService mService;

    /**
    * @throws RemoteException
    * @hide
    */
    public RkDisplayOutputManager() {
        IBinder b = ServiceManager.getService("drm_device_management");
        if(b == null) {
            Log.e(TAG, "Unable to connect to display device management service! - is it running yet?");
            return;
        }
        mService = IRkDisplayDeviceManagementService.Stub.asInterface(b);

        int mMainState = getCurrentDpyConnState(MAIN_DISPLAY);
        try {
            // Get main display interface
            String[] display_iface = mService.listInterfaces(MAIN_DISPLAY);
            if(DBG) Log.d(TAG, "main display iface num is " + display_iface.length);
            if(display_iface != null && display_iface.length > 0 &&  mMainState==DRM_MODE_CONNECTED) {
                m_main_iface = new int[display_iface.length];
                for(int i = 0; i < m_main_iface.length; i++) {
                if(DBG) Log.d(TAG, display_iface[i]);
                    m_main_iface[i] = ifacetotype(display_iface[i]);
                }
            }
            else
                m_main_iface = null;
        } catch (Exception e) {
            Log.e(TAG, "Error listing main interfaces :" + e);
        }

        int mAuxState = getCurrentDpyConnState(AUX_DISPLAY);
        try {
            // Get aux display interface
            String[] display_iface = mService.listInterfaces(AUX_DISPLAY);
            if(DBG) Log.d(TAG, "aux display iface num is " + display_iface.length);
            if(display_iface != null && display_iface.length > 0 && mAuxState==DRM_MODE_CONNECTED) {
                m_aux_iface = new int[display_iface.length];
                for(int i = 0; i < m_aux_iface.length; i++) {
                    if(DBG) Log.d(TAG, display_iface[i]);
                    m_aux_iface[i] = ifacetotype(display_iface[i]);
                }
            }
            else
                m_aux_iface = null;
        } catch (Exception e) {
            Log.e(TAG, "Error listing aux interfaces :" + e);
        }

        if (m_main_iface == null && m_aux_iface == null)
            Log.e(TAG, "There is no display interface.");

    }

    private int ifacetotype(String iface) {
        int ifaceType;
        if(iface.equals(DISPLAY_TYPE_UNKNOW)) {
            ifaceType = DRM_MODE_CONNECTOR_Unknown;
        } else if(iface.equals(DISPLAY_TYPE_DVII)) {
            ifaceType = DRM_MODE_CONNECTOR_DVII;
        } else if(iface.equals(DISPLAY_TYPE_DVID)) {
            ifaceType = DRM_MODE_CONNECTOR_DVID;
        }else if(iface.equals(DISPLAY_TYPE_DVIA)) {
            ifaceType = DRM_MODE_CONNECTOR_DVIA;
        } else if(iface.equals(DISPLAY_TYPE_Composite)) {
            ifaceType = DRM_MODE_CONNECTOR_Composite;
        }else if(iface.equals(DISPLAY_TYPE_VGA)) {
            ifaceType = DRM_MODE_CONNECTOR_VGA;
        } else if(iface.equals(DISPLAY_TYPE_LVDS)) {
            ifaceType = DRM_MODE_CONNECTOR_LVDS;
        } else if(iface.equals(DISPLAY_TYPE_Component)) {
            ifaceType = DRM_MODE_CONNECTOR_Component;
        } else if(iface.equals(DISPLAY_TYPE_9PinDIN)) {
            ifaceType = DRM_MODE_CONNECTOR_9PinDIN;
        } else if(iface.equals(DISPLAY_TYPE_DP)) {
            ifaceType = DRM_MODE_CONNECTOR_DisplayPort;
        } else if(iface.equals(DISPLAY_TYPE_HDMIA)) {
            ifaceType = DRM_MODE_CONNECTOR_HDMIA;
        } else if(iface.equals(DISPLAY_TYPE_HDMIB)) {
            ifaceType = DRM_MODE_CONNECTOR_HDMIB;
        } else if(iface.equals(DISPLAY_TYPE_TV)) {
            ifaceType = DRM_MODE_CONNECTOR_TV;
        } else if(iface.equals(DISPLAY_TYPE_EDP)) {
            ifaceType = DRM_MODE_CONNECTOR_eDP;
        } else if(iface.equals(DISPLAY_TYPE_VIRTUAL)) {
            ifaceType = DRM_MODE_CONNECTOR_VIRTUAL;
        } else if(iface.equals(DISPLAY_TYPE_DSI)) {
            ifaceType = DRM_MODE_CONNECTOR_DSI;
        } else {
            ifaceType = 0;
        }
        return ifaceType;
    }

    public String typetoface(int type) {
        String iface;

        if(type == DRM_MODE_CONNECTOR_Unknown)
            iface = DISPLAY_TYPE_UNKNOW;
        else if(type == DRM_MODE_CONNECTOR_DVII)
            iface = DISPLAY_TYPE_DVII;
        else if(type == DRM_MODE_CONNECTOR_VGA)
            iface = DISPLAY_TYPE_VGA;
        else if(type == DRM_MODE_CONNECTOR_DVID)
            iface = DISPLAY_TYPE_DVID;
        else if(type == DRM_MODE_CONNECTOR_DVIA)
            iface = DISPLAY_TYPE_DVIA;
        else if(type == DRM_MODE_CONNECTOR_Composite)
            iface = DISPLAY_TYPE_Composite;
        else if(type == DRM_MODE_CONNECTOR_LVDS)
            iface = DISPLAY_TYPE_LVDS;
        else if(type == DRM_MODE_CONNECTOR_9PinDIN)
            iface = DISPLAY_TYPE_9PinDIN;
        else if(type == DRM_MODE_CONNECTOR_Component)
            iface = DISPLAY_TYPE_Component;
        else if(type == DRM_MODE_CONNECTOR_DisplayPort)
            iface = DISPLAY_TYPE_DP;
        else if(type == DRM_MODE_CONNECTOR_HDMIA)
            iface = DISPLAY_TYPE_HDMI;//DISPLAY_TYPE_HDMIA
        else if(type == DRM_MODE_CONNECTOR_HDMIB)
            iface = DISPLAY_TYPE_HDMI;//DISPLAY_TYPE_HDMIB
        else if(type == DRM_MODE_CONNECTOR_TV)
            iface = DISPLAY_TYPE_TV;
        else if(type == DRM_MODE_CONNECTOR_eDP)
            iface = DISPLAY_TYPE_EDP;
        else if(type == DRM_MODE_CONNECTOR_VIRTUAL)
            iface = DISPLAY_TYPE_VIRTUAL;
        else if(type == DRM_MODE_CONNECTOR_DSI)
            iface = DISPLAY_TYPE_DSI;
        else
            return null;

        return iface;

    }

    public int getCurrentDpyConnState(int display){
        int state=2;
        try {
            state = mService.getDpyConnState(display);
            return state;
        } catch (Exception e) {
            Log.e(TAG, "Error get getDisplayNumber :" + e);
            return 2;
        }
    }

    /**
     *
     * @return
     * @hide
     */
    public int getDisplayNumber() {
        int number = 0;

        try {
            number = mService.getDisplayNumbers();
            return number;
        } catch (Exception e) {
            Log.e(TAG, "Error get getDisplayNumber :" + e);
            return 0;
        }
    }

    /**
    *
    * @param display
    * @return
    * @hide
    */
    public int[] getIfaceList(int display) {
        if(display == MAIN_DISPLAY)
            return m_main_iface;
        else if(display == AUX_DISPLAY)
            return m_aux_iface;
        else
            return null;
    }

    /**
    *
    * @param display
    * @return
    * @hide
    */
    public int getCurrentInterface(int display) {
        try {
            String iface = mService.getCurrentInterface(display);
            return ifacetotype(iface);
        } catch (Exception e) {
            Log.e(TAG, "Error get current Interface :" + e);
            return 0;
        }
    }

    /**
    *
    * @param display
    * @param type
    * @return
    * @hide
    */
    public String[] getModeList(int display, int type) {
        String iface = typetoface(type);
        try {
            return mService.getModelist(display, iface);
        } catch (Exception e) {
            Log.e(TAG, "Error get list mode :" + e);
            return null;
        }
    }

    /**
    *
    * @param display
    * @param type
    * @return
    * @hide
    */
    public String getCurrentMode(int display, int type) {
        String iface = typetoface(type);
        try {
            return mService.getMode(display, iface);
        } catch (Exception e) {
            Log.e(TAG, "Error get current mode :" + e);
            return null;
        }
    }

    public String getCurrentColorMode(int display, int type) {
        String iface = typetoface(type);
        try {
            return mService.getCurColorMode(display, iface);
        } catch (Exception e) {
            Log.e(TAG, "Error get current color mode :" + e);
            return null;
        }
    }

    public void setColorMode(int display, int type, String format) {
        String iface = typetoface(type);
        try {
            mService.setColorMode(display, iface, format);
        }catch (Exception e) {
            Log.e(TAG, "Error set mode :" + e);
            return;
        }
    }

    /**
    *
    * @param display
    * @param type
    * @return
    * @hide
    */
    public String[] getSupportCorlorList(int display, int type) {
        String iface = typetoface(type);
        try {
            return mService.getSupportCorlorList(display, iface);
        } catch (Exception e) {
            Log.e(TAG, "Error get list mode :" + e);
            return null;
        }
    }

    public void setHdrMode(int display, int type, int hdrMode){
        String iface = typetoface(type);
        try {
            mService.setHdrMode(display, iface, hdrMode);
        }catch (Exception e) {
            Log.e(TAG, "Error set mode :" + e);
            return;
        }
    }

    /**
    *
    * @param display
    * @param type
    * @param mode
    * @hide
    */
    public void setMode(int display, int type, String mode) {
        String iface = typetoface(type);

        try {
            mService.setMode(display, iface, mode);
        }catch (Exception e) {
            Log.e(TAG, "Error set mode :" + e);
            return;
        }
    }

    /**
    *
    * @param display
    * @param direction
    * @param value
    * @hide
    */
    public void setOverScan(int display, int direction, int value) {
        try {
            mService.setScreenScale(display, direction, value);
        }catch (Exception e) {
            Log.e(TAG, "Error setScreenScale :" + e);
            return;
        }
    }

    /**
    *
    * @param display
    * @return
    * @hide
    */
    public Rect getOverScan(int display) {
        int[] overscan = new int[4];
        try {
            overscan = mService.getOverscan(display);
            Log.d(TAG, "getOverscan :" + overscan[0] +","+ overscan[1] +","+ overscan[2] + ","+overscan[3]);
        }catch (RemoteException e) {
            Log.e(TAG, "Error getOverScan :" + e);
        }
        if (overscan[0] != 0 && overscan[1]!=0 && overscan[2] != 0 && overscan[3] != 0)
            return new Rect(overscan[0], overscan[1], overscan[2], overscan[3]);
        return new Rect(100,100,100,100);
    }

    /**
    *
    * @param display
    * @param width
    * @param height
    * @hide
    */
    public void setDisplaySize(int display, int width, int height)
    {
        String displaypolicy = SystemProperties.get("persist.sys.display.policy", "manual");
        if(displaypolicy.equals("auto") == true) {
            if (width >= 0 && height >= 0) {
                try {
                    Log.d(TAG, "setDisplaySize " + display + " " + width + " " + height);
                    mService.setDisplaySize(display, width, height);
                } catch (RemoteException e) {
                    Log.e(TAG, "Error setFramebufferSize :" + e);
                }
            }
        }
    }

    private int setWMDisplaySize(int display, int width, int height)
    {
        String displaypolicy = SystemProperties.get("persist.sys.display.policy", "manual");

        if(displaypolicy.equals("manual") == true)
            return -1;

        return 0;
    }

    /**
    *
    * @param display
    * @param type
    * @return
    * @hide
    */
    public int get3DModes(int display, int type)
    {
        String iface = typetoface(type);
        if(iface.equals(null))
            return 0;

        try {
            return mService.get3DModes(display, iface);
        } catch (Exception e) {
            Log.e(TAG, "Error get 3d modes :" + e);
            return 0;
        }
    }

    /**
    *
    * @param display
    * @param type
    * @return
    * @hide
    */
    public int getCur3DMode(int display, int type)
    {
        String iface = typetoface(type);
        if(iface.equals(null))
            return -1;

        try {
            return mService.getCur3DMode(display, iface);
        } catch (Exception e) {
            Log.e(TAG, "Error get cur 3d mode :" + e);
            return -1;
        }
    }

    /**
    *
    * @param display
    * @param type
    * @param mode
    * @hide
    */
    public void set3DMode(int display, int type, int mode)
    {
        String iface = typetoface(type);
        if(iface.equals(null))
            return;

        try {
            mService.set3DMode(display, iface, mode);
        } catch (Exception e) {
            Log.e(TAG, "Error set 3d modes :" + e);
            return;
        }
    }

    /**
    *
    * @return
    * @hide
    */
    public int saveConfig()
    {
        try {
            return mService.saveConfig();
        } catch (Exception e) {
            Log.e(TAG, "Error save :" + e);
            return -1;
        }
    }

    /**
    *
    * @return
    * @hide
    */
    public int updateDisplayInfos()
    {
        try {
            mService.updateDisplayInfos();
        } catch (Exception e) {
            Log.e(TAG, "Error save :" + e);
            return -1;
        }
	return 0;
    }

    /*
    * brightness: [0, 100], default 50
    */
    /**
    *
    * @param display
    * @param brightness
    * @return
    * @hide
    */
    public int setBrightness(int display, int bright_percent)
    {
        if (bright_percent < 0 || bright_percent > 100) {
            Log.e(TAG, "setBrightness out of range " + bright_percent);
            return -1;
        }
        try {
            mService.setBrightness(display, bright_percent);
        } catch (Exception e) {
            Log.e(TAG, "Error set brightness :" + e);
            return -1;
        }
        return 0;
    }

    /*
    * contrast: [0, 100], default 50;
    */
    /**
    *
    * @param display
    * @param contrast
    * @return
    * @hide
    */
    public int setContrast(int display, int con_percent)
    {
        if (con_percent < 0 || con_percent > 100) {
            Log.e(TAG, "setContrast out of range " + con_percent);
            return -1;
        }
        try {
            mService.setContrast(display, con_percent);
        } catch (Exception e) {
            Log.e(TAG, "Error set Contrast :" + e);
            return -1;
        }
        return 0;
    }

    /*
    * saturation: [0, 100], default 50;
    */
    /**
    *
    * @param display
    * @param saturation
    * @return
    * @hide
    */
    public int setSaturation(int display, int sat_percent)
    {
        if (sat_percent < 0 || sat_percent > 100) {
            Log.e(TAG, "setContrast out of range " + sat_percent);
            return -1;
        }
        try {
            mService.setSaturation(display, sat_percent);
        } catch (Exception e) {
            Log.e(TAG, "Error set sat_con :" + e);
            return -1;
        }
        return 0;
    }

    /*
    * degree: [0, 100], default 0
    */
    /**
    *
    * @param display
    * @param degree
    * @return
    * @hide
    */
    public int setHue(int display, int degree_percent)
    {
        if (degree_percent < 0 || degree_percent > 100) {
            Log.e(TAG, "Error set hue out of range " + degree_percent);
            return -1;
        }
        try {
            mService.setHue(display, degree_percent);
        } catch (Exception e) {
            Log.e(TAG, "Error set hue :" + e);
            return -1;
        }
        return 0;
    }

    /**
    *
    * @param display
    * @return brightness
    * @hide
    */
    public int getBrightness(int display){
        int[] bcsh = new int[4];
        bcsh = getBcsh(display);
        return bcsh[0];
    }

    /**
    *
    * @param display
    * @return display
    * @hide
    */
    public int getContrast(int display){
        int[] bcsh = new int[4];
        bcsh = getBcsh(display);
        return bcsh[1];
    }

    /**
    *
    * @param display
    * @return saturation
    * @hide
    */
    public int getSaturation(int display){
        int[] bcsh = new int[4];
        bcsh = getBcsh(display);
        return bcsh[2];
    }

    /**
    *
    * @param display
    * @return Hue
    * @hide
    */
    public int getHue(int display){
        int[] bcsh = new int[4];
        bcsh = getBcsh(display);
        return bcsh[3];
    }

    /**
    *
    * @param display
    * @return order:brightness, con, sat, hue
    * @hide
    */
    public int[] getBcsh(int display)
    {
        try {
            return mService.getBcsh(display);
        } catch (Exception e) {
            Log.e(TAG, "Error get brightness :" + e);
            int[] bcsh = new int[4];
			bcsh[0]=bcsh[1]=bcsh[2]=bcsh[3]=50;
            return bcsh;
        }
    }

    public int setGamma(int display, int size, int[] red, int[] green, int[] blue){
        int ret;
        try {
            ret = mService.setGamma(display, size, red, green, blue);
        } catch (Exception e) {
            Log.e(TAG, "Error set gamma :" + e);
            return -1;
        }
        return ret;
    }

    public int set3DLut(int display, int size, int[] red, int[] green, int[] blue){
        int ret;
        try {
            ret = mService.set3DLut(display, size, red, green, blue);
        } catch (Exception e) {
            Log.e(TAG, "Error set 3DLut :" + e);
            return -1;
        }
        return ret;
    }

    public String[] getConnectorInfo(){
        String[] ret;
        try {
            ret = mService.getConnectorInfo();
        } catch (Exception e) {
            Log.e(TAG, "Error getConnectorInfo :" + e);
            return null;
        }
        return ret;
    }

    public int updateDispHeader(){
        int ret;
        try {
            ret = mService.updateDispHeader();
        } catch (Exception e) {
            Log.e(TAG, "Error updateDispHeader :" + e);
            return -1;
        }
        return ret;
    }
}
