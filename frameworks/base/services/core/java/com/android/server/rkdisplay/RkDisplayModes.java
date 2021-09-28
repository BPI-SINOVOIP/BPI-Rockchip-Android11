package com.android.server.rkdisplay;

import dalvik.system.CloseGuard;
import android.util.Log;
import android.view.Surface.OutOfResourcesException;
import java.util.ArrayList;
import java.util.List;
import java.io.InputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.io.FileNotFoundException;
import android.os.SystemProperties;
import java.io.FileReader;
import java.io.FileNotFoundException;

public class RkDisplayModes {
    private static final String TAG = "RkDisplayModes";
    private static native RkDisplayModes.RkPhysicalDisplayInfo[] nativeGetDisplayConfigs(
    int dpy);
    private static native RkDisplayModes.RkColorCapacityInfo nativeGetCorlorModeConfigs(int dpy);
    private static native void nativeInit();
    private static native void nativeUpdateConnectors();
    private static native void nativeSaveConfig();
    private static native int nativeGetNumConnectors();
    private static native void nativeSetMode(int dpy, int iface_type, String mode);
    private static native String nativeGetCurMode(int dpy);
    private static native String nativeGetCurCorlorMode(int dpy);
    private static native int nativeGetBuiltIn(int dpy);
    private static native int nativeGetConnectionState(int dpy);
    private static native int[] nativeGetBcsh(int dpy);
    private static native int[] nativeGetOverscan(int dpy);
    private static native int nativeSetGamma(int dpy, int size, int[] r, int[] g, int[] b);
    private static native int nativeSet3DLut(int dpy, int size, int[] r, int[] g, int[] b);
    private static native int nativeSetHue(int display, int degree);
    private static native int nativeSetSaturation(int display, int saturation);
    private static native int nativeSetContrast(int display, int contrast);
    private static native int nativeSetBrightness(int display, int brightness);
    private static native int nativeSetScreenScale(int display, int direction, int value);
    private static native int nativeSetHdrMode(int display, int hdrMode);
    private static native int nativeSetColorMode(int display, String format);
    private static native RkDisplayModes.RkConnectorInfo[] nativeGetConnectorInfo();
    private static native int nativeUpdateDispHeader();


    private static RkDisplayModes.RkPhysicalDisplayInfo mDisplayInfos[];
    private static RkDisplayModes.RkPhysicalDisplayInfo mMainDisplayInfos[];
    private static RkDisplayModes.RkPhysicalDisplayInfo mAuxDisplayInfos[];
    private static List<String> mWhiteList;
    private static List<ResolutionParser.RkResolutionInfo> resolutions=null;
    private static ResolutionParser mParser=null;
    private static RkDisplayModes.RkColorCapacityInfo mMainColorInfos;
    private static RkDisplayModes.RkColorCapacityInfo mAuxColorInfos;

    public final int DRM_MODE_CONNECTOR_Unknown = 0;
    public final int DRM_MODE_CONNECTOR_VGA = 1;
    public final int DRM_MODE_CONNECTOR_DVII = 2;
    public final int DRM_MODE_CONNECTOR_DVID = 3;
    public final int DRM_MODE_CONNECTOR_DVIA = 4;
    public final int DRM_MODE_CONNECTOR_Composite =    5;
    public final int DRM_MODE_CONNECTOR_SVIDEO = 6;
    public final int DRM_MODE_CONNECTOR_LVDS = 7;
    public final int DRM_MODE_CONNECTOR_Component = 8;
    public final int DRM_MODE_CONNECTOR_9PinDIN    = 9;
    public final int DRM_MODE_CONNECTOR_DisplayPort = 10;
    public final int DRM_MODE_CONNECTOR_HDMIA = 11;
    public final int DRM_MODE_CONNECTOR_HDMIB = 12;
    static final int DRM_MODE_CONNECTOR_TV = 13;
    public final int DRM_MODE_CONNECTOR_eDP = 14;
    public final int DRM_MODE_CONNECTOR_VIRTUAL = 15;
    public final int DRM_MODE_CONNECTOR_DSI = 16;

    private final int MAIN_DISPLAY = 0;
    private final int AUX_DISPLAY = 1;

