package com.android.gallery3d.app;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.widget.Toast;

import com.android.gallery3d.R;

/**
 * Created by waha on 2017/12/5.
 */

public class CheckPermissionActivity extends Activity {
    public static final int REQUEST_CODE_ASK_PERMISSIONS = 124;
    private static String mJumpActivityName;
    public static final String[] REQUEST_PERMISSIONS = new String[]{
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.CAMERA,
            //Manifest.permission.ACCESS_FINE_LOCATION,
            //Manifest.permission.ACCESS_COARSE_LOCATION,
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestPermissions(REQUEST_PERMISSIONS, REQUEST_CODE_ASK_PERMISSIONS);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        switch (requestCode) {
            case REQUEST_CODE_ASK_PERMISSIONS:
                if (null == grantResults || grantResults.length < 1) {//monkey maybe enter
                    String toast_text = getResources().getString(R.string.err_permission);
                    Toast.makeText(CheckPermissionActivity.this, toast_text,
                            Toast.LENGTH_SHORT).show();
                    finish();
                    return;
                } else {
                    for (int result : grantResults) {
                        if (result != PackageManager.PERMISSION_GRANTED) {
                            // Permission Denied
                            String toast_text = getResources().getString(R.string.err_permission);
                            Toast.makeText(CheckPermissionActivity.this, toast_text,
                                    Toast.LENGTH_SHORT).show();
                            finish();
                            return;
                        }
                    }
                }
                // Permission Granted
                back2JumpActivity();
                break;
            default:
                super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }

    }

    private void back2JumpActivity() {
        Intent intent = null;
        try {
            intent = new Intent(this, Class.forName(mJumpActivityName));
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
            finish();
            return;
        }
        if (null != getIntent()) {
            intent.setAction(getIntent().getAction());
            intent.setDataAndType(getIntent().getData(), getIntent().getType());
        }
        startActivity(intent);
        finish();
    }

    public static boolean jump2PermissionActivity(Activity activity, Intent intent) {
        if (hasUnauthorizedPermission(activity)) {
            Intent newIntent = new Intent(activity, CheckPermissionActivity.class);
            if (null != intent) {
                //not action:newIntent.setAction(intent.getAction());
                newIntent.setDataAndType(intent.getData(), intent.getType());
                if (null != intent.getExtras()) {
                    newIntent.putExtras(intent.getExtras());
                }
                newIntent.setFlags(intent.getFlags());
            }
            activity.startActivity(newIntent);
            mJumpActivityName = activity.getComponentName().getClassName();
            return true;
        }
        return false;
    }

    public static boolean hasUnauthorizedPermission(Activity activity) {
        for (String permission : REQUEST_PERMISSIONS) {
            if (PackageManager.PERMISSION_GRANTED != activity.checkSelfPermission(permission)) {
                return true;
            }
        }
        return false;
    }
}
