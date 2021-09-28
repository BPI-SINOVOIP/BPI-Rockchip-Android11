/*
 * Copyright (C) 2021 The Android Open Source Project
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

package com.android.car.settings.sound;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.database.Cursor;
import android.database.CursorWrapper;
import android.media.AudioAttributes;
import android.media.Ringtone;
import android.media.RingtoneManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.UserHandle;
import android.provider.MediaStore;
import android.util.TypedValue;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceGroup;
import androidx.preference.TwoStatePreference;

import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.Logger;
import com.android.car.settings.common.PreferenceController;
import com.android.car.ui.preference.CarUiRadioButtonPreference;

import java.util.regex.Pattern;

/** A {@link PreferenceController} to help pick a default ringtone. */
public class RingtonePickerPreferenceController extends PreferenceController<PreferenceGroup> {

    private static final Logger LOG = new Logger(RingtonePickerPreferenceController.class);
    private static final String SOUND_NAME_RES_PREFIX = "sound_name_";

    @VisibleForTesting
    static final String COLUMN_LABEL = MediaStore.Audio.Media.TITLE;

    @VisibleForTesting
    static final int SILENT_ITEM_POS = -1;
    private static final int UNKNOWN_POS = -2;

    private final Context mUserContext;
    private RingtoneManager mRingtoneManager;
    private LocalizedCursor mCursor;
    private Handler mHandler;

    /** See {@link RingtoneManager} for valid values. */
    private int mRingtoneType;
    private boolean mHasSilentItem;

    private int mCurrentlySelectedPos = UNKNOWN_POS;
    private TwoStatePreference mCurrentlySelectedPreference;

    private Ringtone mCurrentRingtone;
    private int mAttributesFlags = 0;
    private Bundle mArgs;

