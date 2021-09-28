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

package com.android.tv;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Intent;
import android.media.tv.TvInputInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.MainThread;
import android.util.Log;

import com.android.tv.common.CommonConstants;
import com.android.tv.common.SoftPreconditions;
import com.android.tv.common.actions.InputSetupActionUtils;
import com.android.tv.data.ChannelDataManager;
import com.android.tv.data.ChannelDataManager.Listener;
import com.android.tv.data.epg.EpgFetcher;
import com.android.tv.data.epg.EpgInputWhiteList;
import com.android.tv.features.TvFeatures;
import com.android.tv.util.SetupUtils;
import com.android.tv.util.TvInputManagerHelper;
import com.android.tv.util.Utils;

import com.google.android.tv.partner.support.EpgContract;

import dagger.android.AndroidInjection;
import dagger.android.ContributesAndroidInjector;

import java.util.concurrent.TimeUnit;

import javax.inject.Inject;

/**
 * An activity to launch a TV input setup activity.
 *
 * <p>After setup activity is finished, all channels will be browsable.
 */
public class SetupPassthroughActivity extends Activity {
    private static final String TAG = "SetupPassthroughAct";
    private static final boolean DEBUG = false;

    private static final int REQUEST_START_SETUP_ACTIVITY = 200;

    private static ScanTimeoutMonitor sScanTimeoutMonitor;

