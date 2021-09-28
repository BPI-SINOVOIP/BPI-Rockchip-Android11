package com.google.android.tv.setup.customizationsample;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.LinkedHashMap;

public abstract class BaseActivity extends Activity {

    private static final String TAG = "SetupCustomization";

    private ImageView iv0;
    private TextView tv0;
    private TextView tv1;
    private TextView tv2;

    private LinkedHashMap<Integer,Runnable> keyRunnables;
    private LinkedHashMap<Integer,String> keyDescriptions;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(null);
        setContentView(R.layout.activity_base_layout);

        keyRunnables = new LinkedHashMap<>();
        keyDescriptions = new LinkedHashMap<>();

        iv0 = findViewById(R.id.image0);
        tv0 = findViewById(R.id.tv0);
        tv1 = findViewById(R.id.tv1);
        tv2 = findViewById(R.id.tv2);

        tv0.setText(getTitle());

        populateIncomingExtrasView();

        // tv1 left open for subclasses - see showText1()
        // tv2 left open for subclasses - see showText2()
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (isFinishing()) {
            return super.onKeyUp(keyCode, event);
        }
        for (int candidateKeyCode : keyRunnables.keySet()) {
            if (candidateKeyCode == keyCode) {
                return reactToKey(candidateKeyCode);
            }
        }
        return super.onKeyUp(keyCode, event);
    }

    protected void log(String message) {
        Log.d(TAG, message);
    }

    protected void registerKeyHandlerResultCancelledBack() {
        Intent intent = new Intent().putExtra(Constants.USER_INITIATED, true);
        registerKeyHandler(KeyEvent.KEYCODE_BACK, null, Activity.RESULT_CANCELED, intent);
    }

    protected void registerKeyHandlerResultCancelledCrashDpadLeft() {
        registerKeyHandler(KeyEvent.KEYCODE_DPAD_LEFT, "as if a crash occurred", Activity.RESULT_CANCELED, null);
    }

    protected void registerKeyHandlerResultOk(int keyCode, String addendum, String[] booleanExtrasToSend) {
        final Intent intent = new Intent();
        for (String booleanExtraToSend : booleanExtrasToSend) {
            intent.putExtra(booleanExtraToSend, true);
        }
        registerKeyHandlerResultOk(keyCode, addendum, intent);
    }

    protected void registerKeyHandlerResultOk(int keyCode, String addendum) {
        final Intent intent = new Intent();
        registerKeyHandlerResultOk(keyCode, addendum, intent);
    }

    protected void registerKeyHandlerResultOkDpadCenterBoring() {
        final Intent intent = new Intent();
        registerKeyHandlerResultOk(KeyEvent.KEYCODE_DPAD_CENTER, "complete without doing anything interesting", intent);
    }

    protected void registerKeyHandlerResultOk(int keyCode, String addendum, final Intent intent) {
        registerKeyHandler(keyCode, addendum, Activity.RESULT_OK, intent);
    }

    protected void showDrawable(int drawableResourceId) {
        iv0.setVisibility(View.VISIBLE);
        iv0.setImageResource(drawableResourceId);
    }

    protected void showText1(String text) {
        tv1.setText(text);
    }

    protected void showText2(String text) {
        tv2.setText(text);
    }

    private void registerKeyHandler(int keyCode, String addendum, final int resultCode, final Intent intent) {
        StringBuilder message = new StringBuilder();
        message.append("finish() and send " + resultCodeAsString(resultCode));
        if (addendum != null) {
            message.append(" - ").append(addendum);
        }
        if (intent != null) {
            Bundle bundle = intent.getExtras();
            if (bundle != null) {
                message.append(" (");
                for (String key : bundle.keySet()) {
                    Object value = bundle.get(key);
                    message.append(key).append('=').append(value).append(',');
                }
                message.append(')');
            }
        }
        keyRunnables.put(keyCode, new Runnable() {
            @Override
            public void run() {
                setResult(resultCode, intent);
                finish();
            }
        });
        keyDescriptions.put(keyCode, message.toString());
        populateKeyHandlerViews();
    }

    private void populateKeyHandlerViews() {
        LinearLayout llLeft = findViewById(R.id.ll_keyHandler1);
        llLeft.removeAllViews();
        LinearLayout llRight = findViewById(R.id.ll_keyHandler2);
        llRight.removeAllViews();
        for (Integer code : keyRunnables.keySet()) {
            String keyStr = KeyEvent.keyCodeToString(code);
            if (keyStr.startsWith("KEYCODE_")) {
                keyStr = keyStr.substring("KEYCODE_".length());
            }
            if (keyStr.startsWith("DPAD_")) {
                keyStr = keyStr.substring("DPAD_".length());
            }
            keyStr = keyStr + ":";
            TextView tvKey = (TextView) getLayoutInflater().inflate(R.layout.key_item, llLeft, false);
            tvKey.setText(keyStr);
            tvKey.setTextColor(0xFF00FFFF);
            llLeft.addView(tvKey);

            TextView tvVal = (TextView) getLayoutInflater().inflate(R.layout.key_item, llRight, false);
            tvVal.setText(keyDescriptions.get(code));
            llRight.addView(tvVal);
        }
    }

    private void populateIncomingExtrasView() {
        LinearLayout llLeft = findViewById(R.id.ll_incomingExtra1);
        llLeft.removeAllViews();
        LinearLayout llRight = findViewById(R.id.ll_incomingExtra2);
        llRight.removeAllViews();
        Bundle bundle = getIntent().getExtras();
        if (bundle != null) {
            for (String key : bundle.keySet()) {
                Object value = bundle.get(key);
                TextView tvKey = (TextView) getLayoutInflater().inflate(R.layout.key_item, llLeft, false);
                tvKey.setText(key);
                tvKey.setTextColor(0xFF00FF00);
                llLeft.addView(tvKey);
                TextView tvVal = (TextView) getLayoutInflater().inflate(R.layout.key_item, llRight, false);
                tvVal.setText(value.toString());
                llRight.addView(tvVal);
            }
        }
    }

    private boolean reactToKey(int candidateKeyCode) {
        try {
            keyRunnables.get(candidateKeyCode).run();
            log(keyDescriptions.get(candidateKeyCode));
        } catch (Exception e) {
            e.printStackTrace();
        }
        return true;
    }

    private static String resultCodeAsString(int resultCode) {
        if (resultCode == Activity.RESULT_OK) {
            return "RESULT_OK";
        }
        if (resultCode == Activity.RESULT_CANCELED) {
            return "RESULT_CANCELLED";
        }
        return String.valueOf(resultCode);
    }
}
