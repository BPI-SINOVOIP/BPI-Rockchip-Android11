package com.android.tv.settings.display;

import android.R.integer;
import android.graphics.drawable.GradientDrawable.Orientation;
import android.os.SystemProperties;
import android.util.Log;
import android.view.Display;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.android.tv.settings.util.ReflectUtils;

/**
 * Drm Display Setting.
 */

public class DrmDisplaySetting {

    private final static boolean DEBUG = true;

    private final static String TAG = "DrmDisplaySetting";

    private final static String SUB_TAG = "DrmDisplaySetting";


    private final static String SYS_NODE_PARAM_STATUS_OFF = "off";

    private final static String SYS_NODE_PARAM_STATUS_ON = "detect";

    private final static String SYS_NODE_STATUS_CONNECTED = "connected";

    private final static String SYS_NODE_STATUS_DISCONNECTED = "disconnected";

    public final static int DISPLAY_TYPE_HDMI = 0;
    public final static int DISPLAY_TYPE_DP = 1;


    private static void logd(String text) {
        Log.d(TAG, SUB_TAG + " - " + text);
    }

    public static List<DisplayInfo> getDisplayInfoList() {
        List<DisplayInfo> displayInfos = new ArrayList<DisplayInfo>();
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            logd("getDisplayInfoList->rkDisplayOutputManager->name:" + rkDisplayOutputManager.getClass().getName());
        } catch (Exception e) {
            // no handle
        }

