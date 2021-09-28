/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.car.developeroptions.biometrics.face;

import static android.app.Activity.RESULT_OK;

import static com.android.car.developeroptions.biometrics.BiometricEnrollBase.CONFIRM_REQUEST;
import static com.android.car.developeroptions.biometrics.BiometricEnrollBase.RESULT_FINISHED;

import android.app.settings.SettingsEnums;
import android.content.Context;
import android.content.Intent;
import android.hardware.face.FaceManager;
import android.os.Bundle;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.SearchIndexableResource;
import android.util.Log;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.SettingsActivity;
import com.android.car.developeroptions.Utils;
import com.android.car.developeroptions.dashboard.DashboardFragment;
import com.android.car.developeroptions.password.ChooseLockSettingsHelper;
import com.android.car.developeroptions.search.BaseSearchIndexProvider;
import com.android.settingslib.core.AbstractPreferenceController;
import com.android.settingslib.core.lifecycle.Lifecycle;
import com.android.settingslib.search.SearchIndexable;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Settings screen for face authentication.
 */
@SearchIndexable
public class FaceSettings extends DashboardFragment {

    private static final String TAG = "FaceSettings";
    private static final String KEY_TOKEN = "key_token";

    private UserManager mUserManager;
    private FaceManager mFaceManager;
    private int mUserId;
    private byte[] mToken;
    private FaceSettingsAttentionPreferenceController mAttentionController;
    private FaceSettingsRemoveButtonPreferenceController mRemoveController;
    private List<AbstractPreferenceController> mControllers;

    private final FaceSettingsRemoveButtonPreferenceController.Listener mRemovalListener = () -> {
        if (getActivity() != null) {
            getActivity().finish();
        }
    };

    public static boolean isAvailable(Context context) {
        FaceManager manager = Utils.getFaceManagerOrNull(context);
        return manager != null && manager.isHardwareDetected();
    }

    @Override
    public int getMetricsCategory() {
        return SettingsEnums.FACE;
    }

    @Override
    protected int getPreferenceScreenResId() {
        return R.xml.security_settings_face;
    }

    @Override
    protected String getLogTag() {
        return TAG;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putByteArray(KEY_TOKEN, mToken);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mUserManager = getPrefContext().getSystemService(UserManager.class);
        mFaceManager = getPrefContext().getSystemService(FaceManager.class);
        mUserId = getActivity().getIntent().getIntExtra(
                Intent.EXTRA_USER_ID, UserHandle.myUserId());

        // There is no better way to do this :/
        for (AbstractPreferenceController controller : mControllers) {
            if (controller instanceof  FaceSettingsPreferenceController) {
                ((FaceSettingsPreferenceController) controller).setUserId(mUserId);
            }
        }
        mRemoveController.setUserId(mUserId);

        // Don't show keyguard controller for work profile settings.
        if (mUserManager.isManagedProfile(mUserId)) {
            removePreference(FaceSettingsKeyguardPreferenceController.KEY);
        }

        if (savedInstanceState != null) {
            mToken = savedInstanceState.getByteArray(KEY_TOKEN);
        }

        if (mToken == null) {
            final long challenge = mFaceManager.generateChallenge();
            ChooseLockSettingsHelper helper = new ChooseLockSettingsHelper(getActivity(), this);
            if (!helper.launchConfirmationActivity(CONFIRM_REQUEST,
                    getString(R.string.security_settings_face_preference_title),
                    null, null, challenge, mUserId)) {
                Log.e(TAG, "Password not set");
                finish();
            }
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mToken != null) {
            mAttentionController.setToken(mToken);
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == CONFIRM_REQUEST) {
            if (resultCode == RESULT_FINISHED || resultCode == RESULT_OK) {
                mFaceManager.setActiveUser(mUserId);
                // The pin/pattern/password was set.
                if (data != null) {
                    mToken = data.getByteArrayExtra(
                            ChooseLockSettingsHelper.EXTRA_KEY_CHALLENGE_TOKEN);
                    if (mToken != null) {
                        mAttentionController.setToken(mToken);
                    }
                }
            }
        }

        if (mToken == null) {
            // Didn't get an authentication, finishing
            getActivity().finish();
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (getActivity().isFinishing()) {
            final int result = mFaceManager.revokeChallenge();
            if (result < 0) {
                Log.w(TAG, "revokeChallenge failed, result: " + result);
            }
        }
    }

    @Override
    protected List<AbstractPreferenceController> createPreferenceControllers(Context context) {
        if (!isAvailable(context)) {
            return null;
        }
        mControllers = buildPreferenceControllers(context, getSettingsLifecycle());
        // There's no great way of doing this right now :/
        for (AbstractPreferenceController controller : mControllers) {
            if (controller instanceof FaceSettingsAttentionPreferenceController) {
                mAttentionController = (FaceSettingsAttentionPreferenceController) controller;
            } else if (controller instanceof FaceSettingsRemoveButtonPreferenceController) {
                mRemoveController = (FaceSettingsRemoveButtonPreferenceController) controller;
                mRemoveController.setListener(mRemovalListener);
                mRemoveController.setActivity((SettingsActivity) getActivity());
            }
        }

        return mControllers;
    }

    private static List<AbstractPreferenceController> buildPreferenceControllers(Context context,
            Lifecycle lifecycle) {
        final List<AbstractPreferenceController> controllers = new ArrayList<>();
        controllers.add(new FaceSettingsVideoPreferenceController(context));
        controllers.add(new FaceSettingsKeyguardPreferenceController(context));
        controllers.add(new FaceSettingsAppPreferenceController(context));
        controllers.add(new FaceSettingsAttentionPreferenceController(context));
        controllers.add(new FaceSettingsRemoveButtonPreferenceController(context));
        controllers.add(new FaceSettingsFooterPreferenceController(context));
        controllers.add(new FaceSettingsConfirmPreferenceController(context));
        return controllers;
    }

    public static final SearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider() {
                @Override
                public List<SearchIndexableResource> getXmlResourcesToIndex(
                        Context context, boolean enabled) {
                    final SearchIndexableResource sir = new SearchIndexableResource(context);
                    sir.xmlResId = R.xml.security_settings_face;
                    return Arrays.asList(sir);
                }

                @Override
                public List<AbstractPreferenceController> createPreferenceControllers(
                        Context context) {
                    if (isAvailable(context)) {
                        return buildPreferenceControllers(context, null /* lifecycle */);
                    } else {
                        return null;
                    }
                }

                @Override
                protected boolean isPageSearchEnabled(Context context) {
                    return isAvailable(context);
                }
            };

}