    private TvInputInfo mTvInputInfo;
    private Intent mActivityAfterCompletion;
    private boolean mEpgFetcherDuringScan;
    @Inject EpgInputWhiteList mEpgInputWhiteList;
    @Inject TvInputManagerHelper mInputManager;
    @Inject SetupUtils mSetupUtils;
    @Inject ChannelDataManager mChannelDataManager;
    @Inject EpgFetcher mEpgFetcher;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        if (DEBUG) Log.d(TAG, "onCreate");
        AndroidInjection.inject(this);
        super.onCreate(savedInstanceState);
        Intent intent = getIntent();
        String inputId = intent.getStringExtra(InputSetupActionUtils.EXTRA_INPUT_ID);
        mTvInputInfo = mInputManager.getTvInputInfo(inputId);
        mActivityAfterCompletion = InputSetupActionUtils.getExtraActivityAfter(intent);
        boolean needToFetchEpg =
                mTvInputInfo != null && Utils.isInternalTvInput(this, mTvInputInfo.getId());
        if (needToFetchEpg) {
            // In case when the activity is restored, this flag should be restored as well.
            mEpgFetcherDuringScan = true;
        }
        if (savedInstanceState == null) {
            SoftPreconditions.checkArgument(
                    InputSetupActionUtils.hasInputSetupAction(intent),
                    TAG,
                    "Unsupported action %s",
                    intent.getAction());
            if (DEBUG) Log.d(TAG, "TvInputId " + inputId + " / TvInputInfo " + mTvInputInfo);
            if (mTvInputInfo == null) {
                Log.w(TAG, "There is no input with the ID " + inputId + ".");
                finish();
                return;
            }
            if (intent.getExtras() == null) {
                Log.w(TAG, "There is no extra info in the intent");
                finish();
                return;
            }
            Intent setupIntent = InputSetupActionUtils.getExtraSetupIntent(intent);
            if (DEBUG) Log.d(TAG, "Setup activity launch intent: " + setupIntent);
            if (setupIntent == null) {
                Log.w(TAG, "The input (" + mTvInputInfo.getId() + ") doesn't have setup.");
                finish();
                return;
            }
            SetupUtils.grantEpgPermission(this, mTvInputInfo.getServiceInfo().packageName);
            if (DEBUG) Log.d(TAG, "Activity after completion " + mActivityAfterCompletion);
            // If EXTRA_SETUP_INTENT is not removed, an infinite recursion happens during
            // setupIntent.putExtras(intent.getExtras()).
            Bundle extras = intent.getExtras();
            InputSetupActionUtils.removeSetupIntent(extras);
            setupIntent.putExtras(extras);
            try {
                ComponentName callingActivity = getCallingActivity();
                if (callingActivity != null
                        && !callingActivity.getPackageName().equals(CommonConstants.BASE_PACKAGE)) {
                    Log.w(
                            TAG,
                            "Calling activity "
                                    + callingActivity.getPackageName()
                                    + " is not trusted. Not forwarding intent.");
                    finish();
                    return;
                }
                startActivityForResult(setupIntent, REQUEST_START_SETUP_ACTIVITY);
            } catch (ActivityNotFoundException e) {
                Log.e(TAG, "Can't find activity: " + setupIntent.getComponent());
                finish();
                return;
            }
            if (needToFetchEpg) {
                if (sScanTimeoutMonitor == null) {
                    sScanTimeoutMonitor = new ScanTimeoutMonitor(mEpgFetcher, mChannelDataManager);
                }
                sScanTimeoutMonitor.startMonitoring();
                mEpgFetcher.onChannelScanStarted();
            }
        }
    }

    @Override
    public void onActivityResult(int requestCode, final int resultCode, final Intent data) {
        if (DEBUG)
            Log.d(TAG, "onActivityResult(" + requestCode + ",  " + resultCode + ",  " + data + ")");
        if (sScanTimeoutMonitor != null) {
            sScanTimeoutMonitor.stopMonitoring();
        }
        // Note: It's not guaranteed that this method is always called after scanning.
        boolean setupComplete =
                requestCode == REQUEST_START_SETUP_ACTIVITY && resultCode == Activity.RESULT_OK;
        // Tells EpgFetcher that channel source setup is finished.

        if (mEpgFetcherDuringScan) {
            mEpgFetcher.onChannelScanFinished();
        }
        if (!setupComplete) {
            setResult(resultCode, data);
            finish();
            return;
        }
        if (TvFeatures.CLOUD_EPG_FOR_3RD_PARTY.isEnabled(this)
                && data != null
                && data.getBooleanExtra(EpgContract.EXTRA_USE_CLOUD_EPG, false)) {
            if (DEBUG) Log.d(TAG, "extra " + data.getExtras());
            String inputId = data.getStringExtra(TvInputInfo.EXTRA_INPUT_ID);
            if (mEpgInputWhiteList.isInputWhiteListed(inputId)) {
                mEpgFetcher.fetchImmediately();
            }
        }

        if (mTvInputInfo == null) {
            Log.w(
                    TAG,
                    "There is no input with ID "
                            + getIntent().getStringExtra(InputSetupActionUtils.EXTRA_INPUT_ID)
                            + ".");
            setResult(resultCode, data);
            finish();
            return;
        }
        mSetupUtils.onTvInputSetupFinished(
                mTvInputInfo.getId(),
                () -> {
                    if (mActivityAfterCompletion != null) {
                        try {
                            startActivity(mActivityAfterCompletion);
                        } catch (ActivityNotFoundException e) {
                            Log.w(TAG, "Activity launch failed", e);
                        }
                    }
                    setResult(resultCode, data);
                    finish();
                });
    }

    /**
     * Monitors the scan progress and notifies the timeout of the scanning. The purpose of this
     * monitor is to call EpgFetcher.onChannelScanFinished() in case when
     * SetupPassthroughActivity.onActivityResult() is not called properly. b/36008534
     */
    @MainThread
    private static class ScanTimeoutMonitor {
        // Set timeout long enough. The message in Sony TV says the scanning takes about 30 minutes.
        private static final long SCAN_TIMEOUT_MS = TimeUnit.MINUTES.toMillis(30);

        private final EpgFetcher mEpgFetcher;
        private final ChannelDataManager mChannelDataManager;
        private final Handler mHandler = new Handler(Looper.getMainLooper());
        private final Runnable mScanTimeoutRunnable =
                () -> {
                    Log.w(
                            TAG,
                            "No channels has been added for a while."
                                    + " The scan might have finished unexpectedly.");
                    onScanTimedOut();
                };
        private final Listener mChannelDataManagerListener =
                new Listener() {
                    @Override
                    public void onLoadFinished() {
                        setupTimer();
                    }

                    @Override
                    public void onChannelListUpdated() {
                        setupTimer();
                    }

                    @Override
                    public void onChannelBrowsableChanged() {}
                };
        private boolean mStarted;

        private ScanTimeoutMonitor(EpgFetcher epgFetcher, ChannelDataManager mChannelDataManager) {
            mEpgFetcher = epgFetcher;
            this.mChannelDataManager = mChannelDataManager;
        }

        private void startMonitoring() {
            if (!mStarted) {
                mStarted = true;
                mChannelDataManager.addListener(mChannelDataManagerListener);
            }
            if (mChannelDataManager.isDbLoadFinished()) {
                setupTimer();
            }
        }

        private void stopMonitoring() {
            if (mStarted) {
                mStarted = false;
                mHandler.removeCallbacks(mScanTimeoutRunnable);
                mChannelDataManager.removeListener(mChannelDataManagerListener);
            }
        }

        private void setupTimer() {
            mHandler.removeCallbacks(mScanTimeoutRunnable);
            mHandler.postDelayed(mScanTimeoutRunnable, SCAN_TIMEOUT_MS);
        }

        private void onScanTimedOut() {
            stopMonitoring();
            mEpgFetcher.onChannelScanFinished();
        }
    }

    /** Exports {@link MainActivity} for Dagger codegen to create the appropriate injector. */
    @dagger.Module
    public abstract static class Module {
        @ContributesAndroidInjector
        abstract SetupPassthroughActivity contributesSetupPassthroughActivity();
    }
}
