/*
 * Copyright (C) 2006 The Android Open Source Project
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

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.RingtoneManager;
import android.net.Uri;
import android.os.Handler;
import android.os.Message;
import android.os.PersistableBundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.VideoProfile;
import android.telephony.CarrierConfigManager;
import android.telephony.PhoneNumberUtils;
import android.telephony.SubscriptionManager;
import android.text.TextUtils;
import android.util.Log;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.Toast;

import com.android.internal.telephony.Call;
import com.android.internal.telephony.CallStateException;
import com.android.internal.telephony.Connection;
import com.android.internal.telephony.IccCard;
import com.android.internal.telephony.MmiCode;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneConstants;
import com.android.internal.telephony.PhoneFactory;
import com.android.internal.telephony.TelephonyCapabilities;
import com.android.phone.settings.SuppServicesUiUtil;
import com.android.telephony.Rlog;

import java.io.IOException;
import java.util.List;

/**
 * Misc utilities for the Phone app.
 */
public class PhoneUtils {
    public static final String EMERGENCY_ACCOUNT_HANDLE_ID = "E";
    private static final String LOG_TAG = "PhoneUtils";
    private static final boolean DBG = (PhoneGlobals.DBG_LEVEL >= 2);

    // Do not check in with VDBG = true, since that may write PII to the system log.
    private static final boolean VDBG = false;

    // Return codes from placeCall()
    public static final int CALL_STATUS_DIALED = 0;  // The number was successfully dialed
    public static final int CALL_STATUS_DIALED_MMI = 1;  // The specified number was an MMI code
    public static final int CALL_STATUS_FAILED = 2;  // The call failed

    // USSD string length for MMI operations
    static final int MIN_USSD_LEN = 1;
    static final int MAX_USSD_LEN = 160;

    /** Define for not a special CNAP string */
    private static final int CNAP_SPECIAL_CASE_NO = -1;

    /** Define for default vibrate pattern if res cannot be found */
    private static final long[] DEFAULT_VIBRATE_PATTERN = {0, 250, 250, 250};

    /**
     * Theme to use for dialogs displayed by utility methods in this class. This is needed
     * because these dialogs are displayed using the application context, which does not resolve
     * the dialog theme correctly.
     */
    private static final int THEME = com.android.internal.R.style.Theme_DeviceDefault_Dialog_Alert;

    /** USSD information used to aggregate all USSD messages */
    private static StringBuilder sUssdMsg = new StringBuilder();

    private static final ComponentName PSTN_CONNECTION_SERVICE_COMPONENT =
            new ComponentName("com.android.phone",
                    "com.android.services.telephony.TelephonyConnectionService");

    /** This class is never instantiated. */
    private PhoneUtils() {
    }

    /**
     * For a CDMA phone, advance the call state upon making a new
     * outgoing call.
     *
     * <pre>
     *   IDLE -> SINGLE_ACTIVE
     * or
     *   SINGLE_ACTIVE -> THRWAY_ACTIVE
     * </pre>
     * @param app The phone instance.
     */
    private static void updateCdmaCallStateOnNewOutgoingCall(PhoneGlobals app,
            Connection connection) {
        if (app.cdmaPhoneCallState.getCurrentCallState() ==
            CdmaPhoneCallState.PhoneCallState.IDLE) {
            // This is the first outgoing call. Set the Phone Call State to ACTIVE
            app.cdmaPhoneCallState.setCurrentCallState(
                CdmaPhoneCallState.PhoneCallState.SINGLE_ACTIVE);
        } else {
            // This is the second outgoing call. Set the Phone Call State to 3WAY
            app.cdmaPhoneCallState.setCurrentCallState(
                CdmaPhoneCallState.PhoneCallState.THRWAY_ACTIVE);

            // TODO: Remove this code.
            //app.getCallModeler().setCdmaOutgoing3WayCall(connection);
        }
    }

