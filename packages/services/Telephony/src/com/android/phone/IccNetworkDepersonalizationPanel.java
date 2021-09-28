/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.phone;

import android.content.Context;
import android.os.AsyncResult;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.PersistableBundle;
import android.telephony.CarrierConfigManager;
import android.telephony.TelephonyManager;
import android.text.Editable;
import android.text.Spannable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.method.DialerKeyListener;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.android.internal.telephony.Phone;
import com.android.internal.telephony.uicc.IccCardApplicationStatus.PersoSubState;

/**
 * "SIM network unlock" PIN entry screen.
 *
 * @see PhoneGlobals.EVENT_SIM_NETWORK_LOCKED
 *
 * TODO: This UI should be part of the lock screen, not the
 * phone app (see bug 1804111).
 */
public class IccNetworkDepersonalizationPanel extends IccPanel {

    /**
     * Tracks whether there is an instance of the network depersonalization dialog showing or not.
     * Ensures only a single instance of the dialog is visible.
     */
    private static boolean [] sShowingDialog =
            new boolean[TelephonyManager.getDefault().getSupportedModemCount()];

    //debug constants
    private static final boolean DBG = false;

    //events
    private static final int EVENT_ICC_NTWRK_DEPERSONALIZATION_RESULT = 100;

    private Phone mPhone;
    private int mPersoSubtype;
    private static IccNetworkDepersonalizationPanel [] sNdpPanel =
            new IccNetworkDepersonalizationPanel[
                    TelephonyManager.getDefault().getSupportedModemCount()];

    //UI elements
    private EditText     mPinEntry;
    private LinearLayout mEntryPanel;
    private LinearLayout mStatusPanel;
    private TextView     mPersoSubtypeText;
    private PersoSubState mPersoSubState;
    private TextView     mStatusText;

    private Button       mUnlockButton;
    private Button       mDismissButton;

    enum statusType {
        ENTRY,
        IN_PROGRESS,
        ERROR,
        SUCCESS
    }

    /**
     * Shows the network depersonalization dialog, but only if it is not already visible.
     */
    public static void showDialog(Phone phone, int subType) {
        int phoneId = phone == null ? 0: phone.getPhoneId();
        if (phoneId >= sShowingDialog.length) {
            Log.e(TAG, "[IccNetworkDepersonalizationPanel] showDialog; invalid phoneId" + phoneId);
            return;
        }
        if (sShowingDialog[phoneId]) {
            Log.i(TAG, "[IccNetworkDepersonalizationPanel] - showDialog; skipped already shown.");
            return;
        }
        Log.i(TAG, "[IccNetworkDepersonalizationPanel] - showDialog; showing dialog.");
        sShowingDialog[phoneId] = true;
        sNdpPanel[phoneId] = new IccNetworkDepersonalizationPanel(PhoneGlobals.getInstance(),
                phone, subType);
        sNdpPanel[phoneId].show();
    }

    public static void dialogDismiss(int phoneId) {
        if (phoneId >= sShowingDialog.length) {
            Log.e(TAG, "[IccNetworkDepersonalizationPanel] - dismiss; invalid phoneId " + phoneId);
            return;
        }
        if (sNdpPanel[phoneId] != null && sShowingDialog[phoneId]) {
            sNdpPanel[phoneId].dismiss();
        }
    }

    //private textwatcher to control text entry.
    private TextWatcher mPinEntryWatcher = new TextWatcher() {
        public void beforeTextChanged(CharSequence buffer, int start, int olen, int nlen) {
        }

        public void onTextChanged(CharSequence buffer, int start, int olen, int nlen) {
        }

        public void afterTextChanged(Editable buffer) {
            if (SpecialCharSequenceMgr.handleChars(
                    getContext(), buffer.toString())) {
                mPinEntry.getText().clear();
            }
        }
    };

    //handler for unlock function results
    private Handler mHandler = new Handler() {
        public void handleMessage(Message msg) {
            if (msg.what == EVENT_ICC_NTWRK_DEPERSONALIZATION_RESULT) {
                AsyncResult res = (AsyncResult) msg.obj;
                if (res.exception != null) {
                    if (DBG) log("network depersonalization request failure.");
                    displayStatus(statusType.ERROR.name());
                    postDelayed(new Runnable() {
                        public void run() {
                            hideAlert();
                            mPinEntry.getText().clear();
                            mPinEntry.requestFocus();
                        }
                    }, 3000);
                } else {
                    if (DBG) log("network depersonalization success.");
                    displayStatus(statusType.SUCCESS.name());
                    postDelayed(new Runnable() {
                        public void run() {
                            dismiss();
                        }
                    }, 3000);
                }
            }
        }
    };


    //constructor
    public IccNetworkDepersonalizationPanel(Context context) {
        super(context);
        mPhone = PhoneGlobals.getPhone();
        mPersoSubtype = PersoSubState.PERSOSUBSTATE_SIM_NETWORK.ordinal();
    }

