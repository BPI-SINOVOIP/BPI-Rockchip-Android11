package android.packageinstaller.selfuninstalling.cts;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import androidx.annotation.Nullable;

public class SelfUninstallActivity extends Activity {

    private static final String ACTION_SELF_UNINSTALL =
            "android.packageinstaller.selfuninstalling.cts.action.SELF_UNINSTALL";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.self_uninstalling_activity);
        registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                Intent i = new Intent(Intent.ACTION_UNINSTALL_PACKAGE).setData(
                        Uri.fromParts("package", getPackageName(), null));
                startActivity(i);
            }
        }, new IntentFilter(ACTION_SELF_UNINSTALL));
    }
}
