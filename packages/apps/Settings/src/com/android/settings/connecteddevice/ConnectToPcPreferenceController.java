package com.android.settings.connecteddevice;

import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import androidx.preference.SwitchPreference;
import androidx.preference.Preference;

import com.android.settings.DisplaySettings;
import com.android.settings.core.PreferenceControllerMixin;
import com.android.settings.R;
import com.android.settingslib.core.AbstractPreferenceController;
import java.io.File; 

import java.io.File; 
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import android.util.Log;
import java.util.List; 

import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemProperties;



public class ConnectToPcPreferenceController extends AbstractPreferenceController implements
        PreferenceControllerMixin, Preference.OnPreferenceChangeListener {
    private static final String TAG = "ConnectToPcPreferenceController";
    private final String mConnectToPcKey;

    private static final String SYS_FILE = "/sys/devices/platform/fe8a0000.usb2-phy/otg_mode";
    private File mFile = null;
    private String mMode = null;

    private static final String OTG_MODE = "0";
    private static final String HOST_MODE = new String("1");
    private static final String SLAVE_MODE = new String("2");

    private static final String OTG_MODE_STR = "otg";
    private static final String HOST_MODE_STR = "host";
    private static final String SLAVE_MODE_STR = "peripheral";

    public static final String PERSIST_PRO = "persist.usb.mode";

    private Handler mHandler;
    private SwitchPreference mConnectToPc;
    
    public ConnectToPcPreferenceController(Context context, String key) {
        super(context);
        mConnectToPcKey = key;
        mFile =new File(SYS_FILE);
        mHandler = new Handler() {
            @Override
            public void handleMessage(Message msg) {
                 super.handleMessage(msg);
 
                 if (msg.what == sysFileOperation.SWITCH_FINISH) {
                     String mode = (String) msg.obj;
 
                     if(HOST_MODE.equals(mode)) {
                         mConnectToPc.setChecked(false);
                     } else if(SLAVE_MODE.equals(mode)) {
                         mConnectToPc.setChecked(true);
                     }
                     SystemProperties.set(PERSIST_PRO, mode);
                 } else {
                     Log.e(TAG, "unexpect msg:"+ msg.what);
                 }
                 mConnectToPc.setEnabled(true);
             }
        };
    }

    @Override
    public boolean isAvailable() {
        String show = SystemProperties.get("persist.usb.show");
        if(show.equals("1")){
            return mFile.exists();
        }else{
            return false;
        }
    }

    @Override
    public String getPreferenceKey() {
        return mConnectToPcKey;
    }

    @Override
    public void updateState(Preference preference) {
        mMode = ReadFromFile(mFile);
        Log.v(TAG,"updateState:"+mMode);
        if(mMode != null) {
            if(HOST_MODE.equals(mMode)) {
                ((SwitchPreference) preference).setChecked(false);
            } else if(SLAVE_MODE.equals(mMode)) {
               ((SwitchPreference) preference).setChecked(true);
            } else if (OTG_MODE.equals(mMode)) {
                ((SwitchPreference) preference).setChecked(true);
                Write2File(mFile, SLAVE_MODE);
            }
        }else{
             ((SwitchPreference) preference).setChecked(false);
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        mConnectToPc = (SwitchPreference) preference;
        boolean mode = (Boolean) newValue;
        Log.v(TAG,"onPreferenceChange:"+mode);
        if(mode){
            mMode = SLAVE_MODE;
        }else{
            mMode = HOST_MODE;
        }

        mConnectToPc.setEnabled(false);
        // Start separate thread to do the actual loading.
        Thread thread = new Thread(new sysFileOperation(mFile, mMode, mHandler));
        thread.start();
        
        return true;
    }

  
    public String ReadFromFile(File file) {
        if((file != null) && file.exists()) {
            try {
                FileInputStream fin= new FileInputStream(file);
                BufferedReader reader= new BufferedReader(new InputStreamReader(fin));
                String config = reader.readLine();
                fin.close();
                //Log.d(TAG, "Usb mode: " + config);
                if (HOST_MODE_STR.equals(config)) {
                    config = HOST_MODE;
                } else if (SLAVE_MODE_STR.equals(config)) {
                    config = SLAVE_MODE;
                } else if (OTG_MODE_STR.equals(config)) {
                    config = OTG_MODE;
                }
                return config;
            } catch(IOException e) {
                e.printStackTrace();
            }
        }
               
        return null;
      }
      
    public void Write2File(File file,String mode) {
        //Log.d(TAG,"Write2File,write mode = "+mode);
        if((file == null) || (!file.exists()) || (mode == null)) return ;
  
        try {
            FileOutputStream fout = new FileOutputStream(file);
            PrintWriter pWriter = new PrintWriter(fout);
            pWriter.println(mode);
            pWriter.flush();
            pWriter.close();
            fout.close();
        } catch(IOException re) {
            Log.d(TAG,"write error:"+re);
            return;
        }
    }

    private class sysFileOperation implements Runnable {
  
        private File mFile;
        private String mMode;
        private Handler mHandler;
        public static final int SWITCH_FINISH = 0;
 
        public sysFileOperation(File file, String mode, Handler handler) {
            mFile = file;
            mHandler = handler;
            mMode = mode;
        }
        public void run() {
 
            Write2File(mFile, mMode);
 
            // Tell the UI thread that we are finished.
            Message msg = mHandler.obtainMessage(SWITCH_FINISH, null);
            String real_mode = ReadFromFile(mFile);
            msg.obj = real_mode;
            mHandler.sendMessage(msg);
         }
     }
}
