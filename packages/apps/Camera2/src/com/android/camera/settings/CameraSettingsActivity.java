/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.camera.settings;

import android.Manifest;
import android.app.ActionBar;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.os.Bundle;
import android.os.Environment;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;
import android.preference.PreferenceGroup;
import android.preference.PreferenceScreen;
import androidx.fragment.app.FragmentActivity;
import android.view.MenuItem;
import android.widget.Toast;

import com.android.camera.FatalErrorHandler;
import com.android.camera.FatalErrorHandlerImpl;
import com.android.camera.Storage;
import com.android.camera.app.CameraServicesImpl;
import com.android.camera.debug.DebugPropertyHelper;
import com.android.camera.debug.Log;
import com.android.camera.device.CameraId;
import com.android.camera.exif.Rational;
import com.android.camera.one.OneCamera.Facing;
import com.android.camera.one.OneCamera;
import com.android.camera.one.OneCameraAccessException;
import com.android.camera.one.OneCameraCharacteristics;
import com.android.camera.one.OneCameraException;
import com.android.camera.one.OneCameraManager;
import com.android.camera.one.OneCameraModule;
import com.android.camera.settings.PictureSizeLoader.PictureSizes;
import com.android.camera.settings.SettingsUtil.SelectedVideoQualities;
import com.android.camera.util.CameraSettingsActivityHelper;
import com.android.camera.util.GoogleHelpHelper;
import com.android.camera.util.Size;
import com.android.camera2.R;
import com.android.ex.camera2.portability.CameraAgentFactory;
import com.android.ex.camera2.portability.CameraDeviceInfo;

import java.io.File;
import java.text.DecimalFormat;
import java.util.ArrayList;
import java.util.List;

/**
 * Provides the settings UI for the Camera app.
 */
public class CameraSettingsActivity extends FragmentActivity {

    /**
     * Used to denote a subsection of the preference tree to display in the
     * Fragment. For instance, if 'Advanced' key is provided, the advanced
     * preference section will be treated as the root for display. This is used
     * to enable activity transitions between preference sections, and allows
     * back/up stack to operate correctly.
     */
    public static final String PREF_SCREEN_EXTRA = "pref_screen_extra";
    public static final String HIDE_ADVANCED_SCREEN = "hide_advanced";
    private static final int PERMISSION_REQUEST_CODE = 1;
    public static final String HIDE_EX_SCREEN = "hide_ex";
    public static final String HIDE_WB_SCREEN = "hide_wb";
    private OneCameraManager mOneCameraManager;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        FatalErrorHandler fatalErrorHandler = new FatalErrorHandlerImpl(this);
        boolean hideAdvancedScreen = false;
        boolean hideExposureScreen = false;
        boolean hideWbScreen = false;

        try {
            mOneCameraManager = OneCameraModule.provideOneCameraManager();
        } catch (OneCameraException e) {
            // Log error and continue. Modules requiring OneCamera should check
            // and handle if null by showing error dialog or other treatment.
            fatalErrorHandler.onGenericCameraAccessFailure();
        }