    private final String DISPLAY_TYPE_UNKNOW = "UNKNOW";
    private final String DISPLAY_TYPE_VGA = "VGA";
    private final String DISPLAY_TYPE_DVII = "DVII";
    private final String DISPLAY_TYPE_DVID = "DVID";
    private final String DISPLAY_TYPE_DVIA = "DVIA";
    private final String DISPLAY_TYPE_Composite = "Composite";
    private final String DISPLAY_TYPE_SVideo = "SVideo";
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

    private static final String MODE_TYPE_BUILTIN = "builtin";
    private static final String MODE_TYPE_CLOCKC = "clock_c";
    private static final String MODE_TYPE_CRTCC = "crtc_c";
    private static final String MODE_TYPE_PREFER = "preferred";
    private static final String MODE_TYPE_DEFAULT = "default";
    private static final String MODE_TYPE_USERDEF = "userdef";
    private static final String MODE_TYPE_DRIVER = "driver";

    public static int DRM_HDMI_OUTPUT_DEFAULT_RGB = 1<<0; /* default RGB */
    public static int DRM_HDMI_OUTPUT_YCBCR444 = 1<<1; /* YCBCR 444 */
    public static int DRM_HDMI_OUTPUT_YCBCR422 = 1<<2; /* YCBCR 422 */
    public static int DRM_HDMI_OUTPUT_YCBCR420 = 1<<3; /* YCBCR 420 */

    public static int DEPTH_CAPA_BIT0_8BIT = 1<<0;
    public static int DEPTH_CAPA_BIT1_10BIT = 1<<1;
    public static int DEPTH_CAPA_BIT4_420_10BIT = 1<<4;

    public static int ROCKCHIP_DEPTH_DEFAULT = 0;
    public static int ROCKCHIP_HDMI_DEPTH_8 = 8;
    public static int ROCKCHIP_HDMI_DEPTH_10 = 10;

    private  static String STR_YCBCR444 = "YCBCR444";
    private  static String STR_YCBCR422 = "YCBCR422";
    private  static String STR_YCBCR420 = "YCBCR420";
    private  static String STR_RGB = "RGB";
    private  static String STR_DEPTH_8BIT = "8bit";
    private  static String STR_DEPTH_10BIT = "10bit";

    private static final  String RESOLUTION_XML_PATH = "/system/usr/share/resolution_white.xml";
    private final  String HDMI_DBG_STATUS = "/d/dw-hdmi/status";

    public RkDisplayModes(){
        mWhiteList = new ArrayList<>();
        //resolutions = new ArrayList<ResolutionParser.RkResolutionInfo>();
        mParser = new ResolutionParser();
        readModeWhiteList(RESOLUTION_XML_PATH);
    }

    private String readColorFormatFromNode(){
        FileReader fr=null;
        BufferedReader br=null;
        String line="";

        try {
            fr = new FileReader(HDMI_DBG_STATUS);
        } catch (FileNotFoundException e) {
            Log.e(TAG, "Couldn't find or open  file "+HDMI_DBG_STATUS, e);
            return null;
        }
        br=new BufferedReader(fr);
        try {
            while ((line=br.readLine())!=null) {
                if (line.contains("Color Format:")) {
                    StringBuilder builder = new StringBuilder();
                    int start = line.indexOf("Format:");
                    int end = line.indexOf(" ", start+8);
                    int depthStart = line.indexOf("Depth:");
                    int depthEnd = line.indexOf(" ", depthStart+7);
                    String format = line.substring(start+7, end);
                    String depth = line.substring(depthStart+7, depthEnd);
                    if (format.contains("YUV444"))
                        builder.append(STR_YCBCR444).append("-").append(depth).append("bit");
                    else if (format.contains("YUV422"))
                       builder.append(STR_YCBCR422).append("-").append(depth).append("bit");
                    else if (format.contains("YUV420"))
                       builder.append(STR_YCBCR420).append("-").append(depth).append("bit");
                    else
                       builder.append(format).append("-").append(depth).append("bit");
                    Log.e(TAG, "readColorFormatFromNode :" + builder.toString());
                    return builder.toString();
                }
            }
            br.close();
            fr.close();
            return null;
        } catch (IOException e) {
            Log.e(TAG, "Couldn't read  data from /d/dw-hdmi/status", e);
            return null;
        }
    }

