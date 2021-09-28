package android.security.cts.CVE_2021_0327.workprofilesetup;

import android.app.Activity;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Bundle;

import android.security.cts.CVE_2021_0327.OtherUserActivity;

public class ProvisionedActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getPackageManager().setComponentEnabledSetting(
                new ComponentName(
                        this,
                        OtherUserActivity.class
                ),
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                PackageManager.DONT_KILL_APP
        );

        DevicePolicyManager manager = getSystemService(DevicePolicyManager.class);
        //IntentFilter intentFilter = new IntentFilter("com.example.clearedshell.OTHER_USER_ACTIVITY");
        IntentFilter intentFilter = new IntentFilter("android.security.cts.CVE_2021_0327.OTHER_USER_ACTIVITY");
        //IntentFilter intentFilter = new IntentFilter("android.security.cts.CVE_2021_0327$OTHER_USER_ACTIVITY");
        intentFilter.addCategory(Intent.CATEGORY_DEFAULT);
        ComponentName admin = new ComponentName(this, AdminReceiver.class);
        manager.addCrossProfileIntentFilter(
                admin,
                intentFilter,
                DevicePolicyManager.FLAG_MANAGED_CAN_ACCESS_PARENT
        );
        manager.setProfileEnabled(admin);

        setResult(RESULT_OK);
        finish();
    }
}
