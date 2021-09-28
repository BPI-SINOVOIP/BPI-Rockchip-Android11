/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.google.android.car.kitchensink.audio;

import android.car.Car;
import android.car.CarAppFocusManager;
import android.car.CarAppFocusManager.OnAppFocusChangedListener;
import android.car.CarAppFocusManager.OnAppFocusOwnershipCallback;
import android.car.media.CarAudioManager;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.AudioManager.OnAudioFocusChangeListener;
import android.media.HwAudioSource;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

import androidx.fragment.app.Fragment;

import com.google.android.car.kitchensink.CarEmulator;
import com.google.android.car.kitchensink.R;

import java.util.ArrayList;
import java.util.List;

import javax.annotation.concurrent.GuardedBy;

public class AudioTestFragment extends Fragment {
    private static final String TAG = "CAR.AUDIO.KS";
    private static final boolean DBG = true;

    // Key for communicating to hall which audio zone has been selected to play
    private static final String AAE_PARAMETER_KEY_FOR_SELECTED_ZONE =
            "com.android.car.emulator.selected_zone";

    private AudioManager mAudioManager;
    private FocusHandler mAudioFocusHandler;
    private ToggleButton mEnableMocking;

    private AudioPlayer mMusicPlayer;
    @GuardedBy("mLock")
    private AudioPlayer mMusicPlayerWithDelayedFocus;
    private AudioPlayer mMusicPlayerShort;
    private AudioPlayer mNavGuidancePlayer;
    private AudioPlayer mPhoneAudioPlayer;
    private AudioPlayer mVrPlayer;
    private AudioPlayer mSystemPlayer;
    private AudioPlayer mWavPlayer;
    private AudioPlayer mMusicPlayerForSelectedDeviceAddress;
    private HwAudioSource mHwAudioSource;
    private AudioPlayer[] mAllPlayers;

    private Handler mHandler;
    private Context mContext;

    private Car mCar;
    private CarAppFocusManager mAppFocusManager;
    private AudioAttributes mMusicAudioAttrib;
    private AudioAttributes mNavAudioAttrib;
    private AudioAttributes mPhoneAudioAttrib;
    private AudioAttributes mVrAudioAttrib;
    private AudioAttributes mRadioAudioAttrib;
    private AudioAttributes mSystemSoundAudioAttrib;
    private AudioAttributes mMusicAudioAttribForDeviceAddress;
    private CarEmulator mCarEmulator;
    private CarAudioManager mCarAudioManager;
    private Spinner mZoneSpinner;
    ArrayAdapter<Integer> mZoneAdapter;
    private Spinner mDeviceAddressSpinner;
    ArrayAdapter<CarAudioZoneDeviceInfo> mDeviceAddressAdapter;
    private LinearLayout mDeviceAddressLayout;

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private AudioFocusRequest mDelayedFocusRequest;
    private OnAudioFocusChangeListener mMediaWithDelayedFocusListener;
    private TextView mDelayedStatusText;

    private final OnAudioFocusChangeListener mNavFocusListener = (focusChange) -> {
        Log.i(TAG, "Nav focus change:" + focusChange);
    };
    private final OnAudioFocusChangeListener mVrFocusListener = (focusChange) -> {
        Log.i(TAG, "VR focus change:" + focusChange);
    };
    private final OnAudioFocusChangeListener mRadioFocusListener = (focusChange) -> {
        Log.i(TAG, "Radio focus change:" + focusChange);
    };

    private final CarAppFocusManager.OnAppFocusOwnershipCallback mOwnershipCallbacks =
            new OnAppFocusOwnershipCallback() {
                @Override
                public void onAppFocusOwnershipLost(int focus) {
                }
                @Override
                public void onAppFocusOwnershipGranted(int focus) {
                }
    };

