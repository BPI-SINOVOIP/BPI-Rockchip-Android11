package com.android.settings.display;

import android.os.SystemProperties;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import com.android.settings.utils.ReflectUtils;

/**
 * Display Setting with node.
 */

public class NodeDisplaySetting {
    private final static String TAG = "FbDisplaySetting";

    private final static String SYS_NODE_PARAM_STATUS_OFF = "off";

    private final static String SYS_NODE_PARAM_STATUS_ON = "detect";

    private final static String SYS_NODE_STATUS_CONNECTED = "connected";

    private final static String SYS_NODE_STATUS_DISCONNECTED = "disconnected";

    private final static String SYS_NODE_HDMI_MODES =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/modes";
    private final static String SYS_NODE_HDMI_MODE =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/mode";
    private final static String SYS_NODE_HDMI_STATUS =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/status";
    private final static String PROP_RESOLUTION_HDMI = "persist.sys.resolution.aux";

    private final static String SYS_NODE_DP_MODES =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-DP-1/modes";
    private final static String SYS_NODE_DP_MODE =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-DP-1/mode";
    private final static String SYS_NODE_DP_STATUS =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-DP-1/status";
    private final static String PROP_RESOLUTION_DP = "persist.sys.resolution.aux";

    private static void logd(String text) {
        Log.d(TAG, TAG + " - " + text);
    }

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

    private static String readStrFromFile(String filename) throws IOException {
        logd("readStrFromFile - " + filename);
        File f = new File(filename);
        InputStreamReader reader = new InputStreamReader(new FileInputStream(f));
        BufferedReader br = new BufferedReader(reader);
        String line = br.readLine();
        logd("readStrFromFile - " + line);
        return line;
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

    public static String getHdmiStatus() {
        String status = null;
        try {
            status = readStrFromFile(SYS_NODE_HDMI_STATUS);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return status;
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

    private static String getDpStatus() {
        String status = null;
        try {
            status = readStrFromFile(SYS_NODE_DP_STATUS);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return status;
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
}
