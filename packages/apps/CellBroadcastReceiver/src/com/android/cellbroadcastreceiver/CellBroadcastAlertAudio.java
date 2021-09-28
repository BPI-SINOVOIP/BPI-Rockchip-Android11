/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.cellbroadcastreceiver;

import static android.telephony.PhoneStateListener.LISTEN_NONE;

import static com.android.cellbroadcastreceiver.CellBroadcastReceiver.DBG;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.AssetFileDescriptor;
import android.content.res.Resources;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.MediaPlayer.OnCompletionListener;
import android.media.MediaPlayer.OnErrorListener;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.preference.PreferenceManager;
import android.speech.tts.TextToSpeech;
import android.telephony.PhoneStateListener;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;

import com.android.cellbroadcastreceiver.CellBroadcastAlertService.AlertType;
import com.android.internal.annotations.VisibleForTesting;

import java.util.Locale;

/**
 * Manages alert audio and vibration and text-to-speech. Runs as a service so that
 * it can continue to play if another activity overrides the CellBroadcastListActivity.
 */
public class CellBroadcastAlertAudio extends Service implements TextToSpeech.OnInitListener,
        TextToSpeech.OnUtteranceCompletedListener {
    private static final String TAG = "CellBroadcastAlertAudio";

    /** Action to start playing alert audio/vibration/speech. */
    @VisibleForTesting
    public static final String ACTION_START_ALERT_AUDIO = "ACTION_START_ALERT_AUDIO";

    /** Extra for message body to speak (if speech enabled in settings). */
    public static final String ALERT_AUDIO_MESSAGE_BODY =
            "com.android.cellbroadcastreceiver.ALERT_AUDIO_MESSAGE_BODY";

    /** Extra for text-to-speech preferred language (if speech enabled in settings). */
    public static final String ALERT_AUDIO_MESSAGE_LANGUAGE =
            "com.android.cellbroadcastreceiver.ALERT_AUDIO_MESSAGE_LANGUAGE";

    /** Extra for alert tone type */
    public static final String ALERT_AUDIO_TONE_TYPE =
            "com.android.cellbroadcastreceiver.ALERT_AUDIO_TONE_TYPE";

    /** Extra for alert vibration pattern (unless master volume is silent). */
    public static final String ALERT_AUDIO_VIBRATION_PATTERN_EXTRA =
            "com.android.cellbroadcastreceiver.ALERT_AUDIO_VIBRATION_PATTERN";

    /** Extra for playing alert sound in full volume regardless Do Not Disturb is on. */
    public static final String ALERT_AUDIO_OVERRIDE_DND_EXTRA =
            "com.android.cellbroadcastreceiver.ALERT_OVERRIDE_DND_EXTRA";

    /** Extra for cutomized alert duration in ms. */
    public static final String ALERT_AUDIO_DURATION =
            "com.android.cellbroadcastreceiver.ALERT_AUDIO_DURATION";

    /** Extra for alert subscription index */
    public static final String ALERT_AUDIO_SUB_INDEX =
            "com.android.cellbroadcastreceiver.ALERT_AUDIO_SUB_INDEX";

    private static final String TTS_UTTERANCE_ID = "com.android.cellbroadcastreceiver.UTTERANCE_ID";

    /** Pause duration between alert sound and alert speech. */
    private static final long PAUSE_DURATION_BEFORE_SPEAKING_MSEC = 1000L;

    private static final int STATE_IDLE = 0;
    private static final int STATE_ALERTING = 1;
    private static final int STATE_PAUSING = 2;
    private static final int STATE_SPEAKING = 3;

    /** Default LED flashing frequency is 250 milliseconds */
    private static final long DEFAULT_LED_FLASH_INTERVAL_MSEC = 250L;

    private int mState;

    private TextToSpeech mTts;
    private boolean mTtsEngineReady;

    private AlertType mAlertType;
    private String mMessageBody;
    private String mMessageLanguage;
    private int mSubId;
    private boolean mTtsLanguageSupported;
    private boolean mEnableVibrate;
    private boolean mEnableAudio;
    private boolean mEnableLedFlash;
    private boolean mOverrideDnd;
    private boolean mResetAlarmVolumeNeeded;
    private int mUserSetAlarmVolume;
    private int[] mVibrationPattern;
    private int mAlertDuration = -1;

    private Vibrator mVibrator;
    private MediaPlayer mMediaPlayer;
    private AudioManager mAudioManager;
    private TelephonyManager mTelephonyManager;
    private int mInitialCallState;

    // Internal messages
    private static final int ALERT_SOUND_FINISHED = 1000;
    private static final int ALERT_PAUSE_FINISHED = 1001;
    private static final int ALERT_LED_FLASH_TOGGLE = 1002;

    private Handler mHandler;

    private PhoneStateListener mPhoneStateListener;

    /**
     * Callback from TTS engine after initialization.
     *
     * @param status {@link TextToSpeech#SUCCESS} or {@link TextToSpeech#ERROR}.
     */
    @Override
    public void onInit(int status) {
        if (DBG) log("onInit() TTS engine status: " + status);
        if (status == TextToSpeech.SUCCESS) {
            mTtsEngineReady = true;
            mTts.setOnUtteranceCompletedListener(this);
            // try to set the TTS language to match the broadcast
            setTtsLanguage();
        } else {
            mTtsEngineReady = false;
            mTts = null;
            loge("onInit() TTS engine error: " + status);
        }
    }

    /**
     * Try to set the TTS engine language to the preferred language. If failed, set
     * it to the default language. mTtsLanguageSupported will be updated based on the response.
     */
    private void setTtsLanguage() {
        Locale locale;
        if (!TextUtils.isEmpty(mMessageLanguage)) {
            locale = new Locale(mMessageLanguage);
        } else {
            // If the cell broadcast message does not specify the language, use device's default
            // language.
            locale = Locale.getDefault();
        }

        if (DBG) log("Setting TTS language to '" + locale + '\'');

        int result = mTts.setLanguage(locale);
        if (DBG) log("TTS setLanguage() returned: " + result);
        mTtsLanguageSupported = (result >= TextToSpeech.LANG_AVAILABLE);
    }

    /**
     * Callback from TTS engine.
     *
     * @param utteranceId the identifier of the utterance.
     */
    @Override
    public void onUtteranceCompleted(String utteranceId) {
        if (utteranceId.equals(TTS_UTTERANCE_ID)) {
            // When we reach here, it could be TTS completed or TTS was cut due to another
            // new alert started playing. We don't want to stop the service in the later case.
            if (mState == STATE_SPEAKING) {
                if (DBG) log("TTS completed. Stop CellBroadcastAlertAudio service");
                stopSelf();
            }
        }
    }

    @Override
    public void onCreate() {
        mVibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        // Listen for incoming calls to kill the alarm.
        mTelephonyManager = ((TelephonyManager) getSystemService(Context.TELEPHONY_SERVICE));
        mHandler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(Message msg) {
                switch (msg.what) {
                    case ALERT_SOUND_FINISHED:
                        if (DBG) log("ALERT_SOUND_FINISHED");
                        stop();     // stop alert sound
                        // if we can speak the message text
                        if (mMessageBody != null && mTtsEngineReady && mTtsLanguageSupported) {
                            sendMessageDelayed(mHandler.obtainMessage(ALERT_PAUSE_FINISHED),
                                    PAUSE_DURATION_BEFORE_SPEAKING_MSEC);
                            mState = STATE_PAUSING;
                        } else {
                            if (DBG) {
                                log("MessageEmpty = " + (mMessageBody == null)
                                        + ", mTtsEngineReady = " + mTtsEngineReady
                                        + ", mTtsLanguageSupported = " + mTtsLanguageSupported);
                            }
                            stopSelf();
                            mState = STATE_IDLE;
                        }
                        // Set alert reminder depending on user preference
                        CellBroadcastAlertReminder.queueAlertReminder(getApplicationContext(),
                                mSubId,
                                true);
                        break;

                    case ALERT_PAUSE_FINISHED:
                        if (DBG) log("ALERT_PAUSE_FINISHED");
                        int res = TextToSpeech.ERROR;
                        if (mMessageBody != null && mTtsEngineReady && mTtsLanguageSupported) {
                            if (DBG) log("Speaking broadcast text: " + mMessageBody);

                            mTts.setAudioAttributes(getAlertAudioAttributes());
                            res = mTts.speak(mMessageBody, 2, null, TTS_UTTERANCE_ID);
                            mState = STATE_SPEAKING;
                        }
                        if (res != TextToSpeech.SUCCESS) {
                            loge("TTS engine not ready or language not supported or speak() "
                                    + "failed");
                            stopSelf();
                            mState = STATE_IDLE;
                        }
                        break;

                    case ALERT_LED_FLASH_TOGGLE:
                        if (enableLedFlash(msg.arg1 != 0)) {
                            sendMessageDelayed(mHandler.obtainMessage(
                                    ALERT_LED_FLASH_TOGGLE, msg.arg1 != 0 ? 0 : 1, 0),
                                    DEFAULT_LED_FLASH_INTERVAL_MSEC);
                        }
                        break;

                    default:
                        loge("Handler received unknown message, what=" + msg.what);
                }
            }
        };
        mPhoneStateListener = new PhoneStateListener() {
            @Override
            public void onCallStateChanged(int state, String ignored) {
                // Stop the alert sound and speech if the call state changes.
                if (state != TelephonyManager.CALL_STATE_IDLE
                        && state != mInitialCallState) {
                    if (DBG) log("Call interrupted. Stop CellBroadcastAlertAudio service");
                    stopSelf();
                }
            }
        };
        mTelephonyManager.listen(mPhoneStateListener, PhoneStateListener.LISTEN_CALL_STATE);
    }

    @Override
    public void onDestroy() {
        // stop audio, vibration and TTS
        if (DBG) log("onDestroy");
        stop();
        // Stop listening for incoming calls.
        mTelephonyManager.listen(mPhoneStateListener, LISTEN_NONE);
        // shutdown TTS engine
        if (mTts != null) {
            try {
                mTts.shutdown();
            } catch (IllegalStateException e) {
                // catch "Unable to retrieve AudioTrack pointer for stop()" exception
                loge("exception trying to shutdown text-to-speech");
            }
        }
        if (mEnableAudio) {
            // Release the audio focus so other audio (e.g. music) can resume.
            // Do not do this in stop() because stop() is also called when we stop the tone (before
            // TTS is playing). We only want to release the focus when tone and TTS are played.
            mAudioManager.abandonAudioFocus(null);
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // No intent, tell the system not to restart us.
        if (intent == null) {
            if (DBG) log("Null intent. Stop CellBroadcastAlertAudio service");
            stopSelf();
            return START_NOT_STICKY;
        }

        // Get text to speak (if enabled by user)
        mMessageBody = intent.getStringExtra(ALERT_AUDIO_MESSAGE_BODY);
        mMessageLanguage = intent.getStringExtra(ALERT_AUDIO_MESSAGE_LANGUAGE);
        mSubId = intent.getIntExtra(ALERT_AUDIO_SUB_INDEX,
                SubscriptionManager.INVALID_SUBSCRIPTION_ID);

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);

        // retrieve whether to play alert sound in full volume regardless Do Not Disturb is on.
        mOverrideDnd = intent.getBooleanExtra(ALERT_AUDIO_OVERRIDE_DND_EXTRA, false);
        // retrieve the vibrate settings from cellbroadcast receiver settings.
        mEnableVibrate = prefs.getBoolean(CellBroadcastSettings.KEY_ENABLE_ALERT_VIBRATE, true)
                || mOverrideDnd;
        // retrieve the vibration patterns.
        mVibrationPattern = intent.getIntArrayExtra(ALERT_AUDIO_VIBRATION_PATTERN_EXTRA);

        Resources res = CellBroadcastSettings.getResources(getApplicationContext(), mSubId);
        mEnableLedFlash = res.getBoolean(R.bool.enable_led_flash);

        // retrieve the customized alert duration. -1 means play the alert with the tone's duration.
        mAlertDuration = intent.getIntExtra(ALERT_AUDIO_DURATION, -1);
        // retrieve the alert type
        mAlertType = AlertType.DEFAULT;
        if (intent.getSerializableExtra(ALERT_AUDIO_TONE_TYPE) != null) {
            mAlertType = (AlertType) intent.getSerializableExtra(ALERT_AUDIO_TONE_TYPE);
        }

        switch (mAudioManager.getRingerMode()) {
            case AudioManager.RINGER_MODE_SILENT:
                if (DBG) log("Ringer mode: silent");
                if (!mOverrideDnd) {
                    mEnableVibrate = false;
                }
                // If the phone is in silent mode, we only enable the audio when override dnd
                // setting is turned on.
                mEnableAudio = mOverrideDnd;
                break;
            case AudioManager.RINGER_MODE_VIBRATE:
                if (DBG) log("Ringer mode: vibrate");
                // If the phone is in vibration mode, we only enable the audio when override dnd
                // setting is turned on.
                mEnableAudio = mOverrideDnd;
                break;
            case AudioManager.RINGER_MODE_NORMAL:
            default:
                if (DBG) log("Ringer mode: normal");
                mEnableAudio = true;
                break;
        }

        if (mMessageBody != null && mEnableAudio) {
            if (mTts == null) {
                mTts = new TextToSpeech(this, this);
            } else if (mTtsEngineReady) {
                setTtsLanguage();
            }
        }

        if (mEnableAudio || mEnableVibrate) {
            playAlertTone(mAlertType, mVibrationPattern);
        } else {
            if (DBG) log("No audio/vibrate playing. Stop CellBroadcastAlertAudio service");
            stopSelf();
            return START_NOT_STICKY;
        }

        // Record the initial call state here so that the new alarm has the
        // newest state.
        mInitialCallState = mTelephonyManager.getCallState();

        return START_STICKY;
    }

    // Volume suggested by media team for in-call alarms.
    private static final float IN_CALL_VOLUME_LEFT = 0.125f;
    private static final float IN_CALL_VOLUME_RIGHT = 0.125f;

    /**
     * Start playing the alert sound.
     *
     * @param alertType    the alert type (e.g. default, earthquake, tsunami, etc..)
     * @param patternArray the alert vibration pattern
     */
    private void playAlertTone(AlertType alertType, int[] patternArray) {
        // stop() checks to see if we are already playing.
        stop();

        log("playAlertTone: alertType=" + alertType + ", mEnableVibrate=" + mEnableVibrate
                + ", mEnableAudio=" + mEnableAudio + ", mOverrideDnd=" + mOverrideDnd
                + ", mSubId=" + mSubId);
        Resources res = CellBroadcastSettings.getResources(getApplicationContext(), mSubId);

        // Vibration duration in milliseconds
        long vibrateDuration = 0;

        // Get the alert tone duration. Negative tone duration value means we only play the tone
        // once, not repeat it.
        int customAlertDuration = mAlertDuration;

        // Start the vibration first.
        if (mEnableVibrate) {
            long[] vibrationPattern = new long[patternArray.length];

            for (int i = 0; i < patternArray.length; i++) {
                vibrationPattern[i] = patternArray[i];
                vibrateDuration += patternArray[i];
            }

            AudioAttributes.Builder attrBuilder = new AudioAttributes.Builder();
            attrBuilder.setUsage(AudioAttributes.USAGE_ALARM);
            if (mOverrideDnd) {
                // Set the flags to bypass DnD mode if override dnd is turned on.
                attrBuilder.setFlags(AudioAttributes.FLAG_BYPASS_INTERRUPTION_POLICY
                        | AudioAttributes.FLAG_BYPASS_MUTE);
            }
            AudioAttributes attr = attrBuilder.build();
            // If we only play the tone once, then we also play the vibration pattern once.
            int repeatIndex = (customAlertDuration < 0)
                    ? -1 /* not repeat */ : 0 /* index to repeat */;
            VibrationEffect effect = VibrationEffect.createWaveform(vibrationPattern, repeatIndex);
            log("vibrate: effect=" + effect + ", attr=" + attr + ", duration="
                    + customAlertDuration);
            mVibrator.vibrate(effect, attr);
        }

        if (mEnableLedFlash) {
            log("Start LED flashing");
            mHandler.sendMessage(mHandler.obtainMessage(ALERT_LED_FLASH_TOGGLE, 1, 0));
        }

        if (mEnableAudio) {
            // future optimization: reuse media player object
            mMediaPlayer = new MediaPlayer();
            mMediaPlayer.setOnErrorListener(new OnErrorListener() {
                public boolean onError(MediaPlayer mp, int what, int extra) {
                    loge("Error occurred while playing audio.");
                    mHandler.sendMessage(mHandler.obtainMessage(ALERT_SOUND_FINISHED));
                    return true;
                }
            });

            // If the duration is specified by the config, use the specified duration. Otherwise,
            // just play the alert tone with the tone's duration.
            if (customAlertDuration >= 0) {
                mHandler.sendMessageDelayed(mHandler.obtainMessage(ALERT_SOUND_FINISHED),
                        customAlertDuration);
            } else {
                mMediaPlayer.setOnCompletionListener(new OnCompletionListener() {
                    public void onCompletion(MediaPlayer mp) {
                        if (DBG) log("Audio playback complete.");
                        mHandler.sendMessage(mHandler.obtainMessage(ALERT_SOUND_FINISHED));
                        return;
                    }
                });
            }

            try {
                log("Locale=" + res.getConfiguration().getLocales() + ", alertType=" + alertType);

                // Load the tones based on type
                switch (alertType) {
                    case ETWS_EARTHQUAKE:
                        setDataSourceFromResource(res, mMediaPlayer, R.raw.etws_earthquake);
                        break;
                    case ETWS_TSUNAMI:
                        setDataSourceFromResource(res, mMediaPlayer, R.raw.etws_tsunami);
                        break;
                    case OTHER:
                        setDataSourceFromResource(res, mMediaPlayer, R.raw.etws_other_disaster);
                        break;
                    case ETWS_DEFAULT:
                        setDataSourceFromResource(res, mMediaPlayer, R.raw.etws_default);
                        break;
                    case INFO:
                        setDataSourceFromResource(res, mMediaPlayer, R.raw.info);
                        break;
                    case TEST:
                    case DEFAULT:
                    default:
                        setDataSourceFromResource(res, mMediaPlayer, R.raw.default_tone);
                }

                // Request audio focus (though we're going to play even if we don't get it)
                mAudioManager.requestAudioFocus(null, AudioManager.STREAM_ALARM,
                        AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
                mMediaPlayer.setAudioAttributes(getAlertAudioAttributes());
                setAlertVolume();

                // If we are using the custom alert duration, set looping to true so we can repeat
                // the alert. The tone playing will stop when ALERT_SOUND_FINISHED arrives.
                // Otherwise we just play the alert tone once.
                mMediaPlayer.setLooping(customAlertDuration >= 0);
                mMediaPlayer.prepare();
                mMediaPlayer.start();

            } catch (Exception ex) {
                loge("Failed to play alert sound: " + ex);
                // Immediately move into the next state ALERT_SOUND_FINISHED.
                mHandler.sendMessage(mHandler.obtainMessage(ALERT_SOUND_FINISHED));
            }
        } else {
            // In normal mode (playing tone + vibration), this service will stop after audio
            // playback is done. However, if the device is in vibrate only mode, we need to stop
            // the service right after vibration because there won't be any audio complete callback
            // to stop the service. Unfortunately it's not like MediaPlayer has onCompletion()
            // callback that we can use, we'll have to use our own timer to stop the service.
            mHandler.sendMessageDelayed(mHandler.obtainMessage(ALERT_SOUND_FINISHED),
                    customAlertDuration >= 0 ? customAlertDuration : vibrateDuration);
        }

        mState = STATE_ALERTING;
    }

    private static void setDataSourceFromResource(Resources resources,
            MediaPlayer player, int res) throws java.io.IOException {
        AssetFileDescriptor afd = resources.openRawResourceFd(res);
        if (afd != null) {
            player.setDataSource(afd.getFileDescriptor(), afd.getStartOffset(),
                    afd.getLength());
            afd.close();
        }
    }

    /**
     * Turn on camera's LED
     *
     * @param on {@code true} if turned on, otherwise turned off.
     * @return {@code true} if successful, otherwise false.
     */
    private boolean enableLedFlash(boolean on) {
        log("enbleLedFlash=" + on);
        CameraManager cameraManager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);
        if (cameraManager == null) return false;
        final String[] ids;
        try {
            ids = cameraManager.getCameraIdList();
        } catch (CameraAccessException e) {
            log("Can't get camera id");
            return false;
        }

        boolean success = false;
        for (String id : ids) {
            try {
                CameraCharacteristics c = cameraManager.getCameraCharacteristics(id);
                Boolean flashAvailable = c.get(CameraCharacteristics.FLASH_INFO_AVAILABLE);
                if (flashAvailable != null && flashAvailable) {
                    cameraManager.setTorchMode(id, on);
                    success = true;
                }
            } catch (CameraAccessException e) {
                log("Can't flash. e=" + e);
                // continue with the next available camera
            }
        }
        return success;
    }

    /**
     * Stops alert audio and speech.
     */
    public void stop() {
        if (DBG) log("stop()");

        mHandler.removeMessages(ALERT_SOUND_FINISHED);
        mHandler.removeMessages(ALERT_PAUSE_FINISHED);
        mHandler.removeMessages(ALERT_LED_FLASH_TOGGLE);

        resetAlarmStreamVolume();

        if (mState == STATE_ALERTING) {
            // Stop audio playing
            if (mMediaPlayer != null) {
                try {
                    mMediaPlayer.stop();
                    mMediaPlayer.release();
                } catch (IllegalStateException e) {
                    // catch "Unable to retrieve AudioTrack pointer for stop()" exception
                    loge("exception trying to stop media player");
                }
                mMediaPlayer = null;
            }

            // Stop vibrator
            mVibrator.cancel();
            if (mEnableLedFlash) {
                enableLedFlash(false);
            }
        } else if (mState == STATE_SPEAKING && mTts != null) {
            try {
                mTts.stop();
            } catch (IllegalStateException e) {
                // catch "Unable to retrieve AudioTrack pointer for stop()" exception
                loge("exception trying to stop text-to-speech");
            }
        }
        mState = STATE_IDLE;
    }

    /**
     * Get audio attribute for the alarm.
     */
    private AudioAttributes getAlertAudioAttributes() {
        AudioAttributes.Builder builder = new AudioAttributes.Builder();

        builder.setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION);
        builder.setUsage(AudioAttributes.USAGE_ALARM);
        if (mOverrideDnd) {
            // Set FLAG_BYPASS_INTERRUPTION_POLICY and FLAG_BYPASS_MUTE so that it enables
            // audio in any DnD mode, even in total silence DnD mode (requires MODIFY_PHONE_STATE).
            builder.setFlags(AudioAttributes.FLAG_BYPASS_INTERRUPTION_POLICY
                    | AudioAttributes.FLAG_BYPASS_MUTE);
        }

        return builder.build();
    }

    /**
     * Set volume for alerts.
     */
    private void setAlertVolume() {
        if (mTelephonyManager.getCallState() != TelephonyManager.CALL_STATE_IDLE
                || isOnEarphone()) {
            // If we are in a call, play the alert
            // sound at a low volume to not disrupt the call.
            log("in call: reducing volume");
            mMediaPlayer.setVolume(IN_CALL_VOLUME_LEFT, IN_CALL_VOLUME_RIGHT);
        } else if (mOverrideDnd) {
            // If override DnD is turned on,
            // we overwrite volume setting of STREAM_ALARM to full, play at
            // max possible volume, and reset it after it's finished.
            setAlarmStreamVolumeToFull();
        }
    }

    private boolean isOnEarphone() {
        AudioDeviceInfo[] deviceList = mAudioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);

        for (AudioDeviceInfo devInfo : deviceList) {
            int type = devInfo.getType();
            if (type == AudioDeviceInfo.TYPE_WIRED_HEADSET
                    || type == AudioDeviceInfo.TYPE_WIRED_HEADPHONES
                    || type == AudioDeviceInfo.TYPE_BLUETOOTH_SCO
                    || type == AudioDeviceInfo.TYPE_BLUETOOTH_A2DP) {
                return true;
            }
        }

        return false;
    }

    /**
     * Set volume of STREAM_ALARM to full.
     */
    private void setAlarmStreamVolumeToFull() {
        log("setting alarm volume to full for cell broadcast alerts.");
        int streamType = AudioManager.STREAM_ALARM;
        mUserSetAlarmVolume = mAudioManager.getStreamVolume(streamType);
        mResetAlarmVolumeNeeded = true;
        mAudioManager.setStreamVolume(streamType,
                mAudioManager.getStreamMaxVolume(streamType), 0);
    }

    /**
     * Reset volume of STREAM_ALARM, if needed.
     */
    private void resetAlarmStreamVolume() {
        if (mResetAlarmVolumeNeeded) {
            log("resetting alarm volume to back to " + mUserSetAlarmVolume);
            mAudioManager.setStreamVolume(AudioManager.STREAM_ALARM, mUserSetAlarmVolume, 0);
            mResetAlarmVolumeNeeded = false;
        }
    }

    private static void log(String msg) {
        Log.d(TAG, msg);
    }

    private static void loge(String msg) {
        Log.e(TAG, msg);
    }
}