    private void connectCar() {
        mContext = getContext();
        mHandler = new Handler(Looper.getMainLooper());
        mCar = Car.createCar(mContext, /* handler= */ null,
                Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER, (car, ready) -> {
                    if (!ready) {
                        return;
                    }
                    mAppFocusManager =
                            (CarAppFocusManager) car.getCarManager(Car.APP_FOCUS_SERVICE);
                    OnAppFocusChangedListener listener = new OnAppFocusChangedListener() {
                        @Override
                        public void onAppFocusChanged(int appType, boolean active) {
                        }
                    };
                    mAppFocusManager.addFocusListener(listener,
                            CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
                    mAppFocusManager.addFocusListener(listener,
                            CarAppFocusManager.APP_FOCUS_TYPE_VOICE_COMMAND);

                    mCarAudioManager = (CarAudioManager) car.getCarManager(Car.AUDIO_SERVICE);

                    handleSetUpZoneSelection();

                    setUpDeviceAddressPlayer();
                });
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle bundle) {
        Log.i(TAG, "onCreateView");
        View view = inflater.inflate(R.layout.audio, container, false);
        //Zone Spinner
        setUpZoneSpinnerView(view);

        // Device Address layout
        setUpDeviceAddressLayoutView(view);

        connectCar();
        initializePlayers();

        TextView currentZoneIdTextView = view.findViewById(R.id.activity_current_zone);
        setActivityCurrentZoneId(currentZoneIdTextView);

        mAudioManager = (AudioManager) mContext.getSystemService(
                Context.AUDIO_SERVICE);
        mAudioFocusHandler = new FocusHandler(
                view.findViewById(R.id.button_focus_request_selection),
                view.findViewById(R.id.button_audio_focus_request),
                view.findViewById(R.id.text_audio_focus_state));
        view.findViewById(R.id.button_media_play_start).setOnClickListener(v -> {
            boolean requestFocus = true;
            boolean repeat = true;
            mMusicPlayer.start(requestFocus, repeat, AudioManager.AUDIOFOCUS_GAIN);
        });
        view.findViewById(R.id.button_media_play_once).setOnClickListener(v -> {
            mMusicPlayerShort.start(true, false, AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
            // play only for 1 sec and stop
            mHandler.postDelayed(() -> mMusicPlayerShort.stop(), 1000);
        });
        view.findViewById(R.id.button_media_play_stop).setOnClickListener(v -> mMusicPlayer.stop());
        view.findViewById(R.id.button_wav_play_start).setOnClickListener(
                v -> mWavPlayer.start(true, true, AudioManager.AUDIOFOCUS_GAIN));
        view.findViewById(R.id.button_wav_play_stop).setOnClickListener(v -> mWavPlayer.stop());
        view.findViewById(R.id.button_nav_play_once).setOnClickListener(v -> {
            if (mAppFocusManager == null) {
                Log.e(TAG, "mAppFocusManager is null");
                return;
            }
            if (DBG) {
                Log.i(TAG, "Nav start");
            }
            mAppFocusManager.requestAppFocus(
                    CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION, mOwnershipCallbacks);
            if (!mNavGuidancePlayer.isPlaying()) {
                mNavGuidancePlayer.start(true, false,
                        AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK,
                        () -> mAppFocusManager.abandonAppFocus(mOwnershipCallbacks,
                                CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION));
            }
        });
        view.findViewById(R.id.button_vr_play_once).setOnClickListener(v -> {
            if (mAppFocusManager == null) {
                Log.e(TAG, "mAppFocusManager is null");
                return;
            }
            if (DBG) {
                Log.i(TAG, "VR start");
            }
            mAppFocusManager.requestAppFocus(
                    CarAppFocusManager.APP_FOCUS_TYPE_VOICE_COMMAND, mOwnershipCallbacks);
            if (!mVrPlayer.isPlaying()) {
                mVrPlayer.start(true, false,
                        AudioManager.AUDIOFOCUS_GAIN_TRANSIENT,
                        () -> mAppFocusManager.abandonAppFocus(mOwnershipCallbacks,
                                CarAppFocusManager.APP_FOCUS_TYPE_VOICE_COMMAND));
            }
        });
        view.findViewById(R.id.button_system_play_once).setOnClickListener(v -> {
            if (DBG) {
                Log.i(TAG, "System start");
            }
            if (!mSystemPlayer.isPlaying()) {
                // system sound played without focus
                mSystemPlayer.start(false, false, 0);
            }
        });
        view.findViewById(R.id.button_nav_start).setOnClickListener(v -> handleNavStart());
        view.findViewById(R.id.button_nav_end).setOnClickListener(v -> handleNavEnd());
        view.findViewById(R.id.button_vr_start).setOnClickListener(v -> handleVrStart());
        view.findViewById(R.id.button_vr_end).setOnClickListener(v -> handleVrEnd());
        view.findViewById(R.id.button_radio_start).setOnClickListener(v -> handleRadioStart());
        view.findViewById(R.id.button_radio_end).setOnClickListener(v -> handleRadioEnd());
        view.findViewById(R.id.button_speaker_phone_on).setOnClickListener(
                v -> mAudioManager.setSpeakerphoneOn(true));
        view.findViewById(R.id.button_speaker_phone_off).setOnClickListener(
                v -> mAudioManager.setSpeakerphoneOn(false));
        view.findViewById(R.id.button_microphone_on).setOnClickListener(
                v -> mAudioManager.setMicrophoneMute(false));
        view.findViewById(R.id.button_microphone_off).setOnClickListener(
                v -> mAudioManager.setMicrophoneMute(true));
        final View hwAudioSourceNotFound = view.findViewById(R.id.hw_audio_source_not_found);
        final View hwAudioSourceStart = view.findViewById(R.id.hw_audio_source_start);
        final View hwAudioSourceStop = view.findViewById(R.id.hw_audio_source_stop);
        if (mHwAudioSource == null) {
            hwAudioSourceNotFound.setVisibility(View.VISIBLE);
            hwAudioSourceStart.setVisibility(View.GONE);
            hwAudioSourceStop.setVisibility(View.GONE);
        } else {
            hwAudioSourceNotFound.setVisibility(View.GONE);
            hwAudioSourceStart.setVisibility(View.VISIBLE);
            hwAudioSourceStop.setVisibility(View.VISIBLE);
            view.findViewById(R.id.hw_audio_source_start).setOnClickListener(
                    v -> handleHwAudioSourceStart());
            view.findViewById(R.id.hw_audio_source_stop).setOnClickListener(
                    v -> handleHwAudioSourceStop());
        }

        mEnableMocking = view.findViewById(R.id.button_mock_audio);
        mEnableMocking.setOnCheckedChangeListener((buttonView, isChecked) -> {
            if (mCarEmulator == null) {
                //TODO(pavelm): need to do a full switch between emulated and normal mode
                // all Car*Manager references should be invalidated.
                Toast.makeText(AudioTestFragment.this.getContext(),
                        "Not supported yet :(", Toast.LENGTH_SHORT).show();
                return;
            }
            if (isChecked) {
                mCarEmulator.start();
            } else {
                mCarEmulator.stop();
                mCarEmulator = null;
            }
        });

        // Manage buttons for audio player for device address
        view.findViewById(R.id.button_device_media_play_start).setOnClickListener(v -> {
            startDeviceAudio();
        });
        view.findViewById(R.id.button_device_media_play_once).setOnClickListener(v -> {
            startDeviceAudio();
            // play only for 1 sec and stop
            mHandler.postDelayed(() -> mMusicPlayerForSelectedDeviceAddress.stop(), 1000);
        });
        view.findViewById(R.id.button_device_media_play_stop)
                .setOnClickListener(v -> mMusicPlayerForSelectedDeviceAddress.stop());

        view.findViewById(R.id.media_delayed_focus_start)
                .setOnClickListener(v -> handleDelayedMediaStart());
        view.findViewById(R.id.media_delayed_focus_stop)
                .setOnClickListener(v -> handleDelayedMediaStop());

        view.findViewById(R.id.phone_audio_focus_start)
                .setOnClickListener(v -> mPhoneAudioPlayer.start(true, true,
                        AudioManager.AUDIOFOCUS_GAIN_TRANSIENT));
        view.findViewById(R.id.phone_audio_focus_stop)
                .setOnClickListener(v -> mPhoneAudioPlayer.stop());

        mDelayedStatusText = view.findViewById(R.id.media_delayed_player_status);

        return view;
    }

    @Override
    public void onDestroyView() {
        Log.i(TAG, "onDestroyView");
        if (mCarEmulator != null) {
            mCarEmulator.stop();
        }
        for (AudioPlayer p : mAllPlayers) {
            p.stop();
        }
        handleHwAudioSourceStop();
        if (mAudioFocusHandler != null) {
            mAudioFocusHandler.release();
            mAudioFocusHandler = null;
        }
        if (mAppFocusManager != null) {
            mAppFocusManager.abandonAppFocus(mOwnershipCallbacks);
        }
        if (mCar != null && mCar.isConnected()) {
            mCar.disconnect();
            mCar = null;
        }
        super.onDestroyView();
    }

    private void initializePlayers() {
        mMusicAudioAttrib = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .build();
        mNavAudioAttrib = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE)
                .build();
        mPhoneAudioAttrib = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                .build();
        mVrAudioAttrib = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_ASSISTANT)
                .build();
        mRadioAudioAttrib = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .build();
        mSystemSoundAudioAttrib = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_ASSISTANCE_SONIFICATION)
                .build();
        // Create an audio device address audio attribute
        mMusicAudioAttribForDeviceAddress = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .build();