    /**
     * Dial the number using the phone passed in.
     *
     * @param context To perform the CallerInfo query.
     * @param phone the Phone object.
     * @param number to be dialed as requested by the user. This is
     * NOT the phone number to connect to. It is used only to build the
     * call card and to update the call log. See above for restrictions.
     *
     * @return either CALL_STATUS_DIALED or CALL_STATUS_FAILED
     */
    public static int placeOtaspCall(Context context, Phone phone, String number) {
        final Uri gatewayUri = null;

        if (VDBG) {
            log("placeCall()... number: '" + number + "'"
                    + ", GW:'" + gatewayUri + "'");
        } else {
            log("placeCall()... number: " + toLogSafePhoneNumber(number)
                    + ", GW: " + (gatewayUri != null ? "non-null" : "null"));
        }
        final PhoneGlobals app = PhoneGlobals.getInstance();

        boolean useGateway = false;
        Uri contactRef = null;

        int status = CALL_STATUS_DIALED;
        Connection connection;
        String numberToDial;
        numberToDial = number;

        try {
            connection = app.mCM.dial(phone, numberToDial, VideoProfile.STATE_AUDIO_ONLY);
        } catch (CallStateException ex) {
            // CallStateException means a new outgoing call is not currently
            // possible: either no more call slots exist, or there's another
            // call already in the process of dialing or ringing.
            Log.w(LOG_TAG, "Exception from app.mCM.dial()", ex);
            return CALL_STATUS_FAILED;

            // Note that it's possible for CallManager.dial() to return
            // null *without* throwing an exception; that indicates that
            // we dialed an MMI (see below).
        }

        int phoneType = phone.getPhoneType();

        // On GSM phones, null is returned for MMI codes
        if (null == connection) {
            status = CALL_STATUS_FAILED;
        } else {
            if (phoneType == PhoneConstants.PHONE_TYPE_CDMA) {
                updateCdmaCallStateOnNewOutgoingCall(app, connection);
            }
        }

        return status;
    }

