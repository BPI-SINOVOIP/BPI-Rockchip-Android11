package com.android.settings.display;

import android.app.ProgressDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.preference.Preference.OnPreferenceChangeListener;
import android.util.Log;
import android.view.IWindowManager;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.DialogFragment;
import androidx.preference.CheckBoxPreference;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.SwitchPreference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;

import com.android.internal.logging.nano.MetricsProto.MetricsEvent;
import com.android.settings.HdmiListPreference;
import com.android.settings.R;
import com.android.settings.SettingsPreferenceFragment;

import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import static com.android.settings.display.DrmDisplaySetting.DPY_STATUS_CONNECTED;

public class HdmiSettings extends SettingsPreferenceFragment
        implements Preference.OnPreferenceChangeListener,
        Preference.OnPreferenceClickListener {
    /**
     * Called when the activity is first created.
     */
    private static final String TAG = "HdmiSettings";
    private static final String KEY_SYSTEM_ROTATION = "system_rotation";
    private static final String KEY_PRE_CATE = "Display";
    private static final String KEY_PRE_RESOLUTION = "Resolution";
    private static final String KEY_PRE_SCREEN_SCALE = "ScreenScale";
    private static final String KEY_AUX_CATEGORY = "aux_category";
    private static final String KEY_AUX_SCREEN_VH = "aux_screen_vh";
    private static final String KEY_AUX_SCREEN_VH_LIST = "aux_screen_vhlist";
    private final static String SYS_NODE_HDMI_STATUS =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/status";
    private final static String SYS_NODE_DP_STATUS =
            "/sys/devices/platform/display-subsystem/drm/card0/card0-DP-1/status";

    private static final int MSG_UPDATE_STATUS = 0;
    private static final int MSG_UPDATE_STATUS_UI = 1;
    private static final int MSG_SWITCH_DEVICE_STATUS = 2;
    private static final int MSG_UPDATE_DIALOG_INFO = 3;
    private static final int MSG_SHOW_CONFIRM_DIALOG = 4;
    private static final int SWITCH_STATUS_OFF_ON = 0;
    private static final int SWITCH_STATUS_OFF = 1;
    private static final int SWITCH_STATUS_ON = 2;
    private static final long SWITCH_DEVICE_DELAY_TIME = 200;
    private static final long TIME_WAIT_DEVICE_CONNECT = 10000;
    //we found setprop not effect sometimes if control quickly
    private static final boolean USED_OFFON_RESOLUTION = false;

    /**
     * TODO
     * 目前hwc配置了prop属性开关hdmi和dp，如果是其他的设备，需要配合修改，才能进行开关。因此直接写节点进行开关
     * vendor.hdmi_status.aux：/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/status
     * vendor.dp_status.aux暂无：/sys/devices/platform/display-subsystem/drm/card0/card0-DP-1/status
     */
    private String main_switch_node = SYS_NODE_HDMI_STATUS;
    private String aux_switch_node = SYS_NODE_DP_STATUS;

    private ListPreference mSystemRotation;
    private PreferenceCategory mAuxCategory;
    private CheckBoxPreference mAuxScreenVH;
    private ListPreference mAuxScreenVHList;
    private Context mContext;
    private DisplayInfo mSelectDisplayInfo;
    private DisplayManager mDisplayManager;
    private DisplayListener mDisplayListener;
    private IWindowManager mWindowManager;
    private ProgressDialog mProgressDialog;
    private boolean mDestory;
    private boolean mEnableDisplayListener;
    private Object mLock = new Object();//maybe android reboot if not lock with new thread
    private boolean mResume;
    private long mWaitDialogCountTime;
    private int mRotation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

    private HashMap<Integer, DisplayInfo> mDisplayInfoList = new HashMap<Integer, DisplayInfo>();

    enum ITEM_CONTROL {
        SHOW_RESOLUTION_ITEM,//展示分辨率选项
        CHANGE_RESOLUTION,//切换分辨率
        REFRESH_DISPLAY_STATUS_INFO,//刷新主屏信息
    }

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(final Message msg) {
            if (mDestory && MSG_SWITCH_DEVICE_STATUS != msg.what) {
                return;
            }
            if (MSG_UPDATE_STATUS == msg.what) {
                final ITEM_CONTROL control = (ITEM_CONTROL) msg.obj;
                new Thread() {
                    @Override
                    public void run() {
                        if (ITEM_CONTROL.REFRESH_DISPLAY_STATUS_INFO == control
                                || ITEM_CONTROL.CHANGE_RESOLUTION == control) {
                            getDisplayStatusInfo();
                        } else if (ITEM_CONTROL.SHOW_RESOLUTION_ITEM == control) {
                            getDisplayStatusInfo();
                            getDisplayResolutionInfo(mSelectDisplayInfo);
                        }
                        Message message = new Message();
                        message.what = MSG_UPDATE_STATUS_UI;
                        message.obj = control;
                        mHandler.sendMessage(message);
                    }
                }.start();
            } else if (MSG_UPDATE_STATUS_UI == msg.what) {
                ITEM_CONTROL control = (ITEM_CONTROL) msg.obj;
                if (ITEM_CONTROL.SHOW_RESOLUTION_ITEM == control) {
                    updateStateUI();
                    if (null != mSelectDisplayInfo
                            && DPY_STATUS_CONNECTED == mSelectDisplayInfo.getStatus()) {
                        showResolutionItemUI(mSelectDisplayInfo);
                    }
                    hideWaitingDialog();
                } else if (ITEM_CONTROL.CHANGE_RESOLUTION == control) {
                    updateStateUI();
                    showConfirmSetModeDialog();
                    hideWaitingDialog();
                } else if (ITEM_CONTROL.REFRESH_DISPLAY_STATUS_INFO == control) {
                    updateStateUI();
                    hideWaitingDialog();
                }
                mEnableDisplayListener = true;
            } else if (MSG_SWITCH_DEVICE_STATUS == msg.what) {
                final ITEM_CONTROL control = (ITEM_CONTROL) msg.obj;
                if (SWITCH_STATUS_ON == msg.arg1) {
                    if (ITEM_CONTROL.CHANGE_RESOLUTION == control
                            || ITEM_CONTROL.REFRESH_DISPLAY_STATUS_INFO == control) {
                        showWaitingDialog(R.string.dialog_wait_screen_connect);
                        new Thread() {
                            @Override
                            public void run() {
                                write2Node(main_switch_node, "detect");
                                mWaitDialogCountTime = TIME_WAIT_DEVICE_CONNECT / 1000;
                                mHandler.removeMessages(MSG_UPDATE_DIALOG_INFO);
                                mHandler.sendEmptyMessage(MSG_UPDATE_DIALOG_INFO);
                                sendUpdateStateMsg(control, TIME_WAIT_DEVICE_CONNECT);
                            }
                        }.start();
                    }
                } else {
                    if (ITEM_CONTROL.CHANGE_RESOLUTION == control
                            || ITEM_CONTROL.REFRESH_DISPLAY_STATUS_INFO == control) {
                        new Thread() {
                            @Override
                            public void run() {
                                write2Node(main_switch_node, "off");
                                if (SWITCH_STATUS_OFF_ON == msg.arg1) {
                                    sendSwitchDeviceOffOnMsg(control, SWITCH_STATUS_ON);
                                } else {
                                    sendUpdateStateMsg(control, 2000);
                                }
                            }
                        }.start();
                    }
                }
            } else if (MSG_UPDATE_DIALOG_INFO == msg.what) {
                if (mWaitDialogCountTime > 0) {
                    if (null != mProgressDialog && mProgressDialog.isShowing()) {
                        mProgressDialog.setMessage(getContext().getString(
                                R.string.dialog_wait_screen_connect) + " " + mWaitDialogCountTime);
                        mWaitDialogCountTime--;
                        mHandler.removeMessages(MSG_UPDATE_DIALOG_INFO);
                        mHandler.sendEmptyMessageDelayed(MSG_UPDATE_DIALOG_INFO, 1000);
                    }
                }
            } else if (MSG_SHOW_CONFIRM_DIALOG == msg.what) {
                mHandler.removeMessages(MSG_SHOW_CONFIRM_DIALOG);
                hideWaitingDialog();
                showConfirmSetModeDialog();
            }
        }
    };

    private final BroadcastReceiver HdmiListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context ctxt, Intent receivedIt) {
            String action = receivedIt.getAction();
            String HDMIINTENT = "android.intent.action.HDMI_PLUGGED";
            if (action.equals(HDMIINTENT)) {
                boolean state = receivedIt.getBooleanExtra("state", false);
                if (state) {
                    Log.d(TAG, "BroadcastReceiver.onReceive() : Connected HDMI-TV");
                } else {
                    Log.d(TAG, "BroadcastReceiver.onReceive() : Disconnected HDMI-TV");
                }
            }
        }
    };

    @Override
    public int getMetricsCategory() {
        return MetricsEvent.DISPLAY;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mContext = getActivity();
        mRotation = getActivity().getRequestedOrientation();
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LOCKED);
        mDisplayManager = (DisplayManager) mContext.getSystemService(Context.DISPLAY_SERVICE);
        mWindowManager = IWindowManager.Stub.asInterface(
                ServiceManager.getService(Context.WINDOW_SERVICE));
        mDisplayListener = new DisplayListener();
        addPreferencesFromResource(R.xml.hdmi_settings);

        init();
        mEnableDisplayListener = true;
        Log.d(TAG, "---------onCreate---------------------");
    }


    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        Log.d(TAG, "onCreateView----------------------------------------");
        return super.onCreateView(inflater, container, savedInstanceState);
    }

    @Override
    public void onResume() {
        super.onResume();
        //showWaitingDialog(0, "");
        IntentFilter filter = new IntentFilter("android.intent.action.HDMI_PLUGGED");
        getContext().registerReceiver(HdmiListener, filter);
        //refreshState();
        mDisplayManager.registerDisplayListener(mDisplayListener, null);
        mResume = true;
    }

    private void showWaitingDialog(int msgResId) {
        if (mDestory) {
            return;
        }
        if (null == mProgressDialog) {
            mProgressDialog = new ProgressDialog(getActivity());
            mProgressDialog.setCanceledOnTouchOutside(false);
            mProgressDialog.setCancelable(false);
        }
        mProgressDialog.setMessage(getContext().getString(msgResId));
        if (!mProgressDialog.isShowing()) {
            mProgressDialog.show();
        }
    }

    private void hideWaitingDialog() {
        if (null != mProgressDialog && mProgressDialog.isShowing()) {
            mProgressDialog.cancel();
            mProgressDialog = null;
        }
    }

    public void onPause() {
        super.onPause();
        mResume = false;
        Log.d(TAG, "onPause----------------");
        mDisplayManager.unregisterDisplayListener(mDisplayListener);
        getContext().unregisterReceiver(HdmiListener);
    }

    public void onDestroy() {
        mDestory = true;
        getActivity().setRequestedOrientation(mRotation);
        super.onDestroy();
        mHandler.removeMessages(MSG_UPDATE_STATUS);
        mHandler.removeMessages(MSG_SWITCH_DEVICE_STATUS);
        mHandler.removeMessages(MSG_UPDATE_DIALOG_INFO);
        mHandler.removeMessages(MSG_SHOW_CONFIRM_DIALOG);
        hideWaitingDialog();
        Log.d(TAG, "onDestroy----------------");
    }

    private void init() {
        //boolean showSystemRotation = mShowSettings != DISPLAY_SHOW_SETTINGS.ONLY_SHOW_AUX;
        boolean showSystemRotation = false;
        if (showSystemRotation) {
            mSystemRotation = (ListPreference) findPreference(KEY_SYSTEM_ROTATION);
            mSystemRotation.setOnPreferenceChangeListener(this);
            try {
                int rotation = mWindowManager.getDefaultDisplayRotation();
                switch (rotation) {
                    case Surface.ROTATION_0:
                        mSystemRotation.setValue("0");
                        break;
                    case Surface.ROTATION_90:
                        mSystemRotation.setValue("90");
                        break;
                    case Surface.ROTATION_180:
                        mSystemRotation.setValue("180");
                        break;
                    case Surface.ROTATION_270:
                        mSystemRotation.setValue("270");
                        break;
                    default:
                        mSystemRotation.setValue("0");
                }
            } catch (Exception e) {
                Log.e(TAG, e.toString());
            }
        } else {
            removePreference(KEY_SYSTEM_ROTATION);
        }
        int displayNumber = DrmDisplaySetting.getDisplayNumber();
        Log.v(TAG, "displayNumber=" + displayNumber);
        String[] connectorInfos = DrmDisplaySetting.getConnectorInfo();
        if (null != connectorInfos) {
            for (int i = 0; i < connectorInfos.length; i++) {
                Log.v(TAG, i + " connectorInfo====" + connectorInfos[i]);
            }
        }
        mDisplayInfoList.clear();
        for (int i = 0; i < displayNumber; i++) {
            String typeName = "";
            String id = "";
            if (null != connectorInfos && connectorInfos.length == displayNumber) {
                String[] rets = connectorInfos[i].split(",");
                if (null != rets && rets.length > 2) {
                    String type = rets[0].replaceAll("type:", "");
                    typeName = DrmDisplaySetting.CONNECTOR_DISPLAY_NAME.get(type);
                    id = rets[1].replaceAll("id:", "");
                    Log.v(TAG, "type======" + type + ", typeName=" + typeName + ", id=" + id);
                    if (DrmDisplaySetting.DRM_MODE_CONNECTOR_VIRTUAL.equals(typeName)
                            || DrmDisplaySetting.DRM_MODE_CONNECTOR_eDP.equals(typeName)
                            || DrmDisplaySetting.DRM_MODE_CONNECTOR_DSI.equals(typeName)) {
                        continue;
                    }
                }
            }
            int display = i;
            PreferenceCategory category = new PreferenceCategory(mContext);
            category.setKey(KEY_PRE_CATE + display);
            if ("0".equals(id) || "".equals(id)) {
                category.setTitle(typeName);
            } else {
                category.setTitle(typeName + "-" + id);
            }
            getPreferenceScreen().addPreference(category);
            //add resolution preference
            HdmiListPreference resolutionPreference = new HdmiListPreference(mContext);
            resolutionPreference.setKey(KEY_PRE_RESOLUTION + display);
            resolutionPreference.setTitle(mContext.getString(R.string.screen_resolution));
            resolutionPreference.setOnPreferenceClickListener(this);
            resolutionPreference.setOnPreferenceChangeListener(this);
            category.addPreference(resolutionPreference);
            //add scale preference
            Preference scalePreference = new Preference(mContext);
            scalePreference.setKey(KEY_PRE_SCREEN_SCALE + display);
            scalePreference.setTitle(mContext.getString(R.string.screen_scale));
            scalePreference.setOnPreferenceClickListener(this);
            category.addPreference(scalePreference);
            category.setEnabled(false);
            DisplayInfo displayInfo = new DisplayInfo();
            displayInfo.setDisplayNo(display);
            mDisplayInfoList.put(display, displayInfo);
        }
        sendUpdateStateMsg(ITEM_CONTROL.REFRESH_DISPLAY_STATUS_INFO, 0);

        mAuxCategory = (PreferenceCategory) findPreference(KEY_AUX_CATEGORY);
        mAuxScreenVH = (CheckBoxPreference) findPreference(KEY_AUX_SCREEN_VH);
        mAuxScreenVH.setChecked(SystemProperties.getBoolean("persist.sys.rotation.efull", false));
        mAuxScreenVH.setOnPreferenceChangeListener(this);
        mAuxCategory.removePreference(mAuxScreenVH);
        mAuxScreenVHList = (ListPreference) findPreference(KEY_AUX_SCREEN_VH_LIST);
        mAuxScreenVHList.setOnPreferenceChangeListener(this);
        mAuxScreenVHList.setOnPreferenceClickListener(this);
        mAuxCategory.removePreference(mAuxScreenVHList);
    }

    private void sendSwitchDeviceOffOnMsg(ITEM_CONTROL control, int status) {
        mEnableDisplayListener = false;
        Message msg = new Message();
        msg.what = MSG_SWITCH_DEVICE_STATUS;
        msg.arg1 = status;
        msg.obj = control;
        mHandler.removeMessages(MSG_SWITCH_DEVICE_STATUS, control);
        mHandler.sendMessageDelayed(msg, SWITCH_DEVICE_DELAY_TIME);
    }

    public static void write2Node(String node, String values) {
        Log.v(TAG, "write " + node + " " + values);
        RandomAccessFile raf = null;
        try {
            raf = new RandomAccessFile(node, "rw");
            raf.writeBytes(values);
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            if (null != raf) {
                try {
                    raf.close();
                } catch (Exception e) {
                    //e.printStackTrace();
                }
            }
        }
    }

    private void updateResolution(final ITEM_CONTROL control, final int index) {
        showWaitingDialog(R.string.dialog_update_resolution);
        mEnableDisplayListener = false;
        new Thread() {
            @Override
            public void run() {
                if (ITEM_CONTROL.CHANGE_RESOLUTION == control) {
                    synchronized (mLock) {
                        int display = mSelectDisplayInfo.getDisplayNo();
                        DrmDisplaySetting.updateDisplayInfos();
                        DrmDisplaySetting.updateDisplayModesInfo(mSelectDisplayInfo);
                        int status = DrmDisplaySetting.getCurrentDpyConnState(display);
                        mSelectDisplayInfo.setStatus(status);
                        String[] modes = mSelectDisplayInfo.getOrginModes();
                        Log.v(TAG, "display " + display + ", status=" + status + ", modes=" + modes);
                        if (DPY_STATUS_CONNECTED == status && null != modes && modes.length > 0) {
                            DrmDisplaySetting.setDisplayModeTemp(mSelectDisplayInfo, index);
//                            String mode = Arrays.asList(modes).get(index);
//                            DrmDisplaySetting.setMode(display, mode);
                            if (USED_OFFON_RESOLUTION) {
                                sendSwitchDeviceOffOnMsg(control, SWITCH_STATUS_OFF_ON);
                            } else {
                                Message message = new Message();
                                message.what = MSG_SHOW_CONFIRM_DIALOG;
                                message.obj = control;
                                mHandler.sendMessageDelayed(message, 300);
                            }
                        } else {
                            Message message = new Message();
                            message.what = MSG_UPDATE_STATUS_UI;
                            message.obj = ITEM_CONTROL.REFRESH_DISPLAY_STATUS_INFO;
                            mHandler.sendMessage(message);
                        }
                    }
                }
            }
        }.start();
    }

    private void sendUpdateStateMsg(ITEM_CONTROL control, long delayMillis) {
        if (mDestory) {
            return;
        }
        Message msg = new Message();
        msg.what = MSG_UPDATE_STATUS;
        msg.obj = control;
        //增加延迟，保证数据能够拿到
        mHandler.removeMessages(MSG_UPDATE_STATUS, control);
        mHandler.sendMessageDelayed(msg, delayMillis);
    }

    private void getDisplayStatusInfo() {
        synchronized (mLock) {
            if (mDestory) {
                return;
            }
            for (Map.Entry<Integer, DisplayInfo> entry : mDisplayInfoList.entrySet()) {
                int display = entry.getKey();
                int status = DrmDisplaySetting.getCurrentDpyConnState(display);
                DisplayInfo displayInfo = entry.getValue();
                displayInfo.setStatus(status);
                Log.v(TAG, "display " + display + ", status=" + status);
            }
        }
    }

    private void getDisplayResolutionInfo(DisplayInfo displayInfo) {
        synchronized (mLock) {
            if (mDestory) {
                return;
            }
            if (null != displayInfo
                    && DPY_STATUS_CONNECTED == displayInfo.getStatus()) {
                DrmDisplaySetting.updateDisplayModesInfo(displayInfo);
            }
        }
    }

    private void updateStateUI() {
        if (mDestory) {
            return;
        }
        for (Map.Entry<Integer, DisplayInfo> entry : mDisplayInfoList.entrySet()) {
            int display = entry.getKey();
            DisplayInfo displayInfo = entry.getValue();
            Preference cate = findPreference(KEY_PRE_CATE + display);
            if (DPY_STATUS_CONNECTED == displayInfo.getStatus()) {
                cate.setEnabled(true);
            } else {
                cate.setEnabled(false);
            }
        }
    }

    private void showResolutionItemUI(DisplayInfo displayInfo) {
        String[] modes = null == displayInfo.getOrginModes() ? new String[]{} :
                displayInfo.getOrginModes();
        HdmiListPreference resolutionPreference = (HdmiListPreference) findPreference(
                KEY_PRE_RESOLUTION + displayInfo.getDisplayNo());
        resolutionPreference.setEntries(modes);
        resolutionPreference.setEntryValues(modes);
        resolutionPreference.setValue(displayInfo.getCurrentResolution());
        resolutionPreference.showClickDialogItem();
    }

    protected void showConfirmSetModeDialog() {
        //mMainDisplayInfo = getDisplayInfo(0);
        if (mSelectDisplayInfo != null && mResume) {
            Log.v(TAG, "showConfirmSetModeDialog");
            DialogFragment df = ConfirmSetModeDialogFragment.newInstance(mSelectDisplayInfo, new ConfirmSetModeDialogFragment.OnDialogDismissListener() {
                @Override
                public void onDismiss(boolean isok) {
                    Log.i(TAG, "showConfirmSetModeDialog->onDismiss->isok:" + isok);
                    Log.i(TAG, "showConfirmSetModeDialog->onDismiss->mOldResolution:" + mSelectDisplayInfo.getCurrentResolution());
                    synchronized (mLock) {
                        DrmDisplaySetting.confirmSaveDisplayMode(mSelectDisplayInfo, isok);
                        if (!isok) {
                            Preference cate = findPreference(KEY_PRE_CATE + mSelectDisplayInfo.getDisplayNo());
                            cate.setEnabled(false);
                            if (USED_OFFON_RESOLUTION) {
                                showWaitingDialog(R.string.dialog_wait_screen_connect);
                                sendSwitchDeviceOffOnMsg(ITEM_CONTROL.REFRESH_DISPLAY_STATUS_INFO, SWITCH_STATUS_OFF_ON);
                            } else {
                                showWaitingDialog(R.string.dialog_update_resolution);
                                sendUpdateStateMsg(ITEM_CONTROL.REFRESH_DISPLAY_STATUS_INFO, 1000);
                            }
                        } else if (!USED_OFFON_RESOLUTION) {
                            updateStateUI();
                        }
                    }
                }
            });
            df.show(getFragmentManager(), "ConfirmDialog");
        }
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        // TODO Auto-generated method stub
        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        String key = preference.getKey();
        Log.i(TAG, "onPreferenceClick " + key);
        if (key.startsWith(KEY_PRE_SCREEN_SCALE)) {
            int display = Integer.parseInt(key.replace(KEY_PRE_SCREEN_SCALE, ""));
            int status = DrmDisplaySetting.getCurrentDpyConnState(display);
            if (DPY_STATUS_CONNECTED == status) {
                Intent screenScaleIntent = new Intent(getActivity(), ScreenScaleActivity.class);
                screenScaleIntent.putExtra(ScreenScaleActivity.EXTRA_DISPLAY, display);
                startActivity(screenScaleIntent);
            } else {
                Preference cate = findPreference(KEY_PRE_CATE + display);
                cate.setEnabled(false);
            }
        } else if (key.startsWith(KEY_PRE_RESOLUTION)) {
            for (Map.Entry<Integer, DisplayInfo> entry : mDisplayInfoList.entrySet()) {
                int display = Integer.parseInt(key.replace(KEY_PRE_RESOLUTION, ""));
                if (display == entry.getKey()) {
                    mSelectDisplayInfo = entry.getValue();
                }
            }
            showWaitingDialog(R.string.dialog_getting_screen_info);
            sendUpdateStateMsg(ITEM_CONTROL.SHOW_RESOLUTION_ITEM, 1000);
        } else if (preference == mAuxScreenVHList) {
            String value = SystemProperties.get("persist.sys.rotation.einit", "0");
            mAuxScreenVHList.setValue(value);
        }
        return true;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object obj) {
        String key = preference.getKey();
        Log.i(TAG, key + " onPreferenceChange:" + obj);
        if (key.startsWith(KEY_PRE_RESOLUTION)) {
            if (null == mSelectDisplayInfo
                    || obj.equals(mSelectDisplayInfo.getCurrentResolution())) {
                return true;
            }
            HdmiListPreference resolutionPreference = (HdmiListPreference) preference;
            int index = resolutionPreference.findIndexOfValue((String) obj);
            Log.i(TAG, "resolutionPreference: index= " + index);
            if (-1 == index) {
                Log.e(TAG, "onPreferenceChange: index=-1 start print");
                CharSequence[] temps = resolutionPreference.getEntryValues();
                if (null == temps) {
                    for (CharSequence temp : temps) {
                        Log.i(TAG, "=======" + temp);
                    }
                } else {
                    Log.e(TAG, "mResolution.getEntryValues() is null, but set " + obj);
                }
                Log.e(TAG, "onPreferenceChange: index=-1 end print");
            }
            preference.getParent().setEnabled(false);
            updateResolution(ITEM_CONTROL.CHANGE_RESOLUTION, index);
        } else if (preference == mSystemRotation) {
            if (KEY_SYSTEM_ROTATION.equals(key)) {
                try {
                    int value = Integer.parseInt((String) obj);
                    android.os.SystemProperties.set("persist.sys.orientation", (String) obj);
                    Log.d(TAG, "freezeRotation~~~value:" + (String) obj);
                    if (value == 0) {
                        mWindowManager.freezeRotation(Surface.ROTATION_0);
                    } else if (value == 90) {
                        mWindowManager.freezeRotation(Surface.ROTATION_90);
                    } else if (value == 180) {
                        mWindowManager.freezeRotation(Surface.ROTATION_180);
                    } else if (value == 270) {
                        mWindowManager.freezeRotation(Surface.ROTATION_270);
                    } else {
                        return true;
                    }
                    //android.os.SystemProperties.set("sys.boot_completed", "1");
                } catch (Exception e) {
                    Log.e(TAG, "freezeRotation error");
                }
            }
        } else if (preference == mAuxScreenVH) {
            mEnableDisplayListener = false;
            showWaitingDialog(R.string.dialog_wait_screen_connect);
            if ((Boolean) obj) {
                SystemProperties.set("persist.sys.rotation.efull", "true");
            } else {
                SystemProperties.set("persist.sys.rotation.efull", "false");
            }
            sendSwitchDeviceOffOnMsg(ITEM_CONTROL.REFRESH_DISPLAY_STATUS_INFO, SWITCH_STATUS_OFF_ON);
        } else if (preference == mAuxScreenVHList) {
            mEnableDisplayListener = false;
            showWaitingDialog(R.string.dialog_wait_screen_connect);
            SystemProperties.set("persist.sys.rotation.einit", obj.toString());
            //mDisplayManager.forceScheduleTraversalLocked();
            sendSwitchDeviceOffOnMsg(ITEM_CONTROL.REFRESH_DISPLAY_STATUS_INFO, SWITCH_STATUS_OFF_ON);
        }
        return true;
    }

    public static boolean isAvailable() {
        return "true".equals(SystemProperties.get("ro.vendor.hdmi_settings"));
    }

    private void refreshState() {
        Log.v(TAG, "refreshState");
        showWaitingDialog(R.string.dialog_getting_screen_info);
        sendUpdateStateMsg(ITEM_CONTROL.REFRESH_DISPLAY_STATUS_INFO, 1000);
    }

    class DisplayListener implements DisplayManager.DisplayListener {
        @Override
        public void onDisplayAdded(int displayId) {
            Log.v(TAG, "onDisplayAdded displayId=" + displayId);
            if (mEnableDisplayListener) {
                refreshState();
            }
        }

        @Override
        public void onDisplayChanged(int displayId) {
            Log.v(TAG, "onDisplayChanged displayId=" + displayId);
            //refreshState();
        }

        @Override
        public void onDisplayRemoved(int displayId) {
            Log.v(TAG, "onDisplayRemoved displayId=" + displayId);
            if (mEnableDisplayListener) {
                refreshState();
            }
        }
    }
}