        //使用drm方式获取显示列表
        logd(" getDisplayInfoList 1");
        int[] mainTypes = (int[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getIfaceList", new Class[]{int.class}, new Object[]{0});
        logd(" getDisplayInfoList 2");
        int[] externalTypes = (int[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getIfaceList", new Class[]{int.class}, new Object[]{1});
        logd(" getDisplayInfoList 3");
        //RK系列芯片，目前最多只能支持2个屏幕
        if (mainTypes != null && mainTypes.length > 0) {
            int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{int.class}, new Object[]{0});
            //主屏只能有一个
            DisplayInfo displayInfo = new DisplayInfo();
            displayInfo.setDisplayId(0);
            logd(" getDisplayInfoList 4");
            displayInfo.setDescription((String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "typetoface", new Class[]{int.class}, new Object[]{currMainType}));
            logd(" getDisplayInfoList 5");

            displayInfo.setType(currMainType);
            String[] orginModes = (String[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getModeList", new Class[]{int.class, int.class}, new Object[]{0, currMainType});
            orginModes = filterOrginModes(orginModes);
            displayInfo.setOrginModes(orginModes);
            displayInfo.setModes(getFilterModeList(orginModes));
            logd(" getDisplayInfoList 6");
            displayInfos.add(displayInfo);
        }
        if (externalTypes != null && externalTypes.length > 0) {
	   logd(" getDisplayInfoList 7");
            int currExternalType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{int.class}, new Object[]{1});
            //副屏只能有一个
            DisplayInfo displayInfo = new DisplayInfo();
            displayInfo.setType(currExternalType);
	   logd(" getDisplayInfoList 8");
            String[] orginModes = (String[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getModeList", new Class[]{int.class, int.class}, new Object[]{1, currExternalType});
            orginModes = filterOrginModes(orginModes);
            displayInfo.setOrginModes(orginModes);
            displayInfo.setModes(getFilterModeList(orginModes));
            logd(" getDisplayInfoList 9");
            //displayInfo.setModes(getFilterModeList((String[])ReflectUtils.invokeMethod(rkDisplayOutputManager, "getModeList", new Class[]{int.class, int.class}, new Object[]{1, externalTypes})));
            displayInfo.setDescription((String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "typetoface", new Class[]{int.class}, new Integer[]{currExternalType}));
            logd(" getDisplayInfoList 10");
            displayInfo.setDisplayId(1);
            displayInfos.add(displayInfo);
        }
        return displayInfos;
    }

    public static List<String> getDisplayModes(DisplayInfo di) {
        List<String> res = new ArrayList<String>();
        if (di.getDisplayId() == DISPLAY_TYPE_HDMI) {
            di = getHdmiDisplayInfo();
        } else if (di.getDisplayId() == DISPLAY_TYPE_DP) {
            di = getDpDisplayInfo();
        }
        if (di != null) {
            String[] modes = di.getOrginModes();
            if (modes != null && modes.length != 0) {
                res = Arrays.asList(modes);
            }
        }
        return res;
    }

    public static String getCurDisplayMode(DisplayInfo di) {
        String curMode = null;
        if (di.getDisplayId() == DISPLAY_TYPE_HDMI) {
            logd("DrmDisplaySetting getCurDisplayMode DISPLAY_TYPE_HDMI" + System.currentTimeMillis());
            curMode = getCurHdmiMode();
        } else if (di.getDisplayId() == DISPLAY_TYPE_DP) {
            logd("DrmDisplaySetting getCurDisplayMode DISPLAY_TYPE_DP " + System.currentTimeMillis());
            curMode = getCurDpMode();
        }
        logd("getCurDisplayMode - curMode - " + curMode);
        return curMode;
    }

    public static String getCurHdmiMode() {
        return getHdmiMode();
    }

    public static String getCurDpMode() {
        return getDpMode();
    }

    public static List<String> getColorModeList() {
        List<String> res = new ArrayList<String>();
                Object rkDisplayOutputManager = null;
        logd(" DrmDisplaySetting getColorModes");
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            logd("getDisplayInfoList->rkDisplayOutputManager->name:" + rkDisplayOutputManager.getClass().getName());
        } catch (Exception e) {
            // no handle
        }
            int[] mainTypes = (int[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getIfaceList", new Class[]{int.class}, new Object[]{0});
            if (mainTypes != null && mainTypes.length > 0) {
                int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{int.class}, new Object[]{0});
                    String[] modes = (String[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getSupportCorlorList", new Class[]{int.class, int.class}, new Object[]{0, currMainType});
                if (modes != null && modes.length != 0) {
                    res = Arrays.asList(modes);
                }
            }
        return res;
    }  
    public static void setColorMode(int displayid, int type, String format) {
        Object rkDisplayOutputManager = null;
        logd(" setColorMode 1");

        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            logd("getDisplayInfoList->rkDisplayOutputManager->name:" + rkDisplayOutputManager.getClass().getName());
        } catch (Exception e) {
            // no handle
        }
        if (rkDisplayOutputManager != null)
            ReflectUtils.invokeMethod(rkDisplayOutputManager, "setColorMode", new Class[]{int.class, int.class, String.class}, new Object[]{displayid, type, format});
        saveConfig();
    }

    public static String getColorMode() {
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            logd("getDisplayInfoList->rkDisplayOutputManager->name:" + rkDisplayOutputManager.getClass().getName());
        } catch (Exception e) {
            // no handle
        }
        if (rkDisplayOutputManager == null)
            return null;
        logd(" getColorMode 1");
        int[] mainTypes = (int[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getIfaceList", new Class[]{int.class}, new Object[]{0});
        logd(" getColorMode 2");
        //RK系列芯片，目前最多只能支持2个屏幕
        if (mainTypes != null && mainTypes.length > 0) {
            int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{int.class}, new Object[]{0});
            return (String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentColorMode", new Class[]{int.class, int.class}, new Object[]{0, currMainType});
        }
        return null;
    }

    public static void setDisplayModeTemp(DisplayInfo di, int index) {
        String mode = getDisplayModes(di).get(index);
        setDisplayModeTemp(di, mode);
    }

    public static void setDisplayModeTemp(DisplayInfo di, String mode) {
        curSetHdmiMode = getCurDisplayMode(di);
        logd("setDisplayModeTemp curSetHdmiMode = "+curSetHdmiMode);
        if (di.getDisplayId() == DISPLAY_TYPE_HDMI) {
            setHdmiModeTemp(mode);
        } else if (di.getDisplayId() == DISPLAY_TYPE_DP) {
            setDpModeTemp(mode);
        }
    }

    public static void confirmSaveDisplayMode(DisplayInfo di, boolean isSave) {
        if (di == null) {
            return;
        }
        if (di.getDisplayId() == DISPLAY_TYPE_HDMI) {
            confirmSaveHdmiMode(isSave);
        } else if (di.getDisplayId() == DISPLAY_TYPE_DP) {
            confirmSaveDpMode(isSave);
        }
    }

    /**
     * ==================================================================================
     * HDMI Setting
     * ==================================================================================
     */

    private final static String SYS_NODE_HDMI_MODES =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/modes";

    private final static String SYS_NODE_HDMI_MODE =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/mode";

    private final static String SYS_NODE_HDMI_STATUS =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/status";

    private final static String PROP_RESOLUTION_HDMI = "persist.sys.resolution.main";

    public static String tmpSetHdmiMode = null;
    public static String curSetHdmiMode = null;

    public static DisplayInfo getHdmiDisplayInfo() {
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            logd("getDisplayInfoList->rkDisplayOutputManager->name:" + rkDisplayOutputManager.getClass().getName());
        } catch (Exception e) {
            // no handle
        }
        if (rkDisplayOutputManager == null)
            return null;
        logd(" getHdmiDisplayInfo 1");
        int[] mainTypes = (int[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getIfaceList", new Class[]{int.class}, new Object[]{0});
        logd(" getHdmiDisplayInfo 2");
        //RK系列芯片，目前最多只能支持2个屏幕
        if (mainTypes != null && mainTypes.length > 0) {
            int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{int.class}, new Object[]{0});
            //主屏只能有一个
            DisplayInfo displayInfo = new DisplayInfo();
            displayInfo.setDisplayId(0);
            logd(" getHdmiDisplayInfo 3");
            displayInfo.setDescription((String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "typetoface", new Class[]{int.class}, new Object[]{currMainType}));
            logd(" getHdmiDisplayInfo 4");
            displayInfo.setType(currMainType);
            String[] orginModes = (String[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getModeList", new Class[]{int.class, int.class}, new Object[]{0, currMainType});
            orginModes = filterOrginModes(orginModes);
            displayInfo.setOrginModes(orginModes);
            displayInfo.setModes(getFilterModeList(orginModes));
            String[] colors = (String[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getSupportCorlorList", new Class[]{int.class, int.class}, new Object[]{0, currMainType});
            displayInfo.setColors(colors);
            logd(" getHdmiDisplayInfo 5");
            return displayInfo;
        }
        return null;
       /* if (SYS_NODE_STATUS_CONNECTED.equals(getHdmiStatus())) {
            DisplayInfo di = new DisplayInfo();
            List<String> hdmiResoList = getHdmiModes();
            String[] hdmiResoStrs = hdmiResoList.toArray(new String[hdmiResoList.size()]);
            di.setModes(hdmiResoStrs);
            di.setDescription("HDMI");
            di.setDisplayId(DISPLAY_TYPE_HDMI);
            return di;
        }
        return null;*/
    }

    private static List<String> getHdmiModes() {
        List<String> res = null;
        try {
            res = readStrListFromFile(SYS_NODE_HDMI_MODES);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return processModeStr(res);
    }

    private static String getHdmiStatus() {
        String status = null;
        try {
            status = readStrFromFile(SYS_NODE_HDMI_STATUS);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return status;
    }

    private static String getHdmiMode() {
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            logd("getDisplayInfoList->rkDisplayOutputManager->name:" + rkDisplayOutputManager.getClass().getName());
        } catch (Exception e) {
            // no handle
        }
        if (rkDisplayOutputManager == null)
            return null;
        logd(" getHdmiMode 1");
        int[] mainTypes = (int[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getIfaceList", new Class[]{int.class}, new Object[]{0});
        logd(" getHdmiMode 2");
        //RK系列芯片，目前最多只能支持2个屏幕
        if (mainTypes != null && mainTypes.length > 0) {
            int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{int.class}, new Object[]{0});
            return (String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentMode", new Class[]{int.class, int.class}, new Object[]{0, currMainType});
        }
        return null;
    }

    private static void setHdmiModeTemp(String mode) {
        setHdmiMode(mode);
        tmpSetHdmiMode = mode;
    }

    private static void confirmSaveHdmiMode(boolean isSave) {
        logd(" confirmSaveHdmiMode save = "+isSave);
        if (tmpSetHdmiMode == null) {
            return;
        }
        if (isSave) {
            curSetHdmiMode = tmpSetHdmiMode;
            saveConfig();
        } else {
            setHdmiMode(curSetHdmiMode);
            tmpSetHdmiMode = null;
        }
    }

    private static void setHdmiMode(String mode) {
        //SystemProperties.set(PROP_RESOLUTION_HDMI, mode);
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            logd("getDisplayInfoList->rkDisplayOutputManager->name:" + rkDisplayOutputManager.getClass().getName());
        } catch (Exception e) {
            // no handle
        }
        if (rkDisplayOutputManager == null)
            return;
        logd(" setHdmiMode 1");
        int[] mainTypes = (int[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getIfaceList", new Class[]{int.class}, new Object[]{0});
        logd(" setHdmiMode 2");
        if (mainTypes != null && mainTypes.length > 0) {
            logd(" setHdmiMode mode = " + mode);
            int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{int.class}, new Object[]{0});
            ReflectUtils.invokeMethod(rkDisplayOutputManager, "setMode", new Class[]{int.class, int.class, String.class}, new Object[]{0, currMainType, mode});
        }
        logd(" setHdmiMode 3");
    }

    /**
     * ==================================================================================
     * DP Setting
     * ==================================================================================
     */

    private final static String SYS_NODE_DP_MODES =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-DP-1/modes";

    private final static String SYS_NODE_DP_MODE =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-DP-1/mode";

    private final static String SYS_NODE_DP_STATUS =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-DP-1/status";

    private final static String PROP_RESOLUTION_DP = "persist.sys.resolution.aux";

    private static String tmpSetDpMode = null;
    private static String curSetDpMode = "1920x1080p60";

    public static DisplayInfo getDpDisplayInfo() {
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            logd("getDisplayInfoList->rkDisplayOutputManager->name:" + rkDisplayOutputManager.getClass().getName());
        } catch (Exception e) {
            // no handle
        }
        logd(" getDpDisplayInfo 1");
        int[] externalTypes = (int[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getIfaceList", new Class[]{int.class}, new Object[]{1});
        logd(" getDpDisplayInfo 2");
        if (externalTypes != null && externalTypes.length > 0) {
            int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{int.class}, new Object[]{1});
            DisplayInfo displayInfo = new DisplayInfo();
            displayInfo.setDisplayId(1);
            logd(" getDpDisplayInfo 3");
            displayInfo.setDescription((String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "typetoface", new Class[]{int.class}, new Object[]{currMainType}));
            logd(" getDpDisplayInfo 4");
            displayInfo.setType(currMainType);
            String[] orginModes = (String[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getModeList", new Class[]{int.class, int.class}, new Object[]{1, currMainType});
            orginModes = filterOrginModes(orginModes);
            displayInfo.setOrginModes(orginModes);
            displayInfo.setModes(getFilterModeList(orginModes));
            String[] colors = (String[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getSupportCorlorList", new Class[]{int.class, int.class}, new Object[]{1, currMainType});
            displayInfo.setColors(colors);

            logd(" getDpDisplayInfo 5");
            return displayInfo;
        }
        return null;
    }

    private static List<String> getDpModes() {
        List<String> res = null;
        try {
            res = readStrListFromFile(SYS_NODE_DP_MODES);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return processModeStr(res);
    }

    private static String getDpStatus() {
        String status = null;
        try {
            status = readStrFromFile(SYS_NODE_DP_STATUS);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return status;
    }

    private static String getDpMode() {
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            logd("getDisplayInfoList->rkDisplayOutputManager->name:" + rkDisplayOutputManager.getClass().getName());
        } catch (Exception e) {
            // no handle
        }
        if (rkDisplayOutputManager == null)
            return null;
        logd(" getDpMode 1");
        int[] mainTypes = (int[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getIfaceList", new Class[]{int.class}, new Object[]{1});
        logd(" getDpMode 2");
        if (mainTypes != null && mainTypes.length > 0) {
            int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{int.class}, new Object[]{1});
            return (String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentMode", new Class[]{int.class, int.class}, new Object[]{1, currMainType});
        }
        return null;
    }

    private static void setDpModeTemp(String reso) {
        setDpMode(reso);
        tmpSetDpMode = reso;
    }

    private static void confirmSaveDpMode(boolean isSave) {
        if (tmpSetDpMode == null) {
            return;
        }
        if (isSave) {
            curSetDpMode = tmpSetDpMode;
        } else {
            setDpMode(curSetDpMode);
            tmpSetDpMode = null;
        }
    }

    private static void setDpMode(String reso) {
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            logd("getDisplayInfoList->rkDisplayOutputManager->name:" + rkDisplayOutputManager.getClass().getName());
        } catch (Exception e) {
            // no handle
        }
        if (rkDisplayOutputManager == null)
            return;
        logd(" setDpMode 1");
        int[] mainTypes = (int[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getIfaceList", new Class[]{int.class}, new Object[]{1});
        logd(" setDpMode 2");
        //RK系列芯片，目前最多只能支持2个屏幕
        if (mainTypes != null && mainTypes.length > 0) {
            int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{int.class}, new Object[]{1});
            ReflectUtils.invokeMethod(rkDisplayOutputManager, "setMode", new Class[]{int.class, int.class, String.class}, new Object[]{1, currMainType, reso});
        }
    }

    /**
     * ==================================================================================
     * Common
     * ==================================================================================
     */
    private static final String[] COMMON_RESOLUTION = {
            "3840x2160",
            "1920x1080",
            "1280x720",
            "800x600",
            "640x480"
    };

    private static List<String> processModeStr(List<String> resoStrList) {
        if (resoStrList == null) {
            return null;
        }
        List<String> processedResoStrList = new ArrayList<>();
        List<String> tmpResoStrList = new ArrayList<>();
        for (String reso : resoStrList) {
            if (reso.contains("p") || reso.contains("i")) {
                boolean hasRepeat = false;
                for (String s : tmpResoStrList) {
                    if (s.equals(reso)) {
                        hasRepeat = true;
                        break;
                    }
                }
                if (!hasRepeat) {
                    tmpResoStrList.add(reso);
                }
            }
        }
        return tmpResoStrList;
    }

    private static List<String> readStrListFromFile(String pathname) throws IOException {
        List<String> fileStrings = new ArrayList<>();
        File filename = new File(pathname);
        InputStreamReader reader = new InputStreamReader(new FileInputStream(filename));
        BufferedReader br = new BufferedReader(reader);
        String line;
        while ((line = br.readLine()) != null) {
            fileStrings.add(line);
        }
        logd("readStrListFromFile - " + fileStrings.toString());
        return fileStrings;
    }

    private static String readStrFromFile(String filename) throws IOException {
        logd("readStrFromFile - " + filename);
        File f = new File(filename);
        InputStreamReader reader = new InputStreamReader(new FileInputStream(f));
        BufferedReader br = new BufferedReader(reader);
        String line = br.readLine();
        logd("readStrFromFile - " + line);
        return line;
    }

    private static String[] filterOrginModes(String[] modes) {
        if (modes == null)
            return null;
        List<String> filterModeList = new ArrayList<String>();
        List<String> resModeList = new ArrayList<String>();
        for (int i = 0; i < modes.length; ++i) {
            logd("filterOrginModes->mode:" + modes[i]);
            String itemMode = modes[i];
            int endIndex = itemMode.indexOf("-");
            if (endIndex > 0)
                itemMode = itemMode.substring(0, endIndex);
            if (!resModeList.contains(itemMode)) {
                resModeList.add(itemMode);
                if (!filterModeList.contains(modes[i]))
                    filterModeList.add(modes[i]);
            }
        }
        return filterModeList.toArray(new String[0]);
    }

    private static String[] getFilterModeList(String[] modes) {
        if (modes == null)
            return null;
        String[] filterModes = new String[modes.length];
        for (int i = 0; i < modes.length; ++i) {
            String itemMode = modes[i];
            int endIndex = itemMode.indexOf("-");
            if (endIndex > 0)
                itemMode = itemMode.substring(0, endIndex);
            filterModes[i] = itemMode;
        }
        return filterModes;
    }

    public static int saveConfig() {
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            logd("getDisplayInfoList->rkDisplayOutputManager->name:" + rkDisplayOutputManager.getClass().getName());
        } catch (Exception e) {
            e.printStackTrace();
        }
        if (rkDisplayOutputManager == null)
            return -1;

        int res = (int) ReflectUtils.invokeMethod(rkDisplayOutputManager, "saveConfig", new Class[]{}, new Object[]{});
        return res;
    }
}
