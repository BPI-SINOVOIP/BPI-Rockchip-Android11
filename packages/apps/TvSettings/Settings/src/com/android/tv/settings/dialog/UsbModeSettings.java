package com.android.tv.settings.dialog;

import com.android.tv.settings.R;

import android.content.Context;
import android.os.Handler;
import java.io.File;
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import android.os.storage.StorageManager;
import android.os.storage.StorageEventListener;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.SystemProperties;
import android.text.TextUtils;

import android.util.Log;

public class UsbModeSettings {
    private static final String TAG = "UsbModeSettings";
    // 0 otg 1 host 2 peripheral
    public static final String HOST_MODE = new String("host");
    public static final String SLAVE_MODE = new String("otg");
    public static final String USB_BOOT_PRO = "persist.usb.boot";
    public static final String PROP_FAKE_DATA_ROLE = "persist.fake_data_role";
    private static final String FILE_NAME_RK3399 = "/sys/kernel/debug/usb@fe800000/rk_usb_force_mode";
    private static final String FILE_NAME_RK3328 = "/sys/devices/platform/ff450000.syscon/ff450000.syscon:usb2-phy@100/otg_mode";
    private static final String FILE_NAME_RK3229 = "/sys/devices/platform/11000000.syscon/11000000.syscon:usb2-phy@760/otg_mode";
    private static final String FILE_NAME_RK356X = "/sys/devices/platform/fe8a0000.usb2-phy/otg_mode";

    private File file = null;

    private StorageManager mStorageManager = null;
    private String mMode = "";
    private String mSocName = "";

    private Context mContext;

    private boolean mLock = false;

    public UsbModeSettings(Context context) {
        mContext = context;
        mSocName = SystemProperties.get("sys.rk.soc");
        if (TextUtils.isEmpty(mSocName)) {
            mSocName = SystemProperties.get("ro.board.platform");
        }
        if(!TextUtils.isEmpty(mSocName) && mSocName.contains("rk3399")){
            file = new File(FILE_NAME_RK3399);
        }else{
            file = new File(FILE_NAME_RK3328);
        }
        file = getFile();
        mStorageManager = (StorageManager) mContext.getSystemService(Context.STORAGE_SERVICE);
        boolean ret = checkFile();
        String mode = ReadFromFile(file);
        if(ret && !TextUtils.isEmpty(mode)) {
            mMode = mode;
        }

    }

    private File getFile() {
        String fileName = FILE_NAME_RK3328;
        if (!TextUtils.isEmpty(mSocName) && mSocName.contains("rk3399")) {
            fileName = FILE_NAME_RK3399;
        } else if (!TextUtils.isEmpty(mSocName) && (mSocName.contains("rk322x") || mSocName.contains("rk3128h"))) {
            fileName = FILE_NAME_RK3229;
        } else if (!TextUtils.isEmpty(mSocName) && mSocName.contains("rk356x")) {
            fileName = FILE_NAME_RK356X;
        } else {
            fileName = FILE_NAME_RK3328;
        }
        return new File(fileName);
    }

    public boolean getDefaultValue() {
        if (isRk3368()) {//通过prop和init来设置otg模式
            return getFakeDataRole();
        }
        if (checkFile()) {
            Log.d("UsbModeSelect", "/data/otg.cfg not exist,but temp file exist");
            if (isRk3368()) {
                return getFakeDataRole();
            }
            if (mMode.equals(HOST_MODE)) {
                return false;
            } else {
                return true;
            }
        } else {
            mMode = HOST_MODE;
            return false;
        }
    }

    private String ReadFromFile(File file) {
        if (checkFile()) {
            try {
                FileInputStream fin = new FileInputStream(file);
                BufferedReader reader = new BufferedReader(new InputStreamReader(fin));
                String config = reader.readLine();
                fin.close();
                return config;
            } catch (IOException e) {
                Log.i(TAG, "ReadFromFile exception:" + e);
                e.printStackTrace();
            }
        }

        return null;
    }

    private void Write2File(File file, String mode) {
        if (!checkFile() || (mode == null))
            return;
        Log.d("UsbModeSelect", "Write2File,write mode = " + mode);

        try {
            FileOutputStream fout = new FileOutputStream(file);
            PrintWriter pWriter = new PrintWriter(fout);
            pWriter.println(mode);
            pWriter.flush();
            pWriter.close();
            fout.close();
        } catch (IOException re) {
        }
    }

    public void onUsbModeClick(String mode) {
        if (isRk3368()) {
            setPropFakeDataRole(mode);
            return;
        }
        if (mLock)
            return;
        mLock = true;
        mMode = mode;
        synchronized (this) {
            Log.d("UsbModeSettings", "synchronized start");
            new Thread(mUsbSwitch).start();
        }
    }

    private Runnable mUsbSwitch = new Runnable() {
        public synchronized void run() {
            Log.d("UsbModeSettings", "mUsbSwitch Runnable() in*******************");
            if (mStorageManager != null) {
                if (mMode == HOST_MODE) {
                    mStorageManager.disableUsbMassStorage();
                    Log.d("UsbModeSettings", "mStorageManager.disableUsbMassStorage()*******************");
                    SystemProperties.set(USB_BOOT_PRO, HOST_MODE);
                    Write2File(file, mMode);
                } else {
                    Write2File(file, mMode);
                    Log.d("UsbModeSettings", "mStorageManager.enableUsbMassStorage()  in *******************");
                    SystemProperties.set(USB_BOOT_PRO, SLAVE_MODE);
                    mStorageManager.enableUsbMassStorage();
                    Log.d("UsbModeSettings", "mStorageManager.enableUsbMassStorage()   out*******************");
                }
            }
            Log.d("UsbModeSettings", "mUsbSwitch Runnable() out*******************");
            mLock = false;
        }
    };

    private boolean checkFile() {
        if (file == null) {
            Log.e(TAG, "file is null pointer");
            return false;
        }
        String fileName = file.getName();
        if (!file.exists()) {
            Log.e(TAG, fileName + " not exist!!!");
            return false;
        }
        if (!file.canRead()) {
            Log.e(TAG, fileName + " can't read!!!");
            return false;
        }
        if (!file.canWrite()) {
            Log.e(TAG, fileName + " can't write!!!");
            return false;
        }
        return true;
    }

    // 判断是否为3368芯片
    private boolean isRk3368() {
        if (!TextUtils.isEmpty(mSocName) && mSocName.contains("rk3368")) {
            return true;
        } else {
            return false;
        }
    }

    private boolean getFakeDataRole() {
        String fakeDataRole = SystemProperties.get(PROP_FAKE_DATA_ROLE, SLAVE_MODE);
        Log.d(TAG, "prop fakeDataRole = " + fakeDataRole);
        if (!TextUtils.isEmpty(fakeDataRole) && fakeDataRole.equals(SLAVE_MODE)) {
            return true;
        } else {
            return false;
        }
    }

    // 将mode转换为Int值，3368的mode为0,1,2
    private void setPropFakeDataRole(String mode) {
        Log.d(TAG, "setprop mode = " + mode);
        SystemProperties.set(PROP_FAKE_DATA_ROLE, mode);
    }
}