        // Check if manual exposure is available, so we can decide whether to
        // display Advanced screen.
        try {
            CameraId frontCameraId = mOneCameraManager.findFirstCameraFacing(Facing.FRONT);
            CameraId backCameraId = mOneCameraManager.findFirstCameraFacing(Facing.BACK);

            // The exposure compensation is supported when both of the following conditions meet
            //   - we have the valid camera, and
            //   - the valid camera supports the exposure compensation
            boolean isExposureCompensationSupportedByFrontCamera = (frontCameraId != null) &&
                    (mOneCameraManager.getOneCameraCharacteristics(frontCameraId)
                            .isExposureCompensationSupported());
            boolean isExposureCompensationSupportedByBackCamera = (backCameraId != null) &&
                    (mOneCameraManager.getOneCameraCharacteristics(backCameraId)
                            .isExposureCompensationSupported());
            boolean isWbSupportedByFrontCamera = (frontCameraId != null) &&
                    (mOneCameraManager.getOneCameraCharacteristics(frontCameraId)
                            .isWhiteBalanceSupported());
            boolean isWbSupportedByBackCamera = (backCameraId != null) &&
                    (mOneCameraManager.getOneCameraCharacteristics(backCameraId)
                            .isWhiteBalanceSupported());
            

            // Hides the option if neither front and back camera support exposure compensation.
            if (!isExposureCompensationSupportedByFrontCamera &&
                    !isExposureCompensationSupportedByBackCamera
                    && !isWbSupportedByBackCamera && !isWbSupportedByFrontCamera) {
                hideAdvancedScreen = true;
            }
            
            if (!isExposureCompensationSupportedByFrontCamera
                    && !isExposureCompensationSupportedByBackCamera)
                hideExposureScreen = true;
            
            if (!isWbSupportedByBackCamera && !isWbSupportedByFrontCamera)
                hideWbScreen = true;
        } catch (OneCameraAccessException e) {
            fatalErrorHandler.onGenericCameraAccessFailure();
        }

