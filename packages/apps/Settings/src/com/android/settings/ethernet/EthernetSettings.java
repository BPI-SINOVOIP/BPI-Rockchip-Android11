/*
 * Copyright (C) 2009 The Android Open Source Project
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
package com.android.settings.ethernet;

import com.android.settings.R;
import com.android.settings.SettingsPreferenceFragment;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.admin.DevicePolicyManager;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.os.UserManager;
import android.preference.CheckBoxPreference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceScreen;
import android.provider.SearchIndexableResource;
import android.provider.Settings;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;
import android.content.Intent;

import androidx.preference.SwitchPreference;
import androidx.preference.ListPreference;
import androidx.preference.Preference;

import java.io.File;
import java.io.FileDescriptor;
import java.io.File;
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;

import java.util.regex.Pattern;
import java.lang.Integer;
import java.net.InetAddress;
import java.net.Inet4Address;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;

import com.android.settings.SettingsPreferenceFragment.SettingsDialogFragment;

/*for 5.0*/
import android.net.EthernetManager;
import android.net.IpConfiguration;
import android.net.IpConfiguration.IpAssignment;
import android.net.IpConfiguration.ProxySettings;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.StaticIpConfiguration;
import android.net.NetworkUtils;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.widget.Toast;
//import android.preference.ListPreference;
//import com.android.internal.logging.MetricsProto.MetricsEvent;
import com.android.internal.logging.nano.MetricsProto.MetricsEvent;


import com.android.settings.ethernet.ethernet_static_ip_dialog;