    //constructor
    public IccNetworkDepersonalizationPanel(Context context, Phone phone,
            int subtype) {
        super(context);
        mPhone = phone == null ? PhoneGlobals.getPhone() : phone;
        mPersoSubtype = subtype;
    }

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setContentView(R.layout.sim_ndp);

        // PIN entry text field
        mPinEntry = (EditText) findViewById(R.id.pin_entry);
        mPinEntry.setKeyListener(DialerKeyListener.getInstance());
        mPinEntry.setOnClickListener(mUnlockListener);

        // Attach the textwatcher
        CharSequence text = mPinEntry.getText();
        Spannable span = (Spannable) text;
        span.setSpan(mPinEntryWatcher, 0, text.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);

        mEntryPanel = (LinearLayout) findViewById(R.id.entry_panel);
        mPersoSubtypeText = (TextView) findViewById(R.id.perso_subtype_text);
        displayStatus(statusType.ENTRY.name());

        mUnlockButton = (Button) findViewById(R.id.ndp_unlock);
        mUnlockButton.setOnClickListener(mUnlockListener);

        // The "Dismiss" button is present in some (but not all) products,
        // based on the "KEY_SIM_NETWORK_UNLOCK_ALLOW_DISMISS_BOOL" variable.
        mDismissButton = (Button) findViewById(R.id.ndp_dismiss);
        PersistableBundle carrierConfig = PhoneGlobals.getInstance().getCarrierConfig();
        if (carrierConfig.getBoolean(
                CarrierConfigManager.KEY_SIM_NETWORK_UNLOCK_ALLOW_DISMISS_BOOL)) {
            if (DBG) log("Enabling 'Dismiss' button...");
            mDismissButton.setVisibility(View.VISIBLE);
            mDismissButton.setOnClickListener(mDismissListener);
        } else {
            if (DBG) log("Removing 'Dismiss' button...");
            mDismissButton.setVisibility(View.GONE);
        }

        //status panel is used since we're having problems with the alert dialog.
        mStatusPanel = (LinearLayout) findViewById(R.id.status_panel);
        mStatusText = (TextView) findViewById(R.id.status_text);
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    public void onStop() {
        super.onStop();
        Log.i(TAG, "[IccNetworkDepersonalizationPanel] - showDialog; hiding dialog.");
        int phoneId = mPhone == null ? 0 : mPhone.getPhoneId();
        if (phoneId >= sShowingDialog.length) {
            Log.e(TAG, "[IccNetworkDepersonalizationPanel] - onStop; invalid phoneId " + phoneId);
            return;
        }
        sShowingDialog[phoneId] = false;
    }

    //Mirrors IccPinUnlockPanel.onKeyDown().
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    View.OnClickListener mUnlockListener = new View.OnClickListener() {
        public void onClick(View v) {
            String pin = mPinEntry.getText().toString();

            if (TextUtils.isEmpty(pin)) {
                return;
            }

            log("Requesting De-Personalization for subtype " + mPersoSubtype);

            try {
                mPhone.getIccCard().supplySimDepersonalization(mPersoSubState,pin,
                        Message.obtain(mHandler, EVENT_ICC_NTWRK_DEPERSONALIZATION_RESULT));
            } catch (NullPointerException ex) {
                log("NullPointerException @supplySimDepersonalization" + ex);
            }
            displayStatus(statusType.IN_PROGRESS.name());
        }
    };

    private void displayStatus(String type) {
        int label = 0;

        mPersoSubState = PersoSubState.values()[mPersoSubtype];
        log("displayStatus mPersoSubState: " +mPersoSubState.name() +"type: " +type);

        label = getContext().getResources().getIdentifier(mPersoSubState.name()
                + "_" + type, "string", "android");

        if (label == 0) {
            log ("Unable to get the PersoSubType string");
            return;
        }

        if(!PersoSubState.isPersoLocked(mPersoSubState)) {
            log ("Unsupported Perso Subtype :" + mPersoSubState.name());
            return;
        }

        if (type == statusType.ENTRY.name()) {
            String displayText = getContext().getString(label);
            mPersoSubtypeText.setText(displayText);
        } else {
            mStatusText.setText(label);
            mEntryPanel.setVisibility(View.GONE);
            mStatusPanel.setVisibility(View.VISIBLE);
        }
    }

    private void hideAlert() {
        mEntryPanel.setVisibility(View.VISIBLE);
        mStatusPanel.setVisibility(View.GONE);
    }

    View.OnClickListener mDismissListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (DBG) log("mDismissListener: skipping depersonalization...");
            dismiss();
        }
    };

    private void log(String msg) {
        Log.d(TAG, "[IccNetworkDepersonalizationPanel] " + msg);
    }
}
