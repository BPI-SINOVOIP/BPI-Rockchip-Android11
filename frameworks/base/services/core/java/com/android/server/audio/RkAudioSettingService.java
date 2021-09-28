package com.android.server.audio;

import android.content.Context;
import android.os.audio.IRkAudioSettingService;
import android.os.RemoteException;
import android.util.Log;
import android.media.IAudioService;
import android.media.AudioSystem;
import android.media.AudioManager;
import android.os.ServiceManager;

// import rockchip.hardware.rkaudiosetting.V1_0.IRkAudioSetting;

/**
 * @hide
 */
public class RkAudioSettingService extends IRkAudioSettingService.Stub {
    public static final String TAG = "RkAudioSettingService";

    private Context mContext;

    private RkAudioSetting mAudioSetting;
    public static String DEVICE_HDMI = "hdmi";
    public static String DEVICE_SPDIF = "spdif";

    public int DEVICE_DECODE = 0;
    public int DEVICE_HDMI_BITSTREAM = 1;
    public int DEVICE_SPDIF_PASSTHROUGH = 2;

    private int AUTO_MODE = 0;

    private int STATE_CONNECTED = 1;
    private int STATE_DISCONNECTED = 0;

    public RkAudioSettingService(Context context) throws RemoteException {
        mContext = context;
        mAudioSetting = new RkAudioSetting();
        parseSetting();
    }

    public void parseSetting() throws RemoteException {
        int hdmi_select = mAudioSetting.getSelect(DEVICE_HDMI_BITSTREAM);
        int spdif_select = mAudioSetting.getSelect(DEVICE_SPDIF_PASSTHROUGH);
        if (hdmi_select == STATE_CONNECTED) {
            setDeviceConnect(AudioSystem.DEVICE_OUT_AUX_DIGITAL, STATE_CONNECTED);
        }

        if (spdif_select == STATE_CONNECTED) {
            setDeviceConnect(AudioSystem.DEVICE_OUT_SPDIF, STATE_CONNECTED);
        }
    }

    public void setDeviceConnect(int device, int state) throws RemoteException {
       String deviceAddress = "RK_BITSTREAM_DEVICE_ADDRESS";
       String deviceName = "RK_BITSTREAM_DEVICE_NAME";

       AudioSystem.setDeviceConnectionState(device, state, deviceAddress, deviceName,
                      AudioSystem.AUDIO_FORMAT_IEC61937);
    }

    @Override
    public int getSelect(int device) throws RemoteException {
        return mAudioSetting.getSelect(device);
    }

    @Override
    public void setSelect(int device) throws RemoteException {
        if (device == DEVICE_HDMI_BITSTREAM) {
            setDeviceConnect(AudioSystem.DEVICE_OUT_AUX_DIGITAL, STATE_CONNECTED);
        }

        if (device == DEVICE_SPDIF_PASSTHROUGH) {
            setDeviceConnect(AudioSystem.DEVICE_OUT_SPDIF, STATE_CONNECTED);
        }
        mAudioSetting.setSelect(device);
    }

    @Override
    public int getMode(int device) throws RemoteException {
        return mAudioSetting.getMode(device);
    }

    @Override
    public void setMode(int device, int mode) throws RemoteException {
        if ((device == DEVICE_HDMI_BITSTREAM) && (mode == AUTO_MODE)) {
            mAudioSetting.updataFormatForEdid();
        }
        mAudioSetting.setMode(device, mode);
    }

    @Override
    public int getFormat(int device, String format) throws RemoteException {
        return mAudioSetting.getFormat(device, format);
    }

    @Override
    public void setFormat(int device, int close, String format) throws RemoteException {
        mAudioSetting.setFormat(device, close, format);
    }

}