    /* package */ static String toLogSafePhoneNumber(String number) {
        // For unknown number, log empty string.
        if (number == null) {
            return "";
        }

        if (VDBG) {
            // When VDBG is true we emit PII.
            return number;
        }

        // Do exactly same thing as Uri#toSafeString() does, which will enable us to compare
        // sanitized phone numbers.
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < number.length(); i++) {
            char c = number.charAt(i);
            if (c == '-' || c == '@' || c == '.') {
                builder.append(c);
            } else {
                builder.append('x');
            }
        }
        return builder.toString();
    }

    /**
     * Handle the MMIInitiate message and put up an alert that lets
     * the user cancel the operation, if applicable.
     *
     * @param context context to get strings.
     * @param mmiCode the MmiCode object being started.
     * @param buttonCallbackMessage message to post when button is clicked.
     * @param previousAlert a previous alert used in this activity.
     * @return the dialog handle
     */
    static Dialog displayMMIInitiate(Context context,
                                          MmiCode mmiCode,
                                          Message buttonCallbackMessage,
                                          Dialog previousAlert) {
        log("displayMMIInitiate: " + Rlog.pii(LOG_TAG, mmiCode.toString()));
        if (previousAlert != null) {
            previousAlert.dismiss();
        }

        // The UI paradigm we are using now requests that all dialogs have
        // user interaction, and that any other messages to the user should
        // be by way of Toasts.
        //
        // In adhering to this request, all MMI initiating "OK" dialogs
        // (non-cancelable MMIs) that end up being closed when the MMI
        // completes (thereby showing a completion dialog) are being
        // replaced with Toasts.
        //
        // As a side effect, moving to Toasts for the non-cancelable MMIs
        // also means that buttonCallbackMessage (which was tied into "OK")
        // is no longer invokable for these dialogs.  This is not a problem
        // since the only callback messages we supported were for cancelable
        // MMIs anyway.
        //
        // A cancelable MMI is really just a USSD request. The term
        // "cancelable" here means that we can cancel the request when the
        // system prompts us for a response, NOT while the network is
        // processing the MMI request.  Any request to cancel a USSD while
        // the network is NOT ready for a response may be ignored.
        //
        // With this in mind, we replace the cancelable alert dialog with
        // a progress dialog, displayed until we receive a request from
        // the the network.  For more information, please see the comments
        // in the displayMMIComplete() method below.
        //
        // Anything that is NOT a USSD request is a normal MMI request,
        // which will bring up a toast (desribed above).

        boolean isCancelable = (mmiCode != null) && mmiCode.isCancelable();

        if (!isCancelable) {
            log("displayMMIInitiate: not a USSD code, displaying status toast.");
            CharSequence text = context.getText(R.string.mmiStarted);
            Toast.makeText(context, text, Toast.LENGTH_SHORT)
                .show();
            return null;
        } else {
            log("displayMMIInitiate: running USSD code, displaying intermediate progress.");

            // create the indeterminate progress dialog and display it.
            ProgressDialog pd = new ProgressDialog(context, THEME);
            pd.setMessage(context.getText(R.string.ussdRunning));
            pd.setCancelable(false);
            pd.setIndeterminate(true);
            pd.getWindow().addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

            pd.show();

            return pd;
        }

    }

    /**
     * Handle the MMIComplete message and fire off an intent to display
     * the message.
     *
     * @param context context to get strings.
     * @param mmiCode MMI result.
     * @param previousAlert a previous alert used in this activity.
     */
    static void displayMMIComplete(final Phone phone, Context context, final MmiCode mmiCode,
            Message dismissCallbackMessage,
            AlertDialog previousAlert) {
        final PhoneGlobals app = PhoneGlobals.getInstance();
        CharSequence text;
        int title = 0;  // title for the progress dialog, if needed.
        MmiCode.State state = mmiCode.getState();

        log("displayMMIComplete: state=" + state);

        switch (state) {
            case PENDING:
                // USSD code asking for feedback from user.
                text = mmiCode.getMessage();
                log("displayMMIComplete: using text from PENDING MMI message: '" + text + "'");
                break;
            case CANCELLED:
                text = null;
                break;
            case COMPLETE:
                PersistableBundle b = null;
                if (SubscriptionManager.isValidSubscriptionId(phone.getSubId())) {
                    b = app.getCarrierConfigForSubId(
                            phone.getSubId());
                } else {
                    b = app.getCarrierConfig();
                }

                if (b.getBoolean(CarrierConfigManager.KEY_USE_CALLER_ID_USSD_BOOL)) {
                    text = SuppServicesUiUtil.handleCallerIdUssdResponse(app, context, phone,
                            mmiCode);
                    if (mmiCode.getMessage() != null && !text.equals(mmiCode.getMessage())) {
                        break;
                    }
                }

                if (app.getPUKEntryActivity() != null) {
                    // if an attempt to unPUK the device was made, we specify
                    // the title and the message here.
                    title = com.android.internal.R.string.PinMmi;
                    text = context.getText(R.string.puk_unlocked);
                    break;
                }
                // All other conditions for the COMPLETE mmi state will cause
                // the case to fall through to message logic in common with
                // the FAILED case.

            case FAILED:
                text = mmiCode.getMessage();
                log("displayMMIComplete (failed): using text from MMI message: '" + text + "'");
                break;
            default:
                throw new IllegalStateException("Unexpected MmiCode state: " + state);
        }

        if (previousAlert != null) {
            previousAlert.dismiss();
        }

        // Check to see if a UI exists for the PUK activation.  If it does
        // exist, then it indicates that we're trying to unblock the PUK.
        if ((app.getPUKEntryActivity() != null) && (state == MmiCode.State.COMPLETE)) {
            log("displaying PUK unblocking progress dialog.");

            // create the progress dialog, make sure the flags and type are
            // set correctly.
            ProgressDialog pd = new ProgressDialog(app, THEME);
            pd.setTitle(title);
            pd.setMessage(text);
            pd.setCancelable(false);
            pd.setIndeterminate(true);
            pd.getWindow().setType(WindowManager.LayoutParams.TYPE_SYSTEM_DIALOG);
            pd.getWindow().addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

            // display the dialog
            pd.show();

            // indicate to the Phone app that the progress dialog has
            // been assigned for the PUK unlock / SIM READY process.
            app.setPukEntryProgressDialog(pd);

        } else if ((app.getPUKEntryActivity() != null) && (state == MmiCode.State.FAILED)) {
            createUssdDialog(app, context, text, phone,
                    WindowManager.LayoutParams.TYPE_KEYGUARD_DIALOG);
            // In case of failure to unlock, we'll need to reset the
            // PUK unlock activity, so that the user may try again.
            app.setPukEntryActivity(null);
        } else {
            // In case of failure to unlock, we'll need to reset the
            // PUK unlock activity, so that the user may try again.
            if (app.getPUKEntryActivity() != null) {
                app.setPukEntryActivity(null);
            }

            // A USSD in a pending state means that it is still
            // interacting with the user.
            if (state != MmiCode.State.PENDING) {
                createUssdDialog(app, context, text, phone,
                        WindowManager.LayoutParams.TYPE_SYSTEM_ALERT);
            } else {
                log("displayMMIComplete: USSD code has requested user input. Constructing input "
                        + "dialog.");

                // USSD MMI code that is interacting with the user.  The
                // basic set of steps is this:
                //   1. User enters a USSD request
                //   2. We recognize the request and displayMMIInitiate
                //      (above) creates a progress dialog.
                //   3. Request returns and we get a PENDING or COMPLETE
                //      message.
                //   4. These MMI messages are caught in the PhoneApp
                //      (onMMIComplete) and the InCallScreen
                //      (mHandler.handleMessage) which bring up this dialog
                //      and closes the original progress dialog,
                //      respectively.
                //   5. If the message is anything other than PENDING,
                //      we are done, and the alert dialog (directly above)
                //      displays the outcome.
                //   6. If the network is requesting more information from
                //      the user, the MMI will be in a PENDING state, and
                //      we display this dialog with the message.
                //   7. User input, or cancel requests result in a return
                //      to step 1.  Keep in mind that this is the only
                //      time that a USSD should be canceled.

                // inflate the layout with the scrolling text area for the dialog.
                ContextThemeWrapper contextThemeWrapper =
                        new ContextThemeWrapper(context, R.style.DialerAlertDialogTheme);
                LayoutInflater inflater = (LayoutInflater) contextThemeWrapper.getSystemService(
                        Context.LAYOUT_INFLATER_SERVICE);
                View dialogView = inflater.inflate(R.layout.dialog_ussd_response, null);

                // get the input field.
                final EditText inputText = (EditText) dialogView.findViewById(R.id.input_field);

                // specify the dialog's click listener, with SEND and CANCEL logic.
                final DialogInterface.OnClickListener mUSSDDialogListener =
                    new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int whichButton) {
                            switch (whichButton) {
                                case DialogInterface.BUTTON_POSITIVE:
                                    // As per spec 24.080, valid length of ussd string
                                    // is 1 - 160. If length is out of the range then
                                    // display toast message & Cancel MMI operation.
                                    if (inputText.length() < MIN_USSD_LEN
                                            || inputText.length() > MAX_USSD_LEN) {
                                        Toast.makeText(app,
                                                app.getResources().getString(R.string.enter_input,
                                                MIN_USSD_LEN, MAX_USSD_LEN),
                                                Toast.LENGTH_LONG).show();
                                        if (mmiCode.isCancelable()) {
                                            mmiCode.cancel();
                                        }
                                    } else {
                                        phone.sendUssdResponse(inputText.getText().toString());
                                    }
                                    break;
                                case DialogInterface.BUTTON_NEGATIVE:
                                    if (mmiCode.isCancelable()) {
                                        mmiCode.cancel();
                                    }
                                    break;
                            }
                        }
                    };

                // build the dialog
                final AlertDialog newDialog = new AlertDialog.Builder(contextThemeWrapper)
                        .setMessage(text)
                        .setView(dialogView)
                        .setPositiveButton(R.string.send_button, mUSSDDialogListener)
                        .setNegativeButton(R.string.cancel, mUSSDDialogListener)
                        .setCancelable(false)
                        .create();

                // attach the key listener to the dialog's input field and make
                // sure focus is set.
                final View.OnKeyListener mUSSDDialogInputListener =
                    new View.OnKeyListener() {
                        public boolean onKey(View v, int keyCode, KeyEvent event) {
                            switch (keyCode) {
                                case KeyEvent.KEYCODE_CALL:
                                case KeyEvent.KEYCODE_ENTER:
                                    if(event.getAction() == KeyEvent.ACTION_DOWN) {
                                        phone.sendUssdResponse(inputText.getText().toString());
                                        newDialog.dismiss();
                                    }
                                    return true;
                            }
                            return false;
                        }
                    };
                inputText.setOnKeyListener(mUSSDDialogInputListener);
                inputText.requestFocus();

                // set the window properties of the dialog
                newDialog.getWindow().setType(
                        WindowManager.LayoutParams.TYPE_SYSTEM_DIALOG);
                newDialog.getWindow().addFlags(
                        WindowManager.LayoutParams.FLAG_DIM_BEHIND);

                // now show the dialog!
                newDialog.show();

                newDialog.getButton(DialogInterface.BUTTON_POSITIVE)
                        .setTextColor(context.getResources().getColor(R.color.dialer_theme_color));
                newDialog.getButton(DialogInterface.BUTTON_NEGATIVE)
                        .setTextColor(context.getResources().getColor(R.color.dialer_theme_color));
            }

            if (mmiCode.isNetworkInitiatedUssd()) {
                playSound(context);
            }
        }
    }

    private static void playSound(Context context) {
        AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        int callsRingerMode = audioManager.getRingerMode();

        if (callsRingerMode == AudioManager.RINGER_MODE_NORMAL) {
            log("playSound : RINGER_MODE_NORMAL");
            try {
                Uri notificationUri = RingtoneManager.getDefaultUri(
                        RingtoneManager.TYPE_NOTIFICATION);
                MediaPlayer mediaPlayer = new MediaPlayer();
                mediaPlayer.setDataSource(context, notificationUri);
                AudioAttributes aa = new AudioAttributes.Builder()
                        .setLegacyStreamType(AudioManager.STREAM_NOTIFICATION)
                        .setUsage(AudioAttributes.USAGE_NOTIFICATION)
                        .build();
                mediaPlayer.setAudioAttributes(aa);
                mediaPlayer.setLooping(false);
                mediaPlayer.prepare();
                mediaPlayer.start();
            } catch (IOException e) {
                log("playSound exception : " + e);
            }
        } else if (callsRingerMode == AudioManager.RINGER_MODE_VIBRATE) {
            log("playSound : RINGER_MODE_VIBRATE");
            Vibrator vibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
            // Use NotificationManagerService#DEFAULT_VIBRATE_PATTERN if
            // R.array.config_defaultNotificationVibePattern is not defined.
            long[] pattern = getLongArray(context.getResources(),
                    R.array.config_defaultNotificationVibePattern, DEFAULT_VIBRATE_PATTERN);
            vibrator.vibrate(VibrationEffect.createWaveform(pattern, -1));
        }
    }

    private static long[] getLongArray(Resources r, int resid, long[] def) {
        int[] ar = r.getIntArray(resid);
        if (ar == null) {
            return def;
        }
        final int len = ar.length;
        long[] out = new long[len];
        for (int i = 0; i < len; i++) {
            out[i] = ar[i];
        }
        return out;
    }

    /**
     * It displays the message dialog for user about the mmi code result message.
     *
     * @param app This is {@link PhoneGlobals}
     * @param context Context to get strings.
     * @param text This is message's result.
     * @param phone This is phone to create sssd dialog.
     * @param windowType The new window type. {@link WindowManager.LayoutParams}.
     */
    public static void createUssdDialog(PhoneGlobals app, Context context, CharSequence text,
            Phone phone, int windowType) {
        log("displayMMIComplete: MMI code has finished running.");

        log("displayMMIComplete: Extended NW displayMMIInitiate (" + text + ")");
        if (text == null || text.length() == 0) {
            return;
        }

        // displaying system alert dialog on the screen instead of
        // using another activity to display the message.  This
        // places the message at the forefront of the UI.
        AlertDialog ussdDialog = new AlertDialog.Builder(context, THEME)
                .setPositiveButton(R.string.ok, null)
                .setCancelable(true)
                .setOnDismissListener(new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(DialogInterface dialog) {
                        sUssdMsg.setLength(0);
                    }
                })
                .create();

        ussdDialog.getWindow().setType(windowType);
        ussdDialog.getWindow().addFlags(
                WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        if (sUssdMsg.length() != 0) {
            sUssdMsg.insert(0, "\n")
                    .insert(0, app.getResources().getString(R.string.ussd_dialog_sep))
                    .insert(0, "\n");
        }
        if (phone != null && phone.getCarrierName() != null) {
            ussdDialog.setTitle(app.getResources().getString(R.string.carrier_mmi_msg_title,
                    phone.getCarrierName()));
        } else {
            ussdDialog
                    .setTitle(app.getResources().getString(R.string.default_carrier_mmi_msg_title));
        }
        sUssdMsg.insert(0, text);
        ussdDialog.setMessage(sUssdMsg.toString());
        ussdDialog.show();
    }

    /**
     * Cancels the current pending MMI operation, if applicable.
     * @return true if we canceled an MMI operation, or false
     *         if the current pending MMI wasn't cancelable
     *         or if there was no current pending MMI at all.
     *
     * @see displayMMIInitiate
     */
    static boolean cancelMmiCode(Phone phone) {
        List<? extends MmiCode> pendingMmis = phone.getPendingMmiCodes();
        int count = pendingMmis.size();
        if (DBG) log("cancelMmiCode: num pending MMIs = " + count);

        boolean canceled = false;
        if (count > 0) {
            // assume that we only have one pending MMI operation active at a time.
            // I don't think it's possible to enter multiple MMI codes concurrently
            // in the phone UI, because during the MMI operation, an Alert panel
            // is displayed, which prevents more MMI code from being entered.
            MmiCode mmiCode = pendingMmis.get(0);
            if (mmiCode.isCancelable()) {
                mmiCode.cancel();
                canceled = true;
            }
        }
        return canceled;
    }


    //
    // Misc UI policy helper functions
    //

    /**
     * Check if a phone number can be route through a 3rd party
     * gateway. The number must be a global phone number in numerical
     * form (1-800-666-SEXY won't work).
     *
     * MMI codes and the like cannot be used as a dial number for the
     * gateway either.
     *
     * @param number To be dialed via a 3rd party gateway.
     * @return true If the number can be routed through the 3rd party network.
     */
    private static boolean isRoutableViaGateway(String number) {
        if (TextUtils.isEmpty(number)) {
            return false;
        }
        number = PhoneNumberUtils.stripSeparators(number);
        if (!number.equals(PhoneNumberUtils.convertKeypadLettersToDigits(number))) {
            return false;
        }
        number = PhoneNumberUtils.extractNetworkPortion(number);
        return PhoneNumberUtils.isGlobalPhoneNumber(number);
    }

    /**
     * Returns whether the phone is in ECM ("Emergency Callback Mode") or not.
     */
    /* package */ static boolean isPhoneInEcm(Phone phone) {
        if ((phone != null) && TelephonyCapabilities.supportsEcm(phone)) {
            return phone.isInEcm();
        }
        return false;
    }

    /**
     * Returns true when the given call is in INCOMING state and there's no foreground phone call,
     * meaning the call is the first real incoming call the phone is having.
     */
    public static boolean isRealIncomingCall(Call.State state) {
        return (state == Call.State.INCOMING && !PhoneGlobals.getInstance().mCM.hasActiveFgCall());
    }

    //
    // General phone and call state debugging/testing code
    //

    private static void log(String msg) {
        Log.d(LOG_TAG, msg);
    }

    public static PhoneAccountHandle makePstnPhoneAccountHandle(String id) {
        return makePstnPhoneAccountHandleWithPrefix(id, "", false);
    }

    public static PhoneAccountHandle makePstnPhoneAccountHandle(int phoneId) {
        return makePstnPhoneAccountHandle(PhoneFactory.getPhone(phoneId));
    }

    public static PhoneAccountHandle makePstnPhoneAccountHandle(Phone phone) {
        return makePstnPhoneAccountHandleWithPrefix(phone, "", false);
    }

    public static PhoneAccountHandle makePstnPhoneAccountHandleWithPrefix(
            Phone phone, String prefix, boolean isEmergency) {
        // TODO: Should use some sort of special hidden flag to decorate this account as
        // an emergency-only account
        String id = isEmergency ? EMERGENCY_ACCOUNT_HANDLE_ID : prefix +
                String.valueOf(phone.getFullIccSerialNumber());
        return makePstnPhoneAccountHandleWithPrefix(id, prefix, isEmergency);
    }

    public static PhoneAccountHandle makePstnPhoneAccountHandleWithPrefix(
            String id, String prefix, boolean isEmergency) {
        ComponentName pstnConnectionServiceName = getPstnConnectionServiceName();
        return new PhoneAccountHandle(pstnConnectionServiceName, id);
    }

    public static int getSubIdForPhoneAccount(PhoneAccount phoneAccount) {
        if (phoneAccount != null
                && phoneAccount.hasCapabilities(PhoneAccount.CAPABILITY_SIM_SUBSCRIPTION)) {
            return getSubIdForPhoneAccountHandle(phoneAccount.getAccountHandle());
        }
        return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
    }

    public static int getSubIdForPhoneAccountHandle(PhoneAccountHandle handle) {
        Phone phone = getPhoneForPhoneAccountHandle(handle);
        if (phone != null) {
            return phone.getSubId();
        }
        return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
    }

    public static Phone getPhoneForPhoneAccountHandle(PhoneAccountHandle handle) {
        if (handle != null && handle.getComponentName().equals(getPstnConnectionServiceName())) {
            return getPhoneFromIccId(handle.getId());
        }
        return null;
    }

    /**
     * Determine if a given phone account corresponds to an active SIM
     *
     * @param sm An instance of the subscription manager so it is not recreated for each calling of
     * this method.
     * @param handle The handle for the phone account to check
     * @return {@code true} If there is an active SIM for this phone account,
     * {@code false} otherwise.
     */
    public static boolean isPhoneAccountActive(SubscriptionManager sm, PhoneAccountHandle handle) {
        return sm.getActiveSubscriptionInfoForIcc(handle.getId()) != null;
    }

    private static ComponentName getPstnConnectionServiceName() {
        return PSTN_CONNECTION_SERVICE_COMPONENT;
    }

    private static Phone getPhoneFromIccId(String iccId) {
        if (!TextUtils.isEmpty(iccId)) {
            for (Phone phone : PhoneFactory.getPhones()) {
                String phoneIccId = phone.getFullIccSerialNumber();
                if (iccId.equals(phoneIccId)) {
                    return phone;
                }
            }
        }
        return null;
    }

    /**
     * Register ICC status for all phones.
     */
    static final void registerIccStatus(Handler handler, int event) {
        for (Phone phone : PhoneFactory.getPhones()) {
            IccCard sim = phone.getIccCard();
            if (sim != null) {
                if (VDBG) Log.v(LOG_TAG, "register for ICC status, phone " + phone.getPhoneId());
                sim.registerForNetworkLocked(handler, event, phone);
            }
        }
    }

    /**
     * Register ICC status for all phones.
     */
    static final void registerIccStatus(Handler handler, int event, int phoneId) {
        Phone[] phones = PhoneFactory.getPhones();
        IccCard sim = phones[phoneId].getIccCard();
        if (sim != null) {
            if (VDBG) {
                Log.v(LOG_TAG, "register for ICC status, phone " + phones[phoneId].getPhoneId());
            }
            sim.registerForNetworkLocked(handler, event, phones[phoneId]);
        }
    }

    /**
     * Unregister ICC status for a specific phone.
     */
    static final void unregisterIccStatus(Handler handler, int phoneId) {
        Phone[] phones = PhoneFactory.getPhones();
        IccCard sim = phones[phoneId].getIccCard();
        if (sim != null) {
            if (VDBG) {
                Log.v(LOG_TAG, "unregister for ICC status, phone " + phones[phoneId].getPhoneId());
            }
            sim.unregisterForNetworkLocked(handler);
        }
    }

    /**
     * Set the radio power on/off state for all phones.
     *
     * @param enabled true means on, false means off.
     */
    static final void setRadioPower(boolean enabled) {
        for (Phone phone : PhoneFactory.getPhones()) {
            phone.setRadioPower(enabled);
        }
    }
}
