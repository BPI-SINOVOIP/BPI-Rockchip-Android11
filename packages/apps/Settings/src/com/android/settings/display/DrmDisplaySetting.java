package com.android.settings.display;

import android.graphics.Rect;
import android.os.RkDisplayOutputManager;
import android.util.Log;

import com.android.settings.utils.ReflectUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Drm Display Setting.
 */

public class DrmDisplaySetting {
    private final static String TAG = "DrmDisplaySetting";

    public static final int DPY_STATUS_CONNECTED = 1;

    public static final String DRM_MODE_CONNECTOR_UNKNOWN = "Display";//Unknown
    public static final String DRM_MODE_CONNECTOR_eDP = "eDP";
    public static final String DRM_MODE_CONNECTOR_VIRTUAL = "VIRTUAL";
    public static final String DRM_MODE_CONNECTOR_DSI = "DSI";
    public static Map<String, String> CONNECTOR_DISPLAY_NAME = new HashMap<String, String>() {
        {
            put("0", DRM_MODE_CONNECTOR_UNKNOWN);
            put("1", "VGA");
            put("2", "DVII");
            put("3", "DVID");
            put("4", "DVIA");
            put("5", "Composite");
            put("6", "SVIDEO");
            put("7", "LVDS");
            put("8", "Component");
            put("9", "9PinDIN");
            put("10", "DisplayPort");
            put("11", "HDMIA");
            put("12", "HDMIB");
            put("13", "TV");
            put("14", DRM_MODE_CONNECTOR_eDP);
            put("15", DRM_MODE_CONNECTOR_VIRTUAL);
            put("16", DRM_MODE_CONNECTOR_DSI);
        }
    };

    private static void logd(String text) {
        Log.d(TAG, TAG + " - " + text);
    }

    public static String[] getConnectorInfo() {
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
        } catch (Exception e) {
            // no handle
        }
        if (rkDisplayOutputManager != null) {
            return (String[]) ReflectUtils.invokeMethodNoParameter(rkDisplayOutputManager, "getConnectorInfo");
        }
        return null;
    }

    public static Rect getOverScan(int display) {
        RkDisplayOutputManager manager = new RkDisplayOutputManager();
        return manager.getOverScan(display);
    }

    public static void setOverScan(int display, int direction, int value) {
        RkDisplayOutputManager manager = new RkDisplayOutputManager();
        manager.setOverScan(display, direction, value);
    }

    public static int getDisplayNumber() {
        RkDisplayOutputManager manager = new RkDisplayOutputManager();
        return manager.getDisplayNumber();
    }

    public static int getCurrentDpyConnState(int display) {
        RkDisplayOutputManager manager = new RkDisplayOutputManager();
        return manager.getCurrentDpyConnState(display);
    }

    public static void updateDisplayModesInfo(DisplayInfo displayInfo) {
        int display = displayInfo.getDisplayNo();
        RkDisplayOutputManager manager = new RkDisplayOutputManager();
//        int[] ifaces = manager.getIfaceList(display);
//        if (null == ifaces || ifaces.length < 1) {
//            Log.w(TAG, "display " + display + ", ifaces=" + ifaces);
//            displayInfo.setOrginModes(null);
//            displayInfo.setModes(null);
//            displayInfo.setCurrentResolution(null);
//        } else {
        int type = manager.getCurrentInterface(display);
        String[] orginModes = manager.getModeList(display, type);
        orginModes = filterOrginModes(orginModes);
        displayInfo.setOrginModes(orginModes);
        displayInfo.setModes(getFilterModeList(orginModes));
        String currentMode = manager.getCurrentMode(display, type);
        displayInfo.setCurrentResolution(currentMode);
//        }
    }

    public static void setMode(int display, String mode) {
        RkDisplayOutputManager manager = new RkDisplayOutputManager();
        int result = manager.updateDisplayInfos();
        int type = manager.getCurrentInterface(display);
        logd("setMode display=" + display + ", type=" + type + ", mode=" + mode);
        manager.setMode(display, type, mode);
    }

    public static void setDisplayModeTemp(DisplayInfo di, int index) {
        String mode = Arrays.asList(di.getOrginModes()).get(index);
        setMode(di.getDisplayNo(), mode);
        tmpSetMode = mode;
    }

    public static void saveConfig() {
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
        } catch (Exception e) {
            // no handle
        }
        if (rkDisplayOutputManager != null) {
            int result = (Integer) ReflectUtils.invokeMethodNoParameter(rkDisplayOutputManager, "saveConfig");
        }
    }

    public static void updateDisplayInfos() {
        Object rkDisplayOutputManager = null;
        try {
            rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
        } catch (Exception e) {
            // no handle
        }
        if (rkDisplayOutputManager != null) {
            logd("updateDisplayInfos");
            int result = (Integer) ReflectUtils.invokeMethodNoParameter(rkDisplayOutputManager, "updateDisplayInfos");
        }
    }

    public static void confirmSaveDisplayMode(DisplayInfo displayInfo, boolean isSave) {
        if (displayInfo == null || tmpSetMode == null) {
            return;
        }
        if (isSave) {
            displayInfo.setLastResolution(tmpSetMode);
        } else {
            setMode(displayInfo.getDisplayNo(), displayInfo.getLastResolution());
            tmpSetMode = null;
        }
        saveConfig();
    }

    private static String tmpSetMode = null;

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
}