    private  static boolean readModeWhiteList(String pathname) {
        InputStream is;
        try {
            is = new FileInputStream(RESOLUTION_XML_PATH);
        } catch (FileNotFoundException e) {
            Log.e(TAG, "Couldn't find " + RESOLUTION_XML_PATH + ".");
            return false;
        }
        resolutions=null;
        try {
            resolutions = mParser.parse(is);
            is.close();
        } catch (Exception e) {
            Log.e(TAG, "Couldn't get resolutions path  " + RESOLUTION_XML_PATH + ".");
            return false;
        }
        //logd("readStrListFromFile - " + fileStrings.toString());
        return true;
    }

    private boolean IsResolutionNeedFilter(int dpy) {
        int type = nativeGetBuiltIn(dpy);
        return type==DRM_MODE_CONNECTOR_Unknown || type==DRM_MODE_CONNECTOR_Composite ||
               type==DRM_MODE_CONNECTOR_SVIDEO || type==DRM_MODE_CONNECTOR_LVDS ||
               type==DRM_MODE_CONNECTOR_TV || type==DRM_MODE_CONNECTOR_VIRTUAL ||
               type==DRM_MODE_CONNECTOR_DSI;
    }

    public  List<String> getModeList(int dpy){
        List<String> modeList = new ArrayList<>();
        Log.e(TAG, "getModeList =========== dpy " + dpy);
        //readModeWhiteList(RESOLUTION_XML_PATH);
        mDisplayInfos = getDisplayConfigs(dpy);
        if (dpy == 0)
            mMainDisplayInfos = mDisplayInfos;
        else
            mAuxDisplayInfos = mDisplayInfos;

        if (mDisplayInfos != null) {
            modeList.add("Auto");
            for (int i = 0; i < mDisplayInfos.length; i++){
                boolean found=false;
                RkDisplayModes.RkPhysicalDisplayInfo info = mDisplayInfos[i];
                StringBuilder builder = new StringBuilder();
                builder.append(info.width).append("x").append(info.height);
                if (info.interlaceFlag == true) {
                    builder.append("i");
                    builder.append(String.format("%.2f", info.refreshRate));
                } else {
                    builder.append("p");
                    builder.append(String.format("%.2f", info.refreshRate));
                }
                //builder.append("@");
                builder.append("-").append(info.idx);

                boolean existingMode = false;
                if (resolutions == null || IsResolutionNeedFilter(dpy)) {
                    modeList.add(builder.toString());
                } else {
                    for (int j = 0; j < resolutions.size(); j++) {
                        if (resolutions.get(j).hasMatchingMode(info)) {
                            existingMode = true;
                            break;
                        }
                    }
                    if (existingMode) {
                        modeList.add(builder.toString());
                    }
                }
            }
        } else {
            modeList = null;
        }

        return modeList;
    }

    /**
    * Describes the properties of a physical display known to surface flinger.
    */
    public static final class RkPhysicalDisplayInfo {
        public int width;
        public int height;
        public float refreshRate;
        public int clock;
        public int flags;
        public boolean interlaceFlag;
        public boolean yuvFlag;
        public int connectorId;
        public int mode_type;
        public int idx;
        public int hsync_start;
        public int hsync_end;
        public int htotal;
        public int hskew;
        public int vsync_start;
        public int vsync_end;
        public int vtotal;
        public int vscan;

        public RkPhysicalDisplayInfo() {
        }

        public RkPhysicalDisplayInfo(RkPhysicalDisplayInfo other) {
            copyFrom(other);
        }

        @Override
        public boolean equals(Object o) {
            return o instanceof RkPhysicalDisplayInfo && equals((RkPhysicalDisplayInfo)o);
        }

        public boolean equals(RkPhysicalDisplayInfo other) {
            return other != null
                && width == other.width
                && height == other.height
                && refreshRate == other.refreshRate
                && clock == other.clock
                && flags == other.flags;
        }

        @Override
        public int hashCode() {
            return 0; // don't care
        }

        public void copyFrom(RkPhysicalDisplayInfo other) {
            width = other.width;
            height = other.height;
            refreshRate = other.refreshRate;
            clock = other.clock;
            flags = other.flags;
        }

        // For debugging purposes
        @Override
        public String toString() {
            return "PhysicalDisplayInfo{" + width + " x " + height + ", " + refreshRate + " fps, "
                + "clock " + clock + ", " + flags + " flags " + "}";
        }
    }


    public static final class RkColorCapacityInfo {
        public int color_capa;
        public int depth_capa;

