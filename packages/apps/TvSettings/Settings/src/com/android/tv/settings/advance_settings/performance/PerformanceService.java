package com.android.tv.settings.advance_settings.performance;

import java.util.Timer;
import java.util.TimerTask;
import android.R.integer;
import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;
import android.net.TrafficStats;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import java.net.NetworkInterface;
import java.util.Collections;
import java.io.*;
import com.android.tv.settings.advance_settings.performance.CpuInfoReader;
import android.view.Gravity;
import com.android.tv.settings.R;
import java.util.List;
import android.text.format.Formatter;

public class PerformanceService extends Service {
    public static final String TAG = "PerformanceService";

    private static final int VIEW_WIDTH = 300;
    private static final int VIEW_HEIGHT = 300;
    private WindowManager mWm = null;
    private WindowManager.LayoutParams mWmParams = null;

    private View mView = null;
    private TextView mCpuUsage = null;
    private TextView mCpuCurFrql = null;
    private TextView mCpuTemp = null;
    private TextView mRamTotal = null;
    private TextView mRamFree = null;
    private TextView mNetSpeed = null;
    private TextView mWifiMac = null;
    private TextView mEthnetMac = null;
    private Handler mMainHandler = new Handler();


    @Override
    public IBinder onBind(Intent intent) {
            return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
            if (intent == null) {
                    Log.e(TAG, "onStartCommand(): intent = null");
                    return -1;
            }
            createFloatingView(LayoutInflater.from(this));
            return super.onStartCommand(intent, flags, startId);
    }

    private void createFloatingView(LayoutInflater inflater) {
            mWm = (WindowManager) getApplicationContext()
                            .getSystemService("window");
            View view = inflater.inflate(R.layout.performance_view, null);
            mView = view.findViewById(R.id.performance_view);
            initWmParams();
            //mView.setBackgroundColor(Color.TRANSPARENT);
            mWm.addView(mView, mWmParams);
            setupViews();
    }

    private void initWmParams() {
            if (mWmParams == null) {
                    mWmParams = new WindowManager.LayoutParams();
                    mWmParams.type = WindowManager.LayoutParams.TYPE_SYSTEM_ALERT;
                    // ;
                    //mWmParams.format = PixelFormat.TRANSLUCENT;
                    mWmParams.alpha = 0.5f;
                    mWmParams.flags |= 8;
                    mWmParams.flags |= WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED;
                    mWmParams.gravity = Gravity.LEFT | Gravity.TOP;
                    mWmParams.x = 20;
                    mWmParams.y = 60;

                    mWmParams.width = VIEW_WIDTH;
                    mWmParams.height = VIEW_HEIGHT;
            }
    }

    private void setupViews() {
         //CPU
            mCpuCurFrql = (TextView) mView.findViewById(R.id.cpu_model);
            mCpuUsage = (TextView) mView.findViewById(R.id.cpu_usage);
            mCpuTemp = (TextView) mView.findViewById(R.id.cpu_temp);
            mMainHandler.postDelayed(mUpdateCpuFrqAction, 50);
            mMainHandler.postDelayed(mUpdateCpuAction, 50);
         //RAM
            mRamTotal = (TextView) mView.findViewById(R.id.ram_total);
            mRamTotal.setText(getString(R.string.ram_total) + getRamTotal());
            mRamFree = (TextView) mView.findViewById(R.id.ram_usage);
            mMainHandler.postDelayed(mUpdateRamAction, 100);
         //Net
            mNetSpeed = (TextView) mView.findViewById(R.id.network_speed);
            //getSpeed
            lastTotalRxBytes = getTotalRxBytes();
            lastTimeStamp = System.currentTimeMillis();
            mMainHandler.postDelayed(mUpdateNetAction, 1000);

            mWifiMac = (TextView) mView.findViewById(R.id.network_mac);
            mWifiMac.setText(getString(R.string.network_mac) + getWifiMac());
            mEthnetMac = (TextView) mView.findViewById(R.id.network_mac2);
            mEthnetMac.setText(getString(R.string.network_mac2) + getEthnetMac());
    }

//cpu
    private long[] mCpuInfo = new long[2];
    private Runnable mUpdateCpuAction = new Runnable(){
                public void run() {
                        ShowCpuInfo();
                        mMainHandler.postDelayed(this, 1000);
                };
    };
    private Runnable mUpdateCpuFrqAction = new Runnable(){
                public void run() {
                        //cur frq
                        mCpuCurFrql.setText(getString(R.string.cpu_model)
                                            + CpuInfoReader.getCpuCurrentFreq()/1000+" MHz" );
                        mMainHandler.postDelayed(this, 100);
                };
    };
   private void ShowCpuInfo() {
        //usage
        long[] cpuInfo = CpuInfoReader.getCpuTime();
        if(cpuInfo[0]==0||cpuInfo[1]==0){
                return;
        }
        if(mCpuInfo[0]==0||mCpuInfo[1]==0){
                mCpuInfo = cpuInfo;
                return;
        }
        long totalTime = cpuInfo[0]-mCpuInfo[0];
        long iddleTime = cpuInfo[1]-mCpuInfo[1];
        int percent = (int)((totalTime-iddleTime)*1.00f/totalTime*100);
        if(percent==0) percent = 1;
        mCpuInfo = cpuInfo;
        mCpuUsage.setText(getString(R.string.cpu_usage) + percent+"%" );

        //temp
        int temp = CpuInfoReader.getCpuCurrentTmp()/1000;
        mCpuTemp.setText(getString(R.string.cpu_temp) + temp+"℃" );
   }
//cpu end

//ram
    private Runnable mUpdateRamAction = new Runnable(){
                public void run() {
                        showRamFree();
                        mMainHandler.postDelayed(this, 2000);
                };
    };
   private void showRamFree() {
        ActivityManager am = (ActivityManager) getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
        am.getMemoryInfo(mi);
        mRamFree.setText(getString(R.string.ram_usage) + Formatter.formatFileSize(getApplicationContext(),mi.availMem));
   }
   private String getRamTotal() {
        String str1 = "/proc/meminfo";// 系统内存信息文件
        String str2;
        String[] arrayOfString;
        long initial_memory = 0;
        try {
             FileReader localFileReader = new FileReader(str1);
             BufferedReader localBufferedReader = new BufferedReader(localFileReader,8192);
             str2 = localBufferedReader.readLine();// 读取meminfo第一行，系统总内存大
             arrayOfString = str2.split("\\s+");
             Log.d(TAG,"getRamTotal:"+str2);
             initial_memory = Integer.valueOf(arrayOfString[1]).intValue() * 1024;// 获得系统总内存，单位是KB，乘以1024转换为Byte
             localBufferedReader.close();
        } catch (IOException e) {
        }
        return Formatter.formatFileSize(getApplicationContext(), initial_memory); // Byte转换为KB或者MB，内存大小规格化
   }
//ram end

//network
    private String getWifiMac() {
      try {
        List<NetworkInterface> all = Collections.list(NetworkInterface.getNetworkInterfaces());
        for (NetworkInterface nif : all) {
            if (!nif.getName().equalsIgnoreCase("wlan0")) continue;

            byte[] macBytes = nif.getHardwareAddress();
            if (macBytes == null) {
                return "";
            }

            StringBuilder res1 = new StringBuilder();
            for (byte b : macBytes) {
                res1.append(String.format("%02X:", b));
            }

            if (res1.length() > 0) {
                res1.deleteCharAt(res1.length() - 1);
            }
            return res1.toString();
        }
      } catch (Exception e) {
        e.printStackTrace();
      }
      return "02:00:00:00:00:00";

    }

