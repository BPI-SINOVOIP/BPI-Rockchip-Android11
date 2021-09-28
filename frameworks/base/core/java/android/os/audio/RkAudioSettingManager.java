package android.os.audio;

import android.annotation.NonNull;
import android.os.IBinder;
import android.os.ServiceManager;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;

import com.android.internal.util.Preconditions;

public class RkAudioSettingManager {
    private IRkAudioSettingService mService;
    public static final String TAG = "RkAudioSetting";

    public static final int CODE_FAILED = -1;

    /**
    * @throws RemoteException
    * @hide
    */
    public RkAudioSettingManager() {
        Log.i(TAG, "RkAudioSettingManager: ");
        IBinder binder = ServiceManager.getService("rockchip_audio_setting");
        if (binder == null) {
            Log.i(TAG, "Unable to connect to rockchip audio setting service! - is it running yet?");
            return;
        }
        mService = IRkAudioSettingService.Stub.asInterface(binder);
    }
// apk interface
/* query  0: decode, 1: hdmi bitstream, 2: spdif passthrough
  * return 1 : support, 0: unsupport
  */
    public int getSelect(int device) {
        try {
            if (mService != null) {
                return mService.getSelect(device);
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }
        return CODE_FAILED;
    }

/*
 * 0->decode
 * 1->hdmi bitstream
 * 2->spdif passthrough
 */
    public void setSelect(int device) {
        try {
            if (mService != null) {
                mService.setSelect(device);
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

/*
 * param : device  : 0 ->decode, 1->hdmi bitstream
 *             mode : mode
 * return : 1 : manual, multi_pcm, 0: decode_pcm, auto
 */
    public int getMode(int device) {
        try {
            if (mService != null) {
                return mService.getMode(device);
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }
        return CODE_FAILED;
    }

/*
 * param : device : 0 ->decode, 1->hdmi bitstream
 *             mode  : 1 : manual, multi_pcm, 0: decode_pcm, auto
 * return void
 */
    public void setMode(int device, int mode) {
        try {
            if (mService != null) {
                mService.setMode(device, mode);
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

/*
 * param : device  : 0 ->decode, 1->hdmi bitstream, 2->spdif passthrough
 *             format :   codec format
 * return : 1 : support, 0: unsupport
 */
    public int getFormat(int device, @NonNull String format) {
        format = Preconditions.checkNotNull(format, "format cannot be null");
        try {
            if (mService != null) {
                return mService.getFormat(device, format);
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }
        return CODE_FAILED;
    }

/*
 * param : device : 0 ->decode, 1->hdmi bitstream, 2->spdif passthrough
 *             close: 0-> add, 1-> delect
 *             format : codec format
 * return void
 */
    public void setFormat(int device, int close, @NonNull String format) {
        format = Preconditions.checkNotNull(format, "format cannot be null");
        try {
            if (mService != null) {
                mService.setFormat(device, close, format);
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }
}