        public RkColorCapacityInfo() {
        }

        public RkColorCapacityInfo(RkColorCapacityInfo other) {
            copyFrom(other);
        }

        public List<String> getCorlorModeList(int builtIn){
            boolean is420Support=false;
            List<String> mCorlorFormatList = new ArrayList<>();
            List<String> depthList = new ArrayList<>();
            List<String> colorList = new ArrayList<>();

            if (depth_capa != 0) {
                if ((depth_capa&DEPTH_CAPA_BIT0_8BIT) >0)
                    depthList.add(STR_DEPTH_8BIT);
                if ((depth_capa & DEPTH_CAPA_BIT1_10BIT)>0)
                    depthList.add(STR_DEPTH_10BIT);
                if ((depth_capa & DEPTH_CAPA_BIT4_420_10BIT)>0)
                    is420Support=true;
            } else {
                depthList.add(STR_DEPTH_8BIT);
            }
            if (color_capa != 0) {
                if ((color_capa & DRM_HDMI_OUTPUT_DEFAULT_RGB)>0)
                    colorList.add(STR_RGB);
                if ((color_capa & DRM_HDMI_OUTPUT_YCBCR444)>0)
                    colorList.add(STR_YCBCR444);
                if ((color_capa & DRM_HDMI_OUTPUT_YCBCR422)>0)
                    colorList.add(STR_YCBCR422);
                if ((color_capa & DRM_HDMI_OUTPUT_YCBCR420)>0)
                    colorList.add(STR_YCBCR420);
            } else {
                if (builtIn != DRM_MODE_CONNECTOR_TV)
                    colorList.add(STR_RGB);
            }

            if (builtIn != DRM_MODE_CONNECTOR_TV)
                mCorlorFormatList.add("Auto");
            else
                mCorlorFormatList.add("YUV");
            for (String color : colorList) {
                for (String depth : depthList){
                    if (color.equals(STR_YCBCR420) && depth.equals(STR_DEPTH_10BIT))
                        if (is420Support==false)
                            continue;
                    StringBuilder builder = new StringBuilder();
                    builder.append(color);
                    builder.append("-");
                    builder.append(depth);
                    mCorlorFormatList.add(builder.toString());
                    Log.e(TAG, "getCorlorModeList :  " + builder.toString());
                }
            }
            return mCorlorFormatList;
        }

        @Override
        public boolean equals(Object o) {
            return o instanceof RkColorCapacityInfo && equals((RkColorCapacityInfo)o);
        }

        public boolean equals(RkColorCapacityInfo other) {
            return other != null
                && color_capa == other.color_capa
                && depth_capa == other.depth_capa;
        }

        @Override
        public int hashCode() {
            return 0; // don't care
        }

        public void copyFrom(RkColorCapacityInfo other) {
            color_capa = other.color_capa;
            depth_capa = other.depth_capa;
        }

        // For debugging purposes
        @Override
        public String toString() {
            return "RkColorCapacityInfo{" + color_capa + " x " + depth_capa + "}";
        }
    }

    public static final class RkConnectorInfo {
        public int type;
        public int id;
        public int state;

        public RkConnectorInfo() {
        }

        public RkConnectorInfo(RkConnectorInfo other) {
            copyFrom(other);
        }

        @Override
        public boolean equals(Object o) {
            return o instanceof RkConnectorInfo && equals((RkConnectorInfo)o);
        }

        public boolean equals(RkConnectorInfo other) {
            return other != null
                && type == other.type
                && id == other.id
                && state == other.state;
        }

        @Override
        public int hashCode() {
            return 0; // don't care
        }

        public void copyFrom(RkConnectorInfo other) {
            type = other.type;
            id = other.id;
            state = other.state;
        }

        @Override
        public String toString() {
            return "RkConnectorInfo{" + "type " + type + " id " + id + " state " + state + "}";
        }
    }

    public static RkDisplayModes.RkPhysicalDisplayInfo[] getDisplayConfigs(int dpy) {
        if (dpy < 0) {
            throw new IllegalArgumentException("dpy is ilegal");
        }
            return nativeGetDisplayConfigs(dpy);
    }

    public static int getNumConnectors() {
        return nativeGetNumConnectors();
    }

    public static void init() {
        nativeInit();
    }

    public  void updateDisplayInfos() {
        nativeUpdateConnectors();
    }