        ActionBar actionBar = getActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);
        actionBar.setTitle(R.string.mode_settings);

        String prefKey = getIntent().getStringExtra(PREF_SCREEN_EXTRA);
        CameraSettingsFragment dialog = new CameraSettingsFragment();
        Bundle bundle = new Bundle(1);
        bundle.putString(PREF_SCREEN_EXTRA, prefKey);
        bundle.putBoolean(HIDE_ADVANCED_SCREEN, hideAdvancedScreen);
        bundle.putBoolean(HIDE_EX_SCREEN, hideExposureScreen);
        bundle.putBoolean(HIDE_WB_SCREEN, hideWbScreen);
        dialog.setArguments(bundle);
        getFragmentManager().beginTransaction().replace(android.R.id.content, dialog).commit();
    }

    @Override
    public boolean onMenuItemSelected(int featureId, MenuItem item) {
        int itemId = item.getItemId();
        if (itemId == android.R.id.home) {
            finish();
            return true;
        }
        return true;
    }

    public static class CameraSettingsFragment extends PreferenceFragment implements
            OnSharedPreferenceChangeListener {

        public static final String PREF_CATEGORY_RESOLUTION = "pref_category_resolution";
        public static final String PREF_CATEGORY_ADVANCED = "pref_category_advanced";
        public static final String PREF_LAUNCH_HELP = "pref_launch_help";
        public static final String PREF_RESTORE_SETTINGS = "pref_restore_settings";
        private static final Log.Tag TAG = new Log.Tag("SettingsFragment");
        private static DecimalFormat sMegaPixelFormat = new DecimalFormat("##0.0");
        private String[] mCamcorderProfileNames;
        private CameraDeviceInfo mInfos;
        private String mPrefKey;
        private boolean mHideAdvancedScreen;
        private boolean mHideExScreen;
        private boolean mHideWbScreen;
        private boolean mGetSubPrefAsRoot = true;

        // Selected resolutions for the different cameras and sizes.
        private PictureSizes mPictureSizes;

        private SettingsManager mSettingsManager;
        private ManagedSwitchPreference mCameraSound;
        private ManagedSwitchPreference mLocation;
        private ManagedSwitchPreference mPreviewFullSize;
        private ManagedSwitchPreference mFaceDetection;

        private String[] mAntiBandingNames;
        private boolean mFaceDetectionSupported;

        private String SDCARD;
        private String FLASH;

        private OneCameraManager mOneCameraManager;

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            Bundle arguments = getArguments();
            if (arguments != null) {
                mPrefKey = arguments.getString(PREF_SCREEN_EXTRA);
                mHideAdvancedScreen = arguments.getBoolean(HIDE_ADVANCED_SCREEN);
                mHideExScreen = arguments.getBoolean(HIDE_EX_SCREEN);
                mHideWbScreen = arguments.getBoolean(HIDE_WB_SCREEN);
            }
            Context context = this.getActivity().getApplicationContext();
            mInfos = CameraAgentFactory
                    .getAndroidCameraAgent(context, CameraAgentFactory.CameraApi.API_1)
                    .getCameraDeviceInfo();
            loadSizes();

            mCamcorderProfileNames = getResources().getStringArray(R.array.camcorder_profile_names);
            mAntiBandingNames = getResources().getStringArray(R.array.pref_antibanding_names);

            SDCARD = getActivity().getResources()
                    .getString(R.string.pref_media_save_path_external_sd);
            FLASH = getActivity().getResources()
                    .getString(R.string.pref_media_save_path_flash);
            
            addPreferencesFromResource(R.xml.camera_preferences);

            // Put in the summaries for the currently set values.
            final PreferenceScreen resolutionScreen =
                    (PreferenceScreen) findPreference(PREF_CATEGORY_RESOLUTION);
            fillEntriesAndSummaries(resolutionScreen);
            //setEntries(findPreference(Keys.KEY_ANTIBANDING));
            //setSummary(findPreference(Keys.KEY_ANTIBANDING));
            setSummary(findPreference(Keys.KEY_MEDIA_SAVE_PATH));

            PreferenceScreen advancedScreen =
                    (PreferenceScreen) findPreference(PREF_CATEGORY_ADVANCED);

            // If manual exposure not enabled, hide the Advanced screen.
            if (mHideAdvancedScreen) {
                PreferenceScreen root = (PreferenceScreen) findPreference("prefscreen_top");
                root.removePreference(advancedScreen);
            } else {
                if (mHideExScreen)
                    advancedScreen.removePreference(findPreference(Keys.KEY_EXPOSURE_COMPENSATION_ENABLED));
                if (mHideWbScreen)
                    advancedScreen.removePreference(findPreference(Keys.KEY_WHITEBALANCE_ENABLED));
            }

            FatalErrorHandler fatalErrorHandler = new FatalErrorHandlerImpl(this.getActivity());
            try {
                mOneCameraManager = OneCameraModule.provideOneCameraManager();
            } catch (OneCameraException e) {
                // Log error and continue. Modules requiring OneCamera should check
                // and handle if null by showing error dialog or other treatment.
                fatalErrorHandler.onGenericCameraAccessFailure();
            }
            mSettingsManager = CameraServicesImpl.instance().getSettingsManager();

            // Allow the Helper to edit the full preference hierarchy, not the
            // sub tree we may show as root. See {@link #getPreferenceScreen()}.
            mGetSubPrefAsRoot = false;
            CameraSettingsActivityHelper.addAdditionalPreferences(this, context);
            mGetSubPrefAsRoot = true;

            /*mCamcorderProfileNames = getResources().getStringArray(R.array.camcorder_profile_names);
            mAntiBandingNames = getResources().getStringArray(R.array.pref_antibanding_names);
            mInfos = CameraAgentFactory
                    .getAndroidCameraAgent(context, CameraAgentFactory.CameraApi.API_1)
                    .getCameraDeviceInfo();

            SDCARD = getActivity().getResources()
                    .getString(R.string.pref_media_save_path_external_sd);
            FLASH = getActivity().getResources()
                    .getString(R.string.pref_media_save_path_flash);*/
        }

        @Override
        public void onResume() {
            super.onResume();
            final Activity activity = this.getActivity();

            // Load the camera sizes.
            loadSizes();

            // Send loaded sizes to additional preferences.
            CameraSettingsActivityHelper.onSizesLoaded(this, mPictureSizes.backCameraSizes,
                    new ListPreferenceFiller() {
                        @Override
                        public void fill(List<Size> sizes, ListPreference preference) {
                            setEntriesForSelection(sizes, preference);
                        }
                    });

            // Make sure to hide settings for cameras that don't exist on this
            // device.
            setVisibilities();

            final MyDialogPreference restoreSettings =
                    (MyDialogPreference) findPreference(PREF_RESTORE_SETTINGS);
            restoreSettings.setPositiveButtonListener(new DialogInterface.OnClickListener() {
                
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    // TODO Auto-generated method stub
                    Keys.setToDefaults(mSettingsManager, getActivity().getApplicationContext());
                    setDefaultPicturesize(mOneCameraManager.findFirstCameraFacing(Facing.BACK), OneCamera.Facing.BACK);
                    setDefaultPicturesize(mOneCameraManager.findFirstCameraFacing(Facing.FRONT), OneCamera.Facing.FRONT);
                    restoreUI();
                }
            });
            mCameraSound = (ManagedSwitchPreference) findPreference(Keys.KEY_CAMERA_SOUND);
            mLocation = (ManagedSwitchPreference) findPreference(Keys.KEY_RECORD_LOCATION);
            //mPreviewFullSize = (ManagedSwitchPreference) findPreference(Keys.KEY_PREVIEW_FULL_SIZE_ENABLE);
            mFaceDetection = (ManagedSwitchPreference) findPreference(Keys.KEY_FACE_DETECTION_ENABLED);
            if (!mFaceDetectionSupported && mFaceDetection != null) {
                getPreferenceScreen().removePreference(mFaceDetection);
            }

            // Put in the summaries for the currently set values.
            final PreferenceScreen resolutionScreen =
                    (PreferenceScreen) findPreference(PREF_CATEGORY_RESOLUTION);
            fillEntriesAndSummaries(resolutionScreen);
            //setEntries(findPreference(Keys.KEY_ANTIBANDING));
            //setSummary(findPreference(Keys.KEY_ANTIBANDING));
            setSummary(findPreference(Keys.KEY_MEDIA_SAVE_PATH));
            setPreferenceScreenIntent(resolutionScreen);

            final PreferenceScreen advancedScreen =
                    (PreferenceScreen) findPreference(PREF_CATEGORY_ADVANCED);

            if (!mHideAdvancedScreen) {
                setPreferenceScreenIntent(advancedScreen);
            }

            /*Preference helpPref = findPreference(PREF_LAUNCH_HELP);
            helpPref.setOnPreferenceClickListener(
                    new OnPreferenceClickListener() {
                        @Override
                        public boolean onPreferenceClick(Preference preference) {
                            new GoogleHelpHelper(activity).launchGoogleHelp();
                            return true;
                        }
                    });*/
            getPreferenceScreen().getSharedPreferences()
                    .registerOnSharedPreferenceChangeListener(this);

            registerMediaMountListener();
        }

        private void restoreUI() {
            mCameraSound.setChecked(true);
            mLocation.setChecked(false);
            //mPreviewFullSize.setChecked(DebugPropertyHelper.isPreviewFullSizeOn());
            if (mFaceDetectionSupported && mFaceDetection != null)
                mFaceDetection.setChecked(true);
			restoreMediaSavePath();
        }

        private void setDefaultPicturesize(CameraId cameraId, Facing cameraFacing) {
            if (cameraId == null) return;
            final String pictureSizeSettingKey = cameraFacing == OneCamera.Facing.FRONT ?
                    Keys.KEY_PICTURE_SIZE_FRONT : Keys.KEY_PICTURE_SIZE_BACK;
            final Rational aspectRatio = ResolutionUtil.ASPECT_RATIO_4x3;

            FatalErrorHandler fatalErrorHandler = new FatalErrorHandlerImpl(this.getActivity());
            try {
                OneCameraCharacteristics cameraCharacteristics =
                        mOneCameraManager.getOneCameraCharacteristics(cameraId);
    
                final List<Size> supportedPictureSizes =
                        ResolutionUtil.filterBlackListedSizes(
                                cameraCharacteristics.getSupportedPictureSizes(ImageFormat.JPEG),
                                "");
                final Size fallbackPictureSize =
                        ResolutionUtil.getLargestPictureSize(aspectRatio, supportedPictureSizes);
                mSettingsManager.set(
                        SettingsManager.SCOPE_GLOBAL,
                        pictureSizeSettingKey,
                        SettingsUtil.sizeToSettingString(fallbackPictureSize));
                Log.d(TAG, "cameraId" + cameraId.getValue() + " setDefaultPicturesize = " + fallbackPictureSize);
            } catch (OneCameraAccessException e) {
                fatalErrorHandler.onGenericCameraAccessFailure();
            }
        }

        @Override
        public void onDestroy() {
            // TODO Auto-generated method stub
            super.onDestroy();
            mSettingsManager = null;
        }

        /**
         * Configure home-as-up for sub-screens.
         */
        private void setPreferenceScreenIntent(final PreferenceScreen preferenceScreen) {
            Intent intent = new Intent(getActivity(), CameraSettingsActivity.class);
            intent.putExtra(PREF_SCREEN_EXTRA, preferenceScreen.getKey());
            preferenceScreen.setIntent(intent);
        }

        /**
         * This override allows the CameraSettingsFragment to be reused for
         * different nested PreferenceScreens within the single camera
         * preferences XML resource. If the fragment is constructed with a
         * desired preference key (delivered via an extra in the creation
         * intent), it is used to look up the nested PreferenceScreen and
         * returned here.
         */
        @Override
        public PreferenceScreen getPreferenceScreen() {
            PreferenceScreen root = super.getPreferenceScreen();
            if (!mGetSubPrefAsRoot || mPrefKey == null || root == null) {
                return root;
            } else {
                PreferenceScreen match = findByKey(root, mPrefKey);
                if (match != null) {
                    return match;
                } else {
                    throw new RuntimeException("key " + mPrefKey + " not found");
                }
            }
        }

        private PreferenceScreen findByKey(PreferenceScreen parent, String key) {
            if (key.equals(parent.getKey())) {
                return parent;
            } else {
                for (int i = 0; i < parent.getPreferenceCount(); i++) {
                    Preference child = parent.getPreference(i);
                    if (child instanceof PreferenceScreen) {
                        PreferenceScreen match = findByKey((PreferenceScreen) child, key);
                        if (match != null) {
                            return match;
                        }
                    }
                }
                return null;
            }
        }

        /**
         * Depending on camera availability on the device, this removes settings
         * for cameras the device doesn't have.
         */
        private void setVisibilities() {
            PreferenceGroup resolutions =
                    (PreferenceGroup) findPreference(PREF_CATEGORY_RESOLUTION);
            if (mPictureSizes.backCameraSizes.isEmpty()) {
                recursiveDelete(resolutions,
                        findPreference(Keys.KEY_PICTURE_SIZE_BACK));
                recursiveDelete(resolutions,
                        findPreference(Keys.KEY_VIDEO_QUALITY_BACK));
            }
            if (mPictureSizes.frontCameraSizes.isEmpty()) {
                recursiveDelete(resolutions,
                        findPreference(Keys.KEY_PICTURE_SIZE_FRONT));
                recursiveDelete(resolutions,
                        findPreference(Keys.KEY_VIDEO_QUALITY_FRONT));
            }
        }

        /**
         * Recursively go through settings and fill entries and summaries of our
         * preferences.
         */
        private void fillEntriesAndSummaries(PreferenceGroup group) {
            for (int i = 0; i < group.getPreferenceCount(); ++i) {
                Preference pref = group.getPreference(i);
                if (pref instanceof PreferenceGroup) {
                    fillEntriesAndSummaries((PreferenceGroup) pref);
                }
                setSummary(pref);
                setEntries(pref);
            }
        }

        /**
         * Recursively traverses the tree from the given group as the route and
         * tries to delete the preference. Traversal stops once the preference
         * was found and removed.
         */
        private boolean recursiveDelete(PreferenceGroup group, Preference preference) {
            if (group == null) {
                Log.d(TAG, "attempting to delete from null preference group");
                return false;
            }
            if (preference == null) {
                Log.d(TAG, "attempting to delete null preference");
                return false;
            }
            if (group.removePreference(preference)) {
                // Removal was successful.
                return true;
            }

            for (int i = 0; i < group.getPreferenceCount(); ++i) {
                Preference pref = group.getPreference(i);
                if (pref instanceof PreferenceGroup) {
                    if (recursiveDelete((PreferenceGroup) pref, preference)) {
                        return true;
                    }
                }
            }
            return false;
        }

        @Override
        public void onPause() {
            super.onPause();
            getPreferenceScreen().getSharedPreferences()
                    .unregisterOnSharedPreferenceChangeListener(this);
            unregisterMediaMountListener();
        }

        @Override
        public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
            setSummary(findPreference(key));
            if (key.equals(Keys.KEY_RECORD_LOCATION)
                    && sharedPreferences.getString(key, "0").equals("1")) {
                Context context = this.getActivity().getApplicationContext();
                if (context.checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
                        != PackageManager.PERMISSION_GRANTED) {
                    requestPermissions(new String[] {Manifest.permission.ACCESS_COARSE_LOCATION},
                        PERMISSION_REQUEST_CODE);
                }
            }
        }

        /**
         * Set the entries for the given preference. The given preference needs
         * to be a {@link ListPreference}
         */
        private void setEntries(Preference preference) {
            if (!(preference instanceof ListPreference)) {
                return;
            }

            ListPreference listPreference = (ListPreference) preference;
            if (listPreference.getKey().equals(Keys.KEY_PICTURE_SIZE_BACK)) {
                setEntriesForSelection(mPictureSizes.backCameraSizes, listPreference);
            } else if (listPreference.getKey().equals(Keys.KEY_PICTURE_SIZE_FRONT)) {
                setEntriesForSelection(mPictureSizes.frontCameraSizes, listPreference);
            } else if (listPreference.getKey().equals(Keys.KEY_VIDEO_QUALITY_BACK)) {
                setEntriesForSelection(mPictureSizes.videoQualitiesBack.orNull(), listPreference);
            } else if (listPreference.getKey().equals(Keys.KEY_VIDEO_QUALITY_FRONT)) {
                setEntriesForSelection(mPictureSizes.videoQualitiesFront.orNull(), listPreference);
            } else if (listPreference.getKey().equals(Keys.KEY_ANTIBANDING)) {
                listPreference.setEntries(mAntiBandingNames);
            }
        }

        /**
         * Set the summary for the given preference. The given preference needs
         * to be a {@link ListPreference}.
         */
        private void setSummary(Preference preference) {
            if (!(preference instanceof ListPreference)) {
                return;
            }

            ListPreference listPreference = (ListPreference) preference;
            if (listPreference.getKey().equals(Keys.KEY_PICTURE_SIZE_BACK)) {
                setSummaryForSelection(mPictureSizes.backCameraSizes,
                        listPreference);
            } else if (listPreference.getKey().equals(Keys.KEY_PICTURE_SIZE_FRONT)) {
                setSummaryForSelection(mPictureSizes.frontCameraSizes,
                        listPreference);
            } else if (listPreference.getKey().equals(Keys.KEY_VIDEO_QUALITY_BACK)) {
                setSummaryForSelection(mPictureSizes.videoQualitiesBack.orNull(), listPreference);
            } else if (listPreference.getKey().equals(Keys.KEY_VIDEO_QUALITY_FRONT)) {
                setSummaryForSelection(mPictureSizes.videoQualitiesFront.orNull(), listPreference);
            } else if (listPreference.getKey().equals(Keys.KEY_MEDIA_SAVE_PATH)) {
                listPreference.setSummary(listPreference.getEntry());
                String value = listPreference.getValue();
                if (FLASH.equals(value))
                    Storage.DIRECTORY = Storage.DEFAULT_DIRECTORY;
                else if (SDCARD.equals(value)) {
                    String state = Environment.getStorageState(new File(Storage.EXTENAL_SD));
                    Log.i(TAG,"getSecondVolumeStorageState = " + state);
                    if (!Environment.MEDIA_MOUNTED.equalsIgnoreCase(state)) {
                        mSettingsManager.setToDefault(SettingsManager.SCOPE_GLOBAL, Keys.KEY_MEDIA_SAVE_PATH);
                        restoreMediaSavePath();
                        Toast.makeText(getActivity(), R.string.external_sd_unmounted, Toast.LENGTH_SHORT).show();
                    } else {
                        Storage.DIRECTORY = Storage.EXTERNAL_DIRECTORY;
                    }
                }
                Log.i(TAG, "setSummary Storage.DIRECTORY = " + Storage.DIRECTORY);
            } else {
                listPreference.setSummary(listPreference.getEntry());
            }
        }

        /**
         * Sets the entries for the given list preference.
         *
         * @param selectedSizes The possible S,M,L entries the user can choose
         *            from.
         * @param preference The preference to set the entries for.
         */
        private void setEntriesForSelection(List<Size> selectedSizes,
                ListPreference preference) {
            if (selectedSizes == null) {
                return;
            }

            String[] entries = new String[selectedSizes.size()];
            String[] entryValues = new String[selectedSizes.size()];
            for (int i = 0; i < selectedSizes.size(); i++) {
                Size size = selectedSizes.get(i);
                entries[i] = getSizeSummaryString(size);
                entryValues[i] = SettingsUtil.sizeToSettingString(size);
            }
            preference.setEntries(entries);
            preference.setEntryValues(entryValues);
        }

        /**
         * Sets the entries for the given list preference.
         *
         * @param selectedQualities The possible S,M,L entries the user can
         *            choose from.
         * @param preference The preference to set the entries for.
         */
        private void setEntriesForSelection(SelectedVideoQualities selectedQualities,
                ListPreference preference) {
            if (selectedQualities == null) {
                return;
            }

            // Avoid adding double entries at the bottom of the list which
            // indicates that not at least 3 qualities are supported.
            ArrayList<String> entries = new ArrayList<String>();
            entries.add(mCamcorderProfileNames[selectedQualities.large]);
            if (selectedQualities.medium != selectedQualities.large) {
                entries.add(mCamcorderProfileNames[selectedQualities.medium]);
            }
            if (selectedQualities.small != selectedQualities.medium) {
                entries.add(mCamcorderProfileNames[selectedQualities.small]);
            }
            preference.setEntries(entries.toArray(new String[0]));
        }

        /**
         * Sets the summary for the given list preference.
         *
         * @param displayableSizes The human readable preferred sizes
         * @param preference The preference for which to set the summary.
         */
        private void setSummaryForSelection(List<Size> displayableSizes,
                                            ListPreference preference) {
            String setting = preference.getValue();
            if (setting == null || !setting.contains("x")) {
                return;
            }
            Size settingSize = SettingsUtil.sizeFromSettingString(setting);
            if (settingSize == null || settingSize.area() == 0) {
                return;
            }
            preference.setSummary(getSizeSummaryString(settingSize));
        }

        /**
         * Sets the summary for the given list preference.
         *
         * @param selectedQualities The selected video qualities.
         * @param preference The preference for which to set the summary.
         */
        private void setSummaryForSelection(SelectedVideoQualities selectedQualities,
                ListPreference preference) {
            if (selectedQualities == null) {
                return;
            }

            int selectedQuality = selectedQualities.getFromSetting(preference.getValue());
            preference.setSummary(mCamcorderProfileNames[selectedQuality]);
        }

        /**
         * This method gets the selected picture sizes for S,M,L and populates
         * {@link #mPictureSizes} accordingly.
         */
        private void loadSizes() {
            if (mInfos == null) {
                Log.w(TAG, "null deviceInfo, cannot display resolution sizes");
                return;
            }
            PictureSizeLoader loader = new PictureSizeLoader(getActivity().getApplicationContext());
            mPictureSizes = loader.computePictureSizes();

            // Back camera.
            boolean backCameraSupportFaceDetection = false;
            int backCameraId = SettingsUtil.getCameraId(mInfos, SettingsUtil.CAMERA_FACING_BACK);
            if (backCameraId >= 0) {
                backCameraSupportFaceDetection = CameraPictureSizesCacher.isFaceDetectionSupported(backCameraId,
                        this.getActivity().getApplicationContext());
            }
            // Front camera.
            boolean frontCameraSupportFaceDetection = false;
            int frontCameraId = SettingsUtil.getCameraId(mInfos, SettingsUtil.CAMERA_FACING_FRONT);
            if (frontCameraId >= 0) {
                frontCameraSupportFaceDetection = CameraPictureSizesCacher.isFaceDetectionSupported(frontCameraId,
                        this.getActivity().getApplicationContext());
            }
            if (backCameraSupportFaceDetection || frontCameraSupportFaceDetection)
                mFaceDetectionSupported = true;
            else
                mFaceDetectionSupported = false;
        }

        /**
         * @param size The photo resolution.
         * @return A human readable and translated string for labeling the
         *         picture size in megapixels.
         */
        private String getSizeSummaryString(Size size) {
            Size approximateSize = ResolutionUtil.getApproximateSize(size);
            String megaPixels = sMegaPixelFormat.format((size.width() * size.height()) / 1e6);
            int numerator = ResolutionUtil.aspectRatioNumerator(approximateSize);
            int denominator = ResolutionUtil.aspectRatioDenominator(approximateSize);
            String result = getResources().getString(
                    R.string.setting_summary_aspect_ratio_and_megapixels, numerator, denominator,
                    megaPixels);
            return result;
        }

        private void registerMediaMountListener() {
            IntentFilter filter = new IntentFilter();
            filter.addAction(Intent.ACTION_MEDIA_REMOVED);
            filter.addAction(Intent.ACTION_MEDIA_EJECT);
            filter.addAction(Intent.ACTION_MEDIA_MOUNTED);
            filter.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
            filter.addDataScheme("file");
            getActivity().registerReceiver(mMediaStateChangedReceiver, filter);
            updateMediaSavePath();
        }

        private void unregisterMediaMountListener() {
            try {
                getActivity().unregisterReceiver(mMediaStateChangedReceiver);
            } catch (Exception e) {
                Log.e(TAG, "unregisterReceiver MediaStateChangedReceiver error:" + e);
            }
        }

        private BroadcastReceiver mMediaStateChangedReceiver = new BroadcastReceiver(){
            public void onReceive(Context context, Intent intent) {
                Log.i(TAG, "receiver broadcast action = " + intent.getAction()
                        + ", data = " + intent.getData().getPath());
                updateMediaSavePath();
            }
        };

        private void updateMediaSavePath() {
            String value = mSettingsManager.getString(SettingsManager.SCOPE_GLOBAL, Keys.KEY_MEDIA_SAVE_PATH);
            Log.i(TAG, "get mediapath value = " + value);
            if (SDCARD.equals(value)) {
                String state = Environment.getStorageState(new File(Storage.EXTENAL_SD));
                Log.i(TAG,"getSecondVolumeStorageState = " + state);
                if (!Environment.MEDIA_MOUNTED.equalsIgnoreCase(state)) {
                    mSettingsManager.setToDefault(SettingsManager.SCOPE_GLOBAL, Keys.KEY_MEDIA_SAVE_PATH);
                    restoreMediaSavePath();
                }
            }
        }

        private void restoreMediaSavePath() {
            ListPreference preference = (ListPreference) findPreference(Keys.KEY_MEDIA_SAVE_PATH);
            if (preference != null) {
                preference.setValue(mSettingsManager.getString(SettingsManager.SCOPE_GLOBAL,
                    Keys.KEY_MEDIA_SAVE_PATH));
                preference.setSummary(preference.getEntry());
            }
            Storage.DIRECTORY = Storage.DEFAULT_DIRECTORY;
            Log.i(TAG, "restore Storage.DIRECTORY = " + Storage.DIRECTORY);
        }
    }
}
