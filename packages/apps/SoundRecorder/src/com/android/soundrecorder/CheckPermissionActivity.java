package com.android.soundrecorder;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.widget.Toast;

/**
 * Created by waha on 2017/12/5.
 */

public class CheckPermissionActivity extends Activity {
    private final int REQUEST_CODE_ASK_PERMISSIONS = 124;
    private static final String[] REQUEST_PERMISSIONS = new String[]{
            Manifest.permission.MANAGE_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE,
//            Manifest.permission.READ_PHONE_STATE,
            Manifest.permission.RECORD_AUDIO
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
                back2MainActivity();
                break;
            default:
                super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }

    }

    private void back2MainActivity() {
        Intent intent = new Intent(this, SoundRecorder.class);
        if (null != getIntent() && null != getIntent().getAction()) {
            intent.setAction(getIntent().getAction());
        }
        startActivity(intent);
        finish();
    }

    public static boolean jump2PermissionActivity(Activity activity, String action) {
        for (String permission : REQUEST_PERMISSIONS) {
            if (PackageManager.PERMISSION_GRANTED != activity.checkSelfPermission(permission)) {
                Intent intent = new Intent(activity, CheckPermissionActivity.class);
                if (!TextUtils.isEmpty(action)) {
                    intent.setAction(action);
                }
                activity.startActivity(intent);
                return true;
            }
        }
        return false;
    }
}
