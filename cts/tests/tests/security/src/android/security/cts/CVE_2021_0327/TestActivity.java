package android.security.cts.CVE_2021_0327;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

public class TestActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d("CVE_2021_0327","TestActivity.onCreate()");
        synchronized(CVE_2021_0327.class){
          CVE_2021_0327.testActivityCreated=true;
          CVE_2021_0327.class.notifyAll();
        }
    }
}