    public RingtonePickerPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);

        mUserContext = createPackageContextAsUser(getContext(), UserHandle.myUserId());
        mRingtoneManager = new RingtoneManager(getContext(), /* includeParentRingtones= */ true);
        mHandler = new Handler(Looper.getMainLooper());
        mArgs = new Bundle();
    }

    /** Arguments used to configure this preference controller. */
    public void setArguments(Bundle args) {
        mArgs = args;
    }

    /**
     * Returns the position of the currently checked preference. Returns 0 if no such element
     * exists.
     */
    public int getCurrentlySelectedPreferencePos() {
        int count = getPreference().getPreferenceCount();
        for (int i = 0; i < count; i++) {
            TwoStatePreference pref = (TwoStatePreference) getPreference().getPreference(i);
            if (pref.isChecked()) {
                return i;
            }
        }
        return 0;
    }

    /** Saves the currently selected ringtone. */
    public void saveRingtone() {
        RingtoneManager.setActualDefaultRingtoneUri(mUserContext, mRingtoneType,
                getCurrentlySelectedRingtoneUri());
    }

    @Override
    protected Class<PreferenceGroup> getPreferenceType() {
        return PreferenceGroup.class;
    }

    @Override
    protected void onCreateInternal() {
        mRingtoneType = mArgs.getInt(RingtoneManager.EXTRA_RINGTONE_TYPE, /* defaultValue= */ -1);
        mHasSilentItem = mArgs.getBoolean(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT,
                /* defaultValue= */ true);
        mAttributesFlags |= mArgs.getInt(RingtoneManager.EXTRA_RINGTONE_AUDIO_ATTRIBUTES_FLAGS,
                /* defaultValue= */ 0);

        mRingtoneManager.setType(mRingtoneType);
        mCursor = new LocalizedCursor(mRingtoneManager.getCursor(), getContext().getResources(),
                COLUMN_LABEL);
    }

    @Override
    protected void onStopInternal() {
        stopAnyPlayingRingtone();
        clearSelection();
    }

    @Override
    protected void updateState(PreferenceGroup preference) {
        populateRingtones(preference);

        clearSelection();
        Uri currentRingtoneUri =
                RingtoneManager.getActualDefaultRingtoneUri(mUserContext, mRingtoneType);
        initSelection(currentRingtoneUri);
    }

    private void populateRingtones(PreferenceGroup preference) {
        preference.removeAll();

        // Keep at the front of the list, if requested.
        if (mHasSilentItem) {
            String label = getContext().getResources().getString(
                    com.android.internal.R.string.ringtone_silent);
            int pos = SILENT_ITEM_POS;
            preference.addPreference(createRingtonePreference(label, pos));
        }

        int pos = 0;
        mCursor.moveToFirst();
        while (!mCursor.isAfterLast()) {
            String label = mCursor.getString(mCursor.getColumnIndex(COLUMN_LABEL));
            preference.addPreference(createRingtonePreference(label, pos));

            mCursor.moveToNext();
            pos++;
        }
    }

    private TwoStatePreference createRingtonePreference(String title, int key) {
        CarUiRadioButtonPreference preference = new CarUiRadioButtonPreference(getContext());
        preference.setTitle(title);
        preference.setKey(Integer.toString(key));
        preference.setChecked(false);
        preference.setViewId(View.NO_ID);
        preference.setOnPreferenceClickListener(pref -> {
            updateCurrentSelection((TwoStatePreference) pref);
            playCurrentlySelectedRingtone();
            return true;
        });
        return preference;
    }

    private void updateCurrentSelection(TwoStatePreference preference) {
        int selectedPos = Integer.parseInt(preference.getKey());
        if (mCurrentlySelectedPos != selectedPos) {
            if (mCurrentlySelectedPreference != null) {
                mCurrentlySelectedPreference.setChecked(false);
                mCurrentlySelectedPreference.setViewId(View.NO_ID);
            }
        }
        mCurrentlySelectedPreference = preference;
        mCurrentlySelectedPos = selectedPos;
        mCurrentlySelectedPreference.setChecked(true);
        mCurrentlySelectedPreference.setViewId(R.id.ringtone_picker_selected_id);
    }

    private void initSelection(Uri uri) {
        if (uri == null) {
            mCurrentlySelectedPos = SILENT_ITEM_POS;
        } else {
            mCurrentlySelectedPos = mRingtoneManager.getRingtonePosition(uri);
        }
        int count = getPreference().getPreferenceCount();
        for (int i = 0; i < count; i++) {
            TwoStatePreference pref = (TwoStatePreference) getPreference().getPreference(i);
            int pos = Integer.parseInt(pref.getKey());
            if (mCurrentlySelectedPos == pos) {
                mCurrentlySelectedPreference = pref;
                pref.setChecked(true);
                pref.setViewId(R.id.ringtone_picker_selected_id);
            }
        }
    }

    private void clearSelection() {
        int count = getPreference().getPreferenceCount();
        for (int i = 0; i < count; i++) {
            TwoStatePreference pref = (TwoStatePreference) getPreference().getPreference(i);
            pref.setChecked(false);
            pref.setViewId(View.NO_ID);
        }

        mCurrentlySelectedPreference = null;
        mCurrentlySelectedPos = UNKNOWN_POS;
    }

    private void playCurrentlySelectedRingtone() {
        mHandler.removeCallbacks(this::run);
        mHandler.post(this::run);
    }

    private void run() {
        stopAnyPlayingRingtone();
        if (mCurrentlySelectedPos == SILENT_ITEM_POS) {
            return;
        }

        if (mCurrentlySelectedPos >= 0) {
            mCurrentRingtone = mRingtoneManager.getRingtone(mCurrentlySelectedPos);
        }

        if (mCurrentRingtone != null) {
            if (mAttributesFlags != 0) {
                mCurrentRingtone.setAudioAttributes(
                        new AudioAttributes.Builder(mCurrentRingtone.getAudioAttributes())
                                .setFlags(mAttributesFlags)
                                .build());
            }
            mCurrentRingtone.play();
        }
    }

    private void stopAnyPlayingRingtone() {
        mHandler.removeCallbacks(this::run);

        if (mCurrentRingtone != null && mCurrentRingtone.isPlaying()) {
            mCurrentRingtone.stop();
        }

        if (mRingtoneManager != null) {
            mRingtoneManager.stopPreviousRingtone();
        }
    }

    private Uri getCurrentlySelectedRingtoneUri() {
        if (mCurrentlySelectedPos >= 0) {
            return mRingtoneManager.getRingtoneUri(mCurrentlySelectedPos);
        } else if (mCurrentlySelectedPos == SILENT_ITEM_POS) {
            // Use a null Uri for the 'Silent' item.
            return null;
        } else {
            LOG.e("Requesting ringtone URI for unknown position: " + mCurrentlySelectedPos);
            return null;
        }
    }

    /**
     * Returns a context created from the given context for the given user, or null if it fails.
     */
    private Context createPackageContextAsUser(Context context, int userId) {
        try {
            return context.createPackageContextAsUser(
                    context.getPackageName(), /* flags= */ 0, UserHandle.of(userId));
        } catch (PackageManager.NameNotFoundException e) {
            LOG.e("Failed to create user context", e);
        }
        return null;
    }

    /**
     * A copy of the localized cursor provided in
     * {@link com.android.soundpicker.RingtonePickerActivity}.
     */
    private static class LocalizedCursor extends CursorWrapper {

        final int mTitleIndex;
        final Resources mResources;
        final Pattern mSanitizePattern;
        String mNamePrefix;

        LocalizedCursor(Cursor cursor, Resources resources, String columnLabel) {
            super(cursor);
            mTitleIndex = mCursor.getColumnIndex(columnLabel);
            mResources = resources;
            mSanitizePattern = Pattern.compile("[^a-zA-Z0-9]");
            if (mTitleIndex == -1) {
                LOG.e("No index for column " + columnLabel);
                mNamePrefix = null;
            } else {
                try {
                    // Build the prefix for the name of the resource to look up.
                    // Format is: "ResourcePackageName::ResourceTypeName/"
                    // (The type name is expected to be "string" but let's not hardcode it).
                    // Here we use an existing resource "ringtone_title" which is
                    // always expected to be found.
                    mNamePrefix = String.format("%s:%s/%s",
                            mResources.getResourcePackageName(R.string.ringtone_title),
                            mResources.getResourceTypeName(R.string.ringtone_title),
                            SOUND_NAME_RES_PREFIX);
                } catch (Resources.NotFoundException e) {
                    mNamePrefix = null;
                }
            }
        }

        /**
         * Process resource name to generate a valid resource name.
         *
         * @return a non-null String
         */
        private String sanitize(String input) {
            if (input == null) {
                return "";
            }
            return mSanitizePattern.matcher(input).replaceAll("_").toLowerCase();
        }

        @Override
        public String getString(int columnIndex) {
            final String defaultName = mCursor.getString(columnIndex);
            if ((columnIndex != mTitleIndex) || (mNamePrefix == null)) {
                return defaultName;
            }
            TypedValue value = new TypedValue();
            try {
                // The name currently in the database is used to derive a name to match
                // against resource names in this package.
                mResources.getValue(mNamePrefix + sanitize(defaultName), value, false);
            } catch (Resources.NotFoundException e) {
                // No localized string, use the default string.
                return defaultName;
            }
            if ((value != null) && (value.type == TypedValue.TYPE_STRING)) {
                LOG.d(String.format("Replacing name %s with %s",
                        defaultName, value.string.toString()));
                return value.string.toString();
            } else {
                LOG.e("Invalid value when looking up localized name, using " + defaultName);
                return defaultName;
            }
        }
    }

    @VisibleForTesting
    void setRingtoneManager(RingtoneManager ringtoneManager) {
        mRingtoneManager = ringtoneManager;
    }
}