    public void setMode(int display, String iface, String mode){
        int ifaceType = ifacetotype(iface);
        String[] mode_str;
        int idx=0;
        RkDisplayModes.RkPhysicalDisplayInfo info;
        Log.w(TAG, "setMode " + mode + " display " + display);
        if (mode.contains("Auto")) {
            nativeSetMode(display, ifaceType, mode);
        } else {
            mode_str = mode.split("-");
            for (String mval: mode_str){
            Log.e(TAG, "setMode split:  " + mval);
        }

        if (mode_str.length != 2){
            return;
        }

        idx = Integer.parseInt(mode_str[1]);
        if (mMainDisplayInfos!=null && idx >= mMainDisplayInfos.length && display==0)
            idx=0;
        else if (mAuxDisplayInfos!=null && idx >= mAuxDisplayInfos.length && display==1)
            idx=0;

        if (display == 0)
            info = mMainDisplayInfos[idx];
        else
            info = mAuxDisplayInfos[idx];

        StringBuilder builder = new StringBuilder();
        builder.append(info.width).append("x").append(info.height);
/*
        if (info.interlaceFlag == true)
            builder.append("i");
        else
            builder.append("p");
*/
        builder.append("@");
        builder.append(String.format("%.2f", info.refreshRate));
        builder.append("-");
        builder.append(info.hsync_start);
        builder.append("-");
        builder.append(info.hsync_end);
        builder.append("-");
        builder.append(info.htotal);
        builder.append("-");
        builder.append(info.vsync_start)
        .append("-").append(info.vsync_end).append("-").append(info.vtotal).append("-").append(String.format("%x", info.flags)).append("-").append(info.clock);

        nativeSetMode(display, ifaceType, builder.toString());
    }
    }

    public String getCurMode(int display){
        String mCurMode = null;
        String mAutoMode = null;
        boolean isAutoMode=false;
        int foundIdx=-1;
        StringBuilder builder = new StringBuilder();

        mCurMode = nativeGetCurMode(display);
        Log.e(TAG, "nativeGetCurMode:  " + mCurMode);
        RkDisplayModes.RkPhysicalDisplayInfo mCurDisplayInfos[];
        if (display == MAIN_DISPLAY) {
            mCurDisplayInfos = getDisplayConfigs(display);
            mAutoMode = SystemProperties.get("persist.vendor.resolution.main", "NULL");
        } else {
            mCurDisplayInfos = getDisplayConfigs(display);
            mAutoMode = SystemProperties.get("persist.vendor.resolution.aux", "NULL");
        }

        if (mAutoMode.equals("Auto"))
            isAutoMode = true;
        if (mCurDisplayInfos != null && mCurMode != null
            && (!isAutoMode && !mCurMode.contains("Auto"))) {
            String[] mode_str = mCurMode.split("-");
            String[] resos = mode_str[0].split("x");
            String[] h_vfresh = resos[1].split("@");

            if (mode_str.length != 9 || resos.length != 2 || h_vfresh.length != 2) {
                for (String mval: mode_str) {
                    Log.e(TAG, "getCurMode split -:  " + mval);
                }
                builder.append("Auto");
                return builder.toString();
            }

            for (int i = 0; i < mCurDisplayInfos.length; i++) {
                RkDisplayModes.RkPhysicalDisplayInfo info = mCurDisplayInfos[i];
                String vfresh;
                boolean isSameVfresh = false;

                vfresh = String.format("%.2f", info.refreshRate);
                if (h_vfresh.length == 2)
                    isSameVfresh = vfresh.equals(h_vfresh[1]);

                if (info.width == Integer.parseInt(resos[0]) &&
                    info.height == Integer.parseInt(h_vfresh[0]) &&
                    isSameVfresh &&
                    info.hsync_start == Integer.parseInt(mode_str[1]) &&
                    info.hsync_end == Integer.parseInt(mode_str[2]) &&
                    info.htotal == Integer.parseInt(mode_str[3]) &&
                    info.vsync_start == Integer.parseInt(mode_str[4]) &&
                    info.vsync_end == Integer.parseInt(mode_str[5]) &&
                    info.vtotal == Integer.parseInt(mode_str[6]) &&
                    info.flags == Integer.parseInt(mode_str[7],16)) {
                    foundIdx = i;
                    builder.append(info.width).append("x").append(info.height);
                    if (info.interlaceFlag == true) {
                        builder.append("i");
                        builder.append(String.format("%.2f", info.refreshRate));
                    } else {
                        builder.append("p");
                        builder.append(String.format("%.2f", info.refreshRate));
                    }
                    builder.append("-").append(info.idx);
                    break;
                }
            }
        }

        if (mCurMode.contains("Auto") || foundIdx == -1) {
            builder.append("Auto");
            Log.e(TAG, "getCurMode idx: "+ foundIdx + "   " + builder.toString());
        }
        return builder.toString();
    }