    private String getEthnetMac() {
      try {
        List<NetworkInterface> all = Collections.list(NetworkInterface.getNetworkInterfaces());
        for (NetworkInterface nif : all) {
            if (!nif.getName().equalsIgnoreCase("eth0")) continue;

            byte[] macBytes = nif.getHardwareAddress();
            if (macBytes == null) {
                return "";
            }

            StringBuilder res1 = new StringBuilder();
            for (byte b : macBytes) {
                res1.append(String.format("%02X:", b));
            }

            if (res1.length() > 0) {
                res1.deleteCharAt(res1.length() - 1);
            }
            return res1.toString();
        }
      } catch (Exception e) {
        e.printStackTrace();
      }
      return "02:00:00:00:00:00";
    }

    private long lastTotalRxBytes = 0;
    private long lastTimeStamp = 0;


    private Runnable mUpdateNetAction = new Runnable(){
                public void run() {
                        showNetSpeed();
                        mMainHandler.postDelayed(this, 1000);
                };
    };

    private long getTotalRxBytes() {
        return TrafficStats.getUidRxBytes(getApplicationContext().getApplicationInfo().uid) == TrafficStats.UNSUPPORTED ? 0 :(TrafficStats.getTotalRxBytes()/1024);//转为KB
    }

    private void showNetSpeed() {
        long nowTotalRxBytes = getTotalRxBytes();
        long nowTimeStamp = System.currentTimeMillis();
        long speed = ((nowTotalRxBytes - lastTotalRxBytes) * 1000 / (nowTimeStamp - lastTimeStamp));//毫秒转换
        long speed2 = ((nowTotalRxBytes - lastTotalRxBytes) * 1000 % (nowTimeStamp - lastTimeStamp));//毫秒转换

        lastTimeStamp = nowTimeStamp;
        lastTotalRxBytes = nowTotalRxBytes;

        String network_speed = String.valueOf(speed) + "." + String.valueOf(speed2) + " kb/s";
        mNetSpeed.setText(getString(R.string.network_speed) + network_speed);//更新界面
    }
//network end

    @Override
    public void onDestroy() {
            Log.e(TAG, "onDestroy");
            super.onDestroy();
            if(mMainHandler!=null){
               mMainHandler.removeCallbacks(mUpdateCpuFrqAction);
               mMainHandler.removeCallbacks(mUpdateCpuAction);
               mMainHandler.removeCallbacks(mUpdateRamAction);
               mMainHandler.removeCallbacks(mUpdateNetAction);
               mMainHandler = null;
            }
            mWm.removeView(mView);
            //stopForeground(true);
    }

    private String getRunningActivity() {
            ActivityManager manager = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
            String name = manager.getRunningTasks(2).get(0).topActivity.getClassName();
            Log.e(TAG, "RUNNING: "+name);
            return name;
    }

    private Boolean isTesting(){
            String name = getRunningActivity();
            if (name.equals("")){
                return true;
            }else
                    return false;
    }
}