public class EthernetSettings extends SettingsPreferenceFragment
        implements DialogInterface.OnClickListener, Preference.OnPreferenceChangeListener {
    private static final String TAG = "EthernetSettings";

    public enum ETHERNET_STATE {
        ETHER_STATE_DISCONNECTED,
        ETHER_STATE_CONNECTING,
        ETHER_STATE_CONNECTED
    }

    private static final String KEY_ETH_IP_ADDRESS = "ethernet_ip_addr";
    private static final String KEY_ETH_HW_ADDRESS = "ethernet_hw_addr";
    private static final String KEY_ETH_NET_MASK = "ethernet_netmask";
    private static final String KEY_ETH_GATEWAY = "ethernet_gateway";
    private static final String KEY_ETH_DNS1 = "ethernet_dns1";
    private static final String KEY_ETH_DNS2 = "ethernet_dns2";
    private static final String KEY_ETH_MODE = "ethernet_mode_select";


    private static String mEthHwAddress = null;
    private static String mEthIpAddress = null;
    private static String mEthNetmask = null;
    private static String mEthGateway = null;
    private static String mEthdns1 = null;
    private static String mEthdns2 = null;
    private final static String nullIpInfo = "0.0.0.0";

    private ListPreference mkeyEthMode;
    //    private SwitchPreference mEthCheckBox;
    private CheckBoxPreference staticEthernet;

    private final IntentFilter mIntentFilter;
    IpConfiguration mIpConfiguration;
    EthernetManager mEthManager;
    StaticIpConfiguration mStaticIpConfiguration;
    Context mContext;
    private ethernet_static_ip_dialog mDialog;
    private String mIfaceName;
    private long mChangeTime;
    private static final int SHOW_RENAME_DIALOG = 0;
    private static final int ETHER_IFACE_STATE_DOWN = 0;
    private static final int ETHER_IFACE_STATE_UP = 1;

    private static final String FILE = "/sys/class/net/eth0/flags";
    private static final int MSG_GET_ETHERNET_STATE = 0;

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            if (MSG_GET_ETHERNET_STATE == msg.what) {
                handleEtherStateChange((ETHERNET_STATE) msg.obj);
            }
        }
    };

    @Override
    public int getMetricsCategory() {
        return MetricsEvent.WIFI_TETHER_SETTINGS;
    }

    @Override
    public int getDialogMetricsCategory(int dialogId) {
        switch (dialogId) {
            case SHOW_RENAME_DIALOG:
                return MetricsEvent.WIFI_TETHER_SETTINGS;
            default:
                return 0;
        }
    }


    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            log("Action " + action);
            if (ConnectivityManager.CONNECTIVITY_ACTION.equals(action)) {
                NetworkInfo info = intent.getParcelableExtra(ConnectivityManager.EXTRA_NETWORK_INFO);
                Log.v(TAG, "===" + info.toString());
                if (null != info && ConnectivityManager.TYPE_ETHERNET == info.getType()) {
                    long currentTime = System.currentTimeMillis();
                    int delayTime = 0;
                    if (currentTime - mChangeTime < 1000) {
                        delayTime = 2000;
                    }
                    if (NetworkInfo.State.CONNECTED == info.getState()) {
                        handleEtherStateChange(ETHERNET_STATE.ETHER_STATE_CONNECTED, delayTime);
                    } else if (NetworkInfo.State.DISCONNECTED == info.getState()) {
                        handleEtherStateChange(ETHERNET_STATE.ETHER_STATE_DISCONNECTED, delayTime);
                    }
                }
            }
        }
    };

    public EthernetSettings() {
        mIntentFilter = new IntentFilter();
        mIntentFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
    }

    private void handleEtherStateChange(ETHERNET_STATE EtherState, long delayMillis) {
        mHandler.removeMessages(MSG_GET_ETHERNET_STATE);
        if (delayMillis > 0) {
            Message msg = new Message();
            msg.what = MSG_GET_ETHERNET_STATE;
            msg.obj = EtherState;
            mHandler.sendMessageDelayed(msg, delayMillis);
        } else {
            handleEtherStateChange(EtherState);
        }
    }

    private void handleEtherStateChange(ETHERNET_STATE EtherState) {
        log("curEtherState" + EtherState);

        switch (EtherState) {
            case ETHER_STATE_DISCONNECTED:
                mEthHwAddress = nullIpInfo;
                mEthIpAddress = nullIpInfo;
                mEthNetmask = nullIpInfo;
                mEthGateway = nullIpInfo;
                mEthdns1 = nullIpInfo;
                mEthdns2 = nullIpInfo;
                break;
            case ETHER_STATE_CONNECTING:
                String mStatusString = this.getResources().getString(R.string.ethernet_info_getting);
                mEthHwAddress = mStatusString;
                mEthIpAddress = mStatusString;
                mEthNetmask = mStatusString;
                mEthGateway = mStatusString;
                mEthdns1 = mStatusString;
                mEthdns2 = mStatusString;
                break;
            case ETHER_STATE_CONNECTED:
                getEthInfo();
                break;
        }

        refreshUI();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.ethernet_settings);

        mContext = this.getActivity().getApplicationContext();
        mEthManager = (EthernetManager) getSystemService(Context.ETHERNET_SERVICE);

        if (mEthManager == null) {
            Log.e(TAG, "get ethernet manager failed");
            Toast.makeText(mContext, R.string.disabled_feature, Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        String[] ifaces = mEthManager.getAvailableInterfaces();
        if (ifaces.length > 0) {
            mIfaceName = ifaces[0];//"eth0";
        }
        if (null == mIfaceName) {
            Log.e(TAG, "get ethernet ifaceName failed");
            Toast.makeText(mContext, R.string.disabled_feature, Toast.LENGTH_SHORT).show();
            finish();
        }
    }

    private Inet4Address getIPv4Address(String text) {
        try {
            return (Inet4Address) NetworkUtils.numericToInetAddress(text);
        } catch (IllegalArgumentException | ClassCastException e) {
            return null;
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (null == mIfaceName) {
            return;
        }
        if (mkeyEthMode == null) {
            mkeyEthMode = (ListPreference) findPreference(KEY_ETH_MODE);
            mkeyEthMode.setOnPreferenceChangeListener(this);
        }
    /*
        if (mEthCheckBox== null) {
            mEthCheckBox = (SwitchPreference) findPreference("ethernet");
            mEthCheckBox.setOnPreferenceChangeListener(this);
        }
    */
        //handleEtherStateChange(1 == getEthernetCarrierState(mIfaceName)? EthernetManager.ETHER_STATE_CONNECTED/*mEthManager.getEthernetConnectState()*/);
        refreshUI();
        log("resume");
        mContext.registerReceiver(mReceiver, mIntentFilter);
    }

    @Override
    public void onPause() {
        super.onPause();
        if (null == mIfaceName) {
            return;
        }
        mContext.unregisterReceiver(mReceiver);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mHandler.removeMessages(MSG_GET_ETHERNET_STATE);
        log("destory");
    }

    @Override
    public void onStop() {
        super.onStop();
        log("stop");
    }

    private void setStringSummary(String preference, String value) {
        try {
            findPreference(preference).setSummary(value);
        } catch (RuntimeException e) {
            findPreference(preference).setSummary("");
            log("can't find " + preference);
        }
    }

    private String getStringFromPref(String preference) {
        try {
            return findPreference(preference).getSummary().toString();
        } catch (RuntimeException e) {
            return null;
        }
    }

    private void refreshUI() {

        //    setStringSummary(KEY_ETH_HW_ADDRESS,mEthHwAddress);

        setStringSummary(KEY_ETH_IP_ADDRESS, mEthIpAddress);
        setStringSummary(KEY_ETH_NET_MASK, mEthNetmask);
        setStringSummary(KEY_ETH_GATEWAY, mEthGateway);
        setStringSummary(KEY_ETH_DNS1, mEthdns1);
        setStringSummary(KEY_ETH_DNS2, mEthdns2);
        updateCheckbox();
    }

    private void updateCheckbox() {  //add by ljh for adding a checkbox switch

        if (mEthManager == null) {
            mkeyEthMode.setSummary("null");
        } else {
            IpAssignment mode = mEthManager.getConfiguration(mIfaceName).getIpAssignment();
            if (mode == IpAssignment.DHCP || mode == IpAssignment.UNASSIGNED) {
                mkeyEthMode.setValue("DHCP");
                mkeyEthMode.setSummary(R.string.usedhcp);
            } else {
                mkeyEthMode.setValue("StaticIP");
                mkeyEthMode.setSummary(R.string.usestatic);
            }
/*            int isEnable = mEthManager.getEthernetIfaceState();
            if(isEnable == ETHER_IFACE_STATE_UP) {
                mEthCheckBox.setChecked(true);
            }else{
                mEthCheckBox.setChecked(false);
            }
*/
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        log("onPreferenceChange");
        if (preference == mkeyEthMode) {
            String value = (String) newValue;
            if (value.equals("DHCP")) {
                mChangeTime = System.currentTimeMillis();
                handleEtherStateChange(ETHERNET_STATE.ETHER_STATE_CONNECTING);
                mEthManager.setConfiguration(mIfaceName, new IpConfiguration(IpAssignment.DHCP, ProxySettings.NONE, null, null));
                log("switch to dhcp");
            } else if (value.equals("StaticIP")) {
                log("static editor");
                this.showDialog(SHOW_RENAME_DIALOG);
            }

        }
        return true;
    }

    //将子网掩码转换成ip子网掩码形式，比如输入32输出为255.255.255.255
    public String interMask2String(int prefixLength) {
        String netMask = null;
        int inetMask = prefixLength;

        int part = inetMask / 8;
        int remainder = inetMask % 8;
        int sum = 0;

        for (int i = 8; i > 8 - remainder; i--) {
            sum = sum + (int) Math.pow(2, i - 1);
        }

        if (part == 0) {
            netMask = sum + ".0.0.0";
        } else if (part == 1) {
            netMask = "255." + sum + ".0.0";
        } else if (part == 2) {
            netMask = "255.255." + sum + ".0";
        } else if (part == 3) {
            netMask = "255.255.255." + sum;
        } else if (part == 4) {
            netMask = "255.255.255.255";
        }

        return netMask;
    }

    /*
     * convert subMask string to prefix length
     */
    private int maskStr2InetMask(String maskStr) {
        StringBuffer sb;
        String str;
        int inetmask = 0;
        int count = 0;
        /*
         * check the subMask format
         */
        Pattern pattern = Pattern.compile("(^((\\d|[01]?\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d|[01]?\\d\\d|2[0-4]\\d|25[0-5])$)|^(\\d|[1-2]\\d|3[0-2])$");
        if (pattern.matcher(maskStr).matches() == false) {
            Log.e(TAG, "subMask is error");
            return 0;
        }

        String[] ipSegment = maskStr.split("\\.");
        for (int n = 0; n < ipSegment.length; n++) {
            sb = new StringBuffer(Integer.toBinaryString(Integer.parseInt(ipSegment[n])));
            str = sb.reverse().toString();
            count = 0;
            for (int i = 0; i < str.length(); i++) {
                i = str.indexOf("1", i);
                if (i == -1)
                    break;
                count++;
            }
            inetmask += count;
        }
        return inetmask;
    }

    private boolean setStaticIpConfiguration() {

        mStaticIpConfiguration = new StaticIpConfiguration();
        /*
         * get ip address, netmask,dns ,gw etc.
         */
        Inet4Address inetAddr = getIPv4Address(this.mEthIpAddress);
        int prefixLength = maskStr2InetMask(this.mEthNetmask);
        InetAddress gatewayAddr = getIPv4Address(this.mEthGateway);
        InetAddress dnsAddr = getIPv4Address(this.mEthdns1);

        if (inetAddr.getAddress().toString().isEmpty() || prefixLength == 0 || gatewayAddr.toString().isEmpty()
                || dnsAddr.toString().isEmpty()) {
            log("ip,mask or dnsAddr is wrong");
            return false;
        }

        String dnsStr2 = this.mEthdns2;
        mStaticIpConfiguration.ipAddress = new LinkAddress(inetAddr, prefixLength);
        mStaticIpConfiguration.gateway = gatewayAddr;
        mStaticIpConfiguration.dnsServers.add(dnsAddr);

        if (!dnsStr2.isEmpty()) {
            mStaticIpConfiguration.dnsServers.add(getIPv4Address(dnsStr2));
        }
        mIpConfiguration = new IpConfiguration(IpAssignment.STATIC, ProxySettings.NONE, mStaticIpConfiguration, null);
        return true;
    }

    public void getEthInfoFromDhcp() {
        String tempIpInfo;

        tempIpInfo = /*SystemProperties.get("dhcp."+ iface +".ipaddress");*/
                mEthManager.getIpAddress(mIfaceName);

        if ((tempIpInfo != null) && (!tempIpInfo.equals(""))) {
            mEthIpAddress = tempIpInfo;
        } else {
            mEthIpAddress = nullIpInfo;
        }

        tempIpInfo = /*SystemProperties.get("dhcp."+ iface +".mask");*/
                mEthManager.getNetmask(mIfaceName);
        if ((tempIpInfo != null) && (!tempIpInfo.equals(""))) {
            mEthNetmask = tempIpInfo;
        } else {
            mEthNetmask = nullIpInfo;
        }

        tempIpInfo = /*SystemProperties.get("dhcp."+ iface +".gateway");*/
                mEthManager.getGateway(mIfaceName);
        if ((tempIpInfo != null) && (!tempIpInfo.equals(""))) {
            mEthGateway = tempIpInfo;
        } else {
            mEthGateway = nullIpInfo;
        }

        tempIpInfo = /*SystemProperties.get("dhcp."+ iface +".dns1");*/
                mEthManager.getDns(mIfaceName);
        if ((tempIpInfo != null) && (!tempIpInfo.equals(""))) {
            String data[] = tempIpInfo.split(",");
            mEthdns1 = data[0];
            if (data.length <= 1) {
                mEthdns2 = nullIpInfo;
            } else {
                mEthdns2 = data[1];
            }
        } else {
            mEthdns1 = nullIpInfo;
        }
    }

    public void getEthInfoFromStaticIp() {
        StaticIpConfiguration staticIpConfiguration = mEthManager.getConfiguration(mIfaceName).getStaticIpConfiguration();

        if (staticIpConfiguration == null) {
            return;
        }
        LinkAddress ipAddress = staticIpConfiguration.ipAddress;
        InetAddress gateway = staticIpConfiguration.gateway;
        ArrayList<InetAddress> dnsServers = staticIpConfiguration.dnsServers;

        if (ipAddress != null) {
            mEthIpAddress = ipAddress.getAddress().getHostAddress();
            mEthNetmask = interMask2String(ipAddress.getPrefixLength());
        }
        if (gateway != null) {
            mEthGateway = gateway.getHostAddress();
        }
        mEthdns1 = dnsServers.get(0).getHostAddress();

        if (dnsServers.size() > 1) { /* 只保留两个*/
            mEthdns2 = dnsServers.get(1).getHostAddress();
        }
    }

    /*
     * TODO:
     */
    public void getEthInfo() {
        /*
        mEthHwAddress = mEthManager.getEthernetHwaddr(mEthManager.getEthernetIfaceName());
        if (mEthHwAddress == null) mEthHwAddress = nullIpInfo;
        */
        IpAssignment mode = mEthManager.getConfiguration(mIfaceName).getIpAssignment();


        if (mode == IpAssignment.DHCP || mode == IpAssignment.UNASSIGNED) {
            /*
             * getEth from dhcp
             */
            getEthInfoFromDhcp();
        } else if (mode == IpAssignment.STATIC) {
            /*
             * TODO: get static IP
             */
            getEthInfoFromStaticIp();
        }
    }

    /*
     * tools
     */
    private void log(String s) {
        Log.d(TAG, s);
    }

    @Override
    public void onClick(DialogInterface dialogInterface, int button) {
        if (button == ethernet_static_ip_dialog.BUTTON_SUBMIT) {
            mDialog.saveIpSettingInfo(); //从Dialog获取静态数据
            if (setStaticIpConfiguration()) {
                mChangeTime = System.currentTimeMillis();
                handleEtherStateChange(ETHERNET_STATE.ETHER_STATE_CONNECTING);
                mEthManager.setConfiguration(mIfaceName, mIpConfiguration);
            } else {
                Log.e(TAG, mIpConfiguration.toString());
            }
        }
        updateCheckbox();
    }

    @Override
    public Dialog onCreateDialog(int dialogId) {
        log("onCreateDialog " + dialogId);
        switch (dialogId) {
            case SHOW_RENAME_DIALOG:

                mDialog = new ethernet_static_ip_dialog(getActivity(), false, this, mGetStaticIpInfo, mIfaceName);
                return mDialog;
        }
        return super.onCreateDialog(dialogId);
    }

    /*interface*/

    public getStaticIpInfo mGetStaticIpInfo = new getStaticIpInfo() {

        public boolean getStaticIp(String ipAddr) {
            mEthIpAddress = ipAddr;

            log("ipAddr: " + ipAddr);
            return true;
        }

        public boolean getStaticNetMask(String netMask) {
            mEthNetmask = netMask;

            log("netMask: " + netMask);
            return true;
        }

        public boolean getStaticGateway(String gateway) {
            mEthGateway = gateway;

            log("gateway: " + gateway);
            return true;
        }

        public boolean getStaticDns1(String dns1) {
            mEthdns1 = dns1;

            log("dns1: " + dns1);
            return true;
        }

        public boolean getStaticDns2(String dns2) {
            mEthdns2 = dns2;

            log("dns2: " + dns2);
            return true;
        }
    };

    private String ReadFromFile(File file) {
        if ((file != null) && file.exists()) {
            try {
                FileInputStream fin = new FileInputStream(file);
                BufferedReader reader = new BufferedReader(new InputStreamReader(fin));
                String flag = reader.readLine();
                fin.close();
                return flag;
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return null;
    }

    private int getEthernetCarrierState(String iface) {
        if (iface != "") {
            try {
                File file = new File("/sys/class/net/" + iface + "/carrier");
                String carrier = ReadFromFile(file);
                return Integer.parseInt(carrier);
            } catch (Exception e) {
                e.printStackTrace();
                return 0;
            }
        } else {
            return 0;
        }
    }

    public static boolean isAvailable() {
        return "true".equals(SystemProperties.get("ro.vendor.ethernet_settings"));
    }
}