    public int getConnectionState(int display){
        return nativeGetConnectionState(display);
    }

    public void saveConfig(){
        nativeSaveConfig();
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

    public String getbuild_in(int display){
        String iface;
        int type = nativeGetBuiltIn(display);

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
            iface = DISPLAY_TYPE_HDMIA;
        else if(type == DRM_MODE_CONNECTOR_HDMIB)
            iface = DISPLAY_TYPE_HDMIB;
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

    public  List<String> getSupportCorlorList(int dpy){
        List<String> colorList = new ArrayList<>();
        int builtIn = nativeGetBuiltIn(dpy);

        Log.e(TAG, "getSupportCorlorList =========== dpy " + dpy);
        if (dpy == 0) {
            mMainColorInfos = nativeGetCorlorModeConfigs(dpy);
            if (mMainColorInfos != null)
                return mMainColorInfos.getCorlorModeList(builtIn);
        } else {
            mAuxColorInfos = nativeGetCorlorModeConfigs(dpy);
            if (mAuxColorInfos != null)
                return mAuxColorInfos.getCorlorModeList(builtIn);
        }

        return null;
    }

    public String getCurColorMode(int dpy) {
        String mCurColorMode = null;
        int foundIdx=-1;
        RkDisplayModes.RkColorCapacityInfo mCurColorInfos;

        mCurColorMode = nativeGetCurCorlorMode(dpy);
        if (dpy == MAIN_DISPLAY) {
            mCurColorInfos = mMainColorInfos;
        } else {
            mCurColorInfos = mAuxColorInfos;
        }
        if (mCurColorInfos != null && mCurColorMode != null && !mCurColorMode.contains("Auto")) {
            List<String> corlorList = getSupportCorlorList(dpy);
            for (int i = 0; i < corlorList.size(); i++) {
                if (corlorList.get(i).equals(mCurColorMode)) {
                    return mCurColorMode;
                }
            }
        } else if (mCurColorMode != null && mCurColorMode.contains("Auto")){
            return mCurColorMode;
        }

        String mColorMode = readColorFormatFromNode();
        if (mColorMode != null)
            mCurColorMode = mColorMode;
        if (mCurColorMode == null)
           mCurColorMode = "RGB-8bit";
        Log.d(TAG, "getCurColorMode ===========  " + mCurColorMode);
        return mCurColorMode;
    }

    public int[] getBcsh(int dpy)
    {
        return nativeGetBcsh(dpy);
    }

    public int[] getOverscan(int dpy)
    {
        return nativeGetOverscan(dpy);
    }

    public int setGamma(int dpy, int size, int[] red, int[] green, int[] blue) {
        return nativeSetGamma(dpy, size, red, green, blue);
    }

    public int set3DLut(int dpy, int size, int[] red, int[] green, int[] blue) {
        return nativeSet3DLut(dpy, size, red, green, blue);
    }

    public int setHue(int display, int degree)
    {
        return nativeSetHue(display, degree);
    }

    public int setSaturation(int display, int saturation)
    {
        return nativeSetSaturation(display, saturation);
    }

    public int setContrast(int display, int contrast)
    {
        return nativeSetContrast(display, contrast);
    }

    public int setBrightness(int display, int brightness)
    {
        return nativeSetBrightness(display, brightness);
    }

    public int setScreenScale(int display, int direction, int value)
    {
        return nativeSetScreenScale(display, direction, value);
    }

    public int setHdrMode(int display, int hdrMode)
    {
        return nativeSetHdrMode(display, hdrMode);
    }

    public int setColorMode(int display, String format)
    {
        return nativeSetColorMode(display, format);
    }

    public RkConnectorInfo[] getConnectorInfo(){
        return nativeGetConnectorInfo();
    }

    public int updateDispHeader(){
        return nativeUpdateDispHeader();
    }

}