        mMusicPlayerForSelectedDeviceAddress = new AudioPlayer(mContext, R.raw.well_worth_the_wait,
                mMusicAudioAttribForDeviceAddress);
        mMusicPlayer = new AudioPlayer(mContext, R.raw.well_worth_the_wait,
                mMusicAudioAttrib);
        mMusicPlayerWithDelayedFocus = new AudioPlayer(mContext, R.raw.well_worth_the_wait,
                mMusicAudioAttrib);
        mMusicPlayerShort = new AudioPlayer(mContext, R.raw.ring_classic_01,
                mMusicAudioAttrib);
        mNavGuidancePlayer = new AudioPlayer(mContext, R.raw.turnright,
                mNavAudioAttrib);
        mPhoneAudioPlayer = new AudioPlayer(mContext, R.raw.free_flight,
                mPhoneAudioAttrib);
        mVrPlayer = new AudioPlayer(mContext, R.raw.one2six,
                mVrAudioAttrib);
        mSystemPlayer = new AudioPlayer(mContext, R.raw.ring_classic_01,
                mSystemSoundAudioAttrib);
        mWavPlayer = new AudioPlayer(mContext, R.raw.free_flight,
                mMusicAudioAttrib);
        final AudioDeviceInfo tuner = findTunerDevice(mContext);
        if (tuner != null) {
            mHwAudioSource = new HwAudioSource.Builder()
                    .setAudioAttributes(mMusicAudioAttrib)
                    .setAudioDeviceInfo(findTunerDevice(mContext))
                    .build();
        }
        mAllPlayers = new AudioPlayer[] {
                mMusicPlayer,
                mMusicPlayerShort,
                mNavGuidancePlayer,
                mVrPlayer,
                mSystemPlayer,
                mWavPlayer,
                mMusicPlayerWithDelayedFocus
        };
    }

    private void setActivityCurrentZoneId(TextView currentZoneIdTextView) {
        if (mCarAudioManager.isDynamicRoutingEnabled()) {
            try {
                ApplicationInfo info = mContext.getPackageManager().getApplicationInfo(
                        mContext.getPackageName(), 0);
                int audioZoneId = mCarAudioManager.getZoneIdForUid(info.uid);
                currentZoneIdTextView.setText(Integer.toString(audioZoneId));
            } catch (PackageManager.NameNotFoundException e) {
                Log.e(TAG, "setActivityCurrentZoneId Failed to find name: " , e);
            }
        }
    }

    private void handleDelayedMediaStart() {
        synchronized (mLock) {
            if (mDelayedFocusRequest != null) {
                return;
            }
            mMediaWithDelayedFocusListener = new MediaWithDelayedFocusListener();
            mDelayedFocusRequest = new AudioFocusRequest
                    .Builder(AudioManager.AUDIOFOCUS_GAIN)
                    .setAudioAttributes(mMusicAudioAttrib)
                    .setOnAudioFocusChangeListener(mMediaWithDelayedFocusListener)
                    .setForceDucking(false)
                    .setWillPauseWhenDucked(false)
                    .setAcceptsDelayedFocusGain(true)
                    .build();
            int delayedFocusRequestResults = mAudioManager.requestAudioFocus(mDelayedFocusRequest);
            if (delayedFocusRequestResults == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                startDelayedMediaPlayerLocked();
                return;
            }
            if (delayedFocusRequestResults == AudioManager.AUDIOFOCUS_REQUEST_DELAYED) {
                if (DBG) Log.d(TAG, "Media With Delayed Focus delayed focus granted");
                mDelayedStatusText.setText(R.string.player_delayed);
                return;
            }
            mMediaWithDelayedFocusListener = null;
            mDelayedFocusRequest = null;
            mDelayedStatusText.setText(R.string.player_not_started);
        }
        if (DBG) Log.d(TAG, "Media With Delayed Focus focus rejected");
    }

    private void startDelayedMediaPlayerLocked() {
        if (!mMusicPlayerWithDelayedFocus.isPlaying()) {
            if (DBG) Log.d(TAG, "Media With Delayed Focus starting player");
            mMusicPlayerWithDelayedFocus.start(false, true,
                    AudioManager.AUDIOFOCUS_GAIN);
            mDelayedStatusText.setText(R.string.player_started);
            return;
        }
        if (DBG) Log.d(TAG, "Media With Delayed Focus player already started");
    }

    private void handleDelayedMediaStop() {
        synchronized (mLock) {
            if (mDelayedFocusRequest != null)  {
                int requestResults = mAudioManager.abandonAudioFocusRequest(mDelayedFocusRequest);
                if (DBG) {
                    Log.d(TAG, "Media With Delayed Focus abandon focus " + requestResults);
                }
                mDelayedFocusRequest = null;
                mMediaWithDelayedFocusListener = null;
                stopDelayedMediaPlayerLocked();
            }
        }
    }

    private void stopDelayedMediaPlayerLocked() {
        mDelayedStatusText.setText(R.string.player_not_started);
        if (mMusicPlayerWithDelayedFocus.isPlaying()) {
            if (DBG) Log.d(TAG, "Media With Delayed Focus stopping player");
            mMusicPlayerWithDelayedFocus.stop();
            return;
        }
        if (DBG) Log.d(TAG, "Media With Delayed Focus already stopped");
    }

    private void pauseDelayedMediaPlayerLocked() {
        mDelayedStatusText.setText(R.string.player_paused);
        if (mMusicPlayerWithDelayedFocus.isPlaying()) {
            if (DBG) Log.d(TAG, "Media With Delayed Focus pausing player");
            mMusicPlayerWithDelayedFocus.stop();
            return;
        }
        if (DBG) Log.d(TAG, "Media With Delayed Focus already stopped");
    }

    private void setUpDeviceAddressLayoutView(View view) {
        mDeviceAddressLayout = view.findViewById(R.id.audio_select_device_address_layout);

        mDeviceAddressSpinner = view.findViewById(R.id.device_address_spinner);
        mDeviceAddressSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
                handleDeviceAddressSelection();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
    }

    private void setUpZoneSpinnerView(View view) {
        mZoneSpinner = view.findViewById(R.id.zone_spinner);
        mZoneSpinner.setEnabled(false);
        mZoneSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
                handleZoneSelection();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
    }

    public void handleZoneSelection() {
        int position = mZoneSpinner.getSelectedItemPosition();
        int zone = mZoneAdapter.getItem(position);
        Log.d(TAG, "Zone Selected: " + zone);
        if (Build.IS_EMULATOR && zone != CarAudioManager.PRIMARY_AUDIO_ZONE) {
            setZoneToPlayOnSpeaker(zone);
        }
    }

    private void handleSetUpZoneSelection() {
        if (!Build.IS_EMULATOR || !mCarAudioManager.isDynamicRoutingEnabled()) {
            return;
        }
        //take care of zone selection
        List<Integer> zoneList = mCarAudioManager.getAudioZoneIds();
        Integer[] zoneArray = zoneList.stream()
                .filter(i -> i != CarAudioManager.PRIMARY_AUDIO_ZONE).toArray(Integer[]::new);
        mZoneAdapter = new ArrayAdapter<>(mContext,
                android.R.layout.simple_spinner_item, zoneArray);
        mZoneAdapter.setDropDownViewResource(
                android.R.layout.simple_spinner_dropdown_item);
        mZoneSpinner.setAdapter(mZoneAdapter);
        mZoneSpinner.setEnabled(true);
    }

    private void handleNavStart() {
        if (mAppFocusManager == null) {
            Log.e(TAG, "mAppFocusManager is null");
            return;
        }
        if (DBG) {
            Log.i(TAG, "Nav start");
        }
        mAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mOwnershipCallbacks);
        mAudioManager.requestAudioFocus(mNavFocusListener, mNavAudioAttrib,
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, 0);
    }

    private void handleNavEnd() {
        if (mAppFocusManager == null) {
            Log.e(TAG, "mAppFocusManager is null");
            return;
        }
        if (DBG) {
            Log.i(TAG, "Nav end");
        }
        mAudioManager.abandonAudioFocus(mNavFocusListener, mNavAudioAttrib);
        mAppFocusManager.abandonAppFocus(mOwnershipCallbacks,
                CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
    }

    private AudioDeviceInfo findTunerDevice(Context context) {
        AudioManager am = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        AudioDeviceInfo[] devices = am.getDevices(AudioManager.GET_DEVICES_INPUTS);
        for (AudioDeviceInfo device : devices) {
            if (device.getType() == AudioDeviceInfo.TYPE_FM_TUNER) {
                return device;
            }
        }
        return null;
    }

    private void handleHwAudioSourceStart() {
        if (mHwAudioSource != null) {
            mHwAudioSource.start();
        }
    }

    private void handleHwAudioSourceStop() {
        if (mHwAudioSource != null) {
            mHwAudioSource.stop();
        }
    }

    private void handleVrStart() {
        if (mAppFocusManager == null) {
            Log.e(TAG, "mAppFocusManager is null");
            return;
        }
        if (DBG) {
            Log.i(TAG, "VR start");
        }
        mAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_VOICE_COMMAND,
                mOwnershipCallbacks);
        mAudioManager.requestAudioFocus(mVrFocusListener, mVrAudioAttrib,
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT, 0);
    }

    private void handleVrEnd() {
        if (mAppFocusManager == null) {
            Log.e(TAG, "mAppFocusManager is null");
            return;
        }
        if (DBG) {
            Log.i(TAG, "VR end");
        }
        mAudioManager.abandonAudioFocus(mVrFocusListener, mVrAudioAttrib);
        mAppFocusManager.abandonAppFocus(mOwnershipCallbacks,
                CarAppFocusManager.APP_FOCUS_TYPE_VOICE_COMMAND);
    }

    private void handleRadioStart() {
        if (DBG) {
            Log.i(TAG, "Radio start");
        }
        mAudioManager.requestAudioFocus(mRadioFocusListener, mRadioAudioAttrib,
                AudioManager.AUDIOFOCUS_GAIN, 0);
    }

    private void handleRadioEnd() {
        if (DBG) {
            Log.i(TAG, "Radio end");
        }
        mAudioManager.abandonAudioFocus(mRadioFocusListener, mRadioAudioAttrib);
    }

    private void setUpDeviceAddressPlayer() {
        if (!mCarAudioManager.isDynamicRoutingEnabled()) {
            mDeviceAddressLayout.setVisibility(View.GONE);
            return;
        }
        mDeviceAddressLayout.setVisibility(View.VISIBLE);
        List<CarAudioZoneDeviceInfo> deviceList = new ArrayList<>();
        for (int audioZoneId: mCarAudioManager.getAudioZoneIds()) {
            AudioDeviceInfo deviceInfo = mCarAudioManager
                    .getOutputDeviceForUsage(audioZoneId, AudioAttributes.USAGE_MEDIA);
            CarAudioZoneDeviceInfo carAudioZoneDeviceInfo = new CarAudioZoneDeviceInfo();
            carAudioZoneDeviceInfo.mDeviceInfo = deviceInfo;
            carAudioZoneDeviceInfo.mAudioZoneId = audioZoneId;
            deviceList.add(carAudioZoneDeviceInfo);
            if (DBG) {
                Log.d(TAG, "Found device address"
                        + carAudioZoneDeviceInfo.mDeviceInfo.getAddress()
                        + " for audio zone id " + audioZoneId);
            }

        }

        CarAudioZoneDeviceInfo[] deviceArray =
                deviceList.stream().toArray(CarAudioZoneDeviceInfo[]::new);
        mDeviceAddressAdapter = new ArrayAdapter<>(mContext,
                android.R.layout.simple_spinner_item, deviceArray);
        mDeviceAddressAdapter.setDropDownViewResource(
                android.R.layout.simple_spinner_dropdown_item);
        mDeviceAddressSpinner.setAdapter(mDeviceAddressAdapter);
        createDeviceAddressAudioPlayer();
    }

    private void createDeviceAddressAudioPlayer() {
        CarAudioZoneDeviceInfo carAudioZoneDeviceInfo = mDeviceAddressAdapter.getItem(
                mDeviceAddressSpinner.getSelectedItemPosition());
        Log.d(TAG, "Setting Bundle to zone " + carAudioZoneDeviceInfo.mAudioZoneId);
        Bundle bundle = new Bundle();
        bundle.putInt(CarAudioManager.AUDIOFOCUS_EXTRA_REQUEST_ZONE_ID,
                carAudioZoneDeviceInfo.mAudioZoneId);
        mMusicAudioAttribForDeviceAddress = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .addBundle(bundle)
                .build();

        mMusicPlayerForSelectedDeviceAddress = new AudioPlayer(mContext,
                R.raw.well_worth_the_wait,
                mMusicAudioAttribForDeviceAddress,
                carAudioZoneDeviceInfo.mDeviceInfo);
    }

    private void startDeviceAudio() {
        Log.d(TAG, "Starting device address audio");
        mMusicPlayerForSelectedDeviceAddress.start(true, false,
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
    }

    public void handleDeviceAddressSelection() {
        if (mMusicPlayerForSelectedDeviceAddress != null
                && mMusicPlayerForSelectedDeviceAddress.isPlaying()) {
            mMusicPlayerForSelectedDeviceAddress.stop();
        }
        createDeviceAddressAudioPlayer();
    }

    /**
     * Sets the left speaker to output sound from zoneId
     * @param zoneId zone id to set left speakers output
     * @Note this should only be used with emulator where the zones are separated into right
     * and left speaker, other platforms would have real devices where audio is routed.
     */
    private void setZoneToPlayOnSpeaker(int zoneId) {
        String selectedZoneKeyValueString = AAE_PARAMETER_KEY_FOR_SELECTED_ZONE + "=" + zoneId;
        // send key value  parameter list to audio HAL
        mAudioManager.setParameters(selectedZoneKeyValueString);
        Log.d(TAG, "setZoneToPlayOnSpeaker : " + zoneId);
    }


    private class FocusHandler {
        private static final String AUDIO_FOCUS_STATE_GAIN = "gain";
        private static final String AUDIO_FOCUS_STATE_RELEASED_UNKNOWN = "released / unknown";

        private final RadioGroup mRequestSelection;
        private final TextView mText;
        private final AudioFocusListener mFocusListener;
        private AudioFocusRequest mFocusRequest;

        public FocusHandler(RadioGroup radioGroup, Button requestButton, TextView text) {
            mText = text;
            mRequestSelection = radioGroup;
            mRequestSelection.check(R.id.focus_gain);
            setFocusText(AUDIO_FOCUS_STATE_RELEASED_UNKNOWN);
            mFocusListener = new AudioFocusListener();
            requestButton.setOnClickListener(v -> {
                int selectedButtonId = mRequestSelection.getCheckedRadioButtonId();
                int focusRequest;
                switch (selectedButtonId) {
                    case R.id.focus_gain:
                        focusRequest = AudioManager.AUDIOFOCUS_GAIN;
                        break;
                    case R.id.focus_gain_transient:
                        focusRequest = AudioManager.AUDIOFOCUS_GAIN_TRANSIENT;
                        break;
                    case R.id.focus_gain_transient_duck:
                        focusRequest = AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK;
                        break;
                    case R.id.focus_gain_transient_exclusive:
                        focusRequest = AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE;
                        break;
                    case R.id.focus_release:
                    default:
                        abandonAudioFocus();
                        return;
                }
                mFocusRequest = new AudioFocusRequest.Builder(focusRequest)
                        .setAudioAttributes(new AudioAttributes.Builder()
                                .setUsage(AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE)
                                .build())
                        .setOnAudioFocusChangeListener(mFocusListener)
                        .build();
                int ret = mAudioManager.requestAudioFocus(mFocusRequest);
                Log.i(TAG, "requestAudioFocus returned " + ret);
                if (ret == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                    setFocusText(AUDIO_FOCUS_STATE_GAIN);
                }
            });
        }

        public void release() {
            abandonAudioFocus();
        }

        private void abandonAudioFocus() {
            if (DBG) {
                Log.i(TAG, "abandonAudioFocus");
            }
            if (mFocusRequest != null) {
                mAudioManager.abandonAudioFocusRequest(mFocusRequest);
                mFocusRequest = null;
            } else {
                Log.i(TAG, "mFocusRequest is already null");
            }
            setFocusText(AUDIO_FOCUS_STATE_RELEASED_UNKNOWN);
        }

        private void setFocusText(String msg) {
            mText.setText("focus state:" + msg);
        }

        private class AudioFocusListener implements OnAudioFocusChangeListener {
            @Override
            public void onAudioFocusChange(int focusChange) {
                Log.i(TAG, "onAudioFocusChange " + focusChange);
                if (focusChange == AudioManager.AUDIOFOCUS_GAIN) {
                    setFocusText(AUDIO_FOCUS_STATE_GAIN);
                } else if (focusChange == AudioManager.AUDIOFOCUS_LOSS) {
                    setFocusText("loss");
                } else if (focusChange == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT) {
                    setFocusText("loss,transient");
                } else if (focusChange == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK) {
                    setFocusText("loss,transient,duck");
                }
            }
        }
    }

    private final class MediaWithDelayedFocusListener implements OnAudioFocusChangeListener {
        @Override
        public void onAudioFocusChange(int focusChange) {
            if (DBG) Log.d(TAG, "Media With Delayed Focus focus change:" + focusChange);
            synchronized (mLock) {
                switch (focusChange) {
                    case AudioManager.AUDIOFOCUS_GAIN:
                    case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT:
                    case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE:
                    case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK:
                        startDelayedMediaPlayerLocked();
                        break;
                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                        pauseDelayedMediaPlayerLocked();
                        break;
                    case AudioManager.AUDIOFOCUS_LOSS:
                    default:
                        stopDelayedMediaPlayerLocked();
                        mDelayedFocusRequest = null;
                        mMediaWithDelayedFocusListener = null;
                        break;
                }
            }
        }
    }

    private class CarAudioZoneDeviceInfo {
        AudioDeviceInfo mDeviceInfo;
        int mAudioZoneId;

        @Override
        public String toString() {
            StringBuilder builder = new StringBuilder();
            builder.append("Device Address : ");
            builder.append(mDeviceInfo.getAddress());
            builder.append(", Audio Zone Id: ");
            builder.append(mAudioZoneId);
            return builder.toString();
        }
    }
}
