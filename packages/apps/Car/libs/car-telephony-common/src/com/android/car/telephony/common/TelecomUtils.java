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

package com.android.car.telephony.common;

import android.Manifest;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.provider.CallLog;
import android.provider.ContactsContract;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract.PhoneLookup;
import android.provider.Settings;
import android.telecom.Call;
import android.telephony.PhoneNumberUtils;
import android.telephony.TelephonyManager;
import android.text.BidiFormatter;
import android.text.TextDirectionHeuristics;
import android.text.TextUtils;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;

import com.android.car.apps.common.LetterTileDrawable;
import com.android.car.apps.common.log.L;

import com.bumptech.glide.Glide;
import com.bumptech.glide.request.RequestOptions;
import com.google.i18n.phonenumbers.NumberParseException;
import com.google.i18n.phonenumbers.PhoneNumberUtil;
import com.google.i18n.phonenumbers.Phonenumber;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CompletableFuture;

/**
 * Helper methods.
 */
public class TelecomUtils {
    private static final String TAG = "CD.TelecomUtils";
    private static final int PII_STRING_LENGTH = 4;
    private static final String COUNTRY_US = "US";
    /**
     * A reference to keep track of the soring method of sorting by the contact's first name.
     */
    public static final Integer SORT_BY_FIRST_NAME = 1;
    /**
     * A reference to keep track of the soring method of sorting by the contact's last name.
     */
    public static final Integer SORT_BY_LAST_NAME = 2;

    private static String sVoicemailNumber;
    private static TelephonyManager sTelephonyManager;

    /**
     * Get the voicemail number.
     */
    public static String getVoicemailNumber(Context context) {
        if (sVoicemailNumber == null) {
            sVoicemailNumber = getTelephonyManager(context).getVoiceMailNumber();
        }
        return sVoicemailNumber;
    }

    /**
     * Returns {@code true} if the given number is a voice mail number.
     *
     * @see TelephonyManager#getVoiceMailNumber()
     */
    public static boolean isVoicemailNumber(Context context, String number) {
        if (TextUtils.isEmpty(number)) {
            return false;
        }

        if (ContextCompat.checkSelfPermission(context, Manifest.permission.READ_PHONE_STATE)
                != PackageManager.PERMISSION_GRANTED) {
            return false;
        }

        return number.equals(getVoicemailNumber(context));
    }

    /**
     * Get the {@link TelephonyManager} instance.
     */
    // TODO(deanh): remove this, getSystemService is not slow.
    public static TelephonyManager getTelephonyManager(Context context) {
        if (sTelephonyManager == null) {
            sTelephonyManager =
                    (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        }
        return sTelephonyManager;
    }

    /**
     * Format a number as a phone number.
     */
    public static String getFormattedNumber(Context context, String number) {
        L.d(TAG, "getFormattedNumber: " + piiLog(number));
        if (number == null) {
            return "";
        }

        String countryIso = getCurrentCountryIsoFromLocale(context);
        L.d(TAG, "PhoneNumberUtils.formatNumber, number: " + piiLog(number)
                + ", country: " + countryIso);

        String formattedNumber = PhoneNumberUtils.formatNumber(number, countryIso);
        formattedNumber = TextUtils.isEmpty(formattedNumber) ? number : formattedNumber;
        L.d(TAG, "getFormattedNumber, result: " + piiLog(formattedNumber));

        return formattedNumber;
    }

    /**
     * @return The ISO 3166-1 two letters country code of the country the user is in.
     */
    private static String getCurrentCountryIso(Context context, Locale locale) {
        String countryIso = locale.getCountry();
        if (countryIso == null || countryIso.length() != 2) {
            L.w(TAG, "Invalid locale, falling back to US");
            countryIso = COUNTRY_US;
        }
        return countryIso;
    }

    private static String getCurrentCountryIso(Context context) {
        return getCurrentCountryIso(context, Locale.getDefault());
    }

    private static String getCurrentCountryIsoFromLocale(Context context) {
        String countryIso;
        countryIso = context.getResources().getConfiguration().getLocales().get(0).getCountry();

        if (countryIso == null) {
            L.w(TAG, "Invalid locale, falling back to US");
            countryIso = COUNTRY_US;
        }

        return countryIso;
    }

    /**
     * Creates a new instance of {@link Phonenumber.PhoneNumber} base on the given number and sim
     * card country code. Returns {@code null} if the number in an invalid number.
     */
    @Nullable
    public static Phonenumber.PhoneNumber createI18nPhoneNumber(Context context, String number) {
        try {
            return PhoneNumberUtil.getInstance().parse(number, getCurrentCountryIso(context));
        } catch (NumberParseException e) {
            return null;
        }
    }

    /**
     * Contains all the info used to display a phone number on the screen. Returned by {@link
     * #getPhoneNumberInfo(Context, String)}
     */
    public static final class PhoneNumberInfo {
        private final String mPhoneNumber;
        private final String mDisplayName;
        private final String mDisplayNameAlt;
        private final String mInitials;
        private final Uri mAvatarUri;
        private final String mTypeLabel;
        private final String mLookupKey;

        public PhoneNumberInfo(String phoneNumber, String displayName, String displayNameAlt,
                String initials, Uri avatarUri, String typeLabel, String lookupKey) {
            mPhoneNumber = phoneNumber;
            mDisplayName = displayName;
            mDisplayNameAlt = displayNameAlt;
            mInitials = initials;
            mAvatarUri = avatarUri;
            mTypeLabel = typeLabel;
            mLookupKey = lookupKey;
        }

        public String getPhoneNumber() {
            return mPhoneNumber;
        }

        public String getDisplayName() {
            return mDisplayName;
        }

        public String getDisplayNameAlt() {
            return mDisplayNameAlt;
        }

        /**
         * Returns the initials of the contact related to the phone number. Returns null if there is
         * no related contact.
         */
        @Nullable
        public String getInitials() {
            return mInitials;
        }

        @Nullable
        public Uri getAvatarUri() {
            return mAvatarUri;
        }

        public String getTypeLabel() {
            return mTypeLabel;
        }

        /** Returns the lookup key of the contact if any is found. */
        @Nullable
        public String getLookupKey() {
            return mLookupKey;
        }

    }

    /**
     * Gets all the info needed to properly display a phone number to the UI. (e.g. if it's the
     * voicemail number, return a string and a uri that represents voicemail, if it's a contact, get
     * the contact's name, its avatar uri, the phone number's label, etc).
     */
    public static CompletableFuture<PhoneNumberInfo> getPhoneNumberInfo(
            Context context, String number) {

        if (TextUtils.isEmpty(number)) {
            return CompletableFuture.completedFuture(new PhoneNumberInfo(
                    number,
                    context.getString(R.string.unknown),
                    context.getString(R.string.unknown),
                    null,
                    null,
                    "",
                    null));
        }

        if (isVoicemailNumber(context, number)) {
            return CompletableFuture.completedFuture(new PhoneNumberInfo(
                    number,
                    context.getString(R.string.voicemail),
                    context.getString(R.string.voicemail),
                    null,
                    makeResourceUri(context, R.drawable.ic_voicemail),
                    "",
                    null));
        }

        return CompletableFuture.supplyAsync(() -> lookupNumberInBackground(context, number));
    }

    /** Lookup phone number info in background. */
    @WorkerThread
    public static PhoneNumberInfo lookupNumberInBackground(Context context, String number) {
        if (ContextCompat.checkSelfPermission(context, Manifest.permission.READ_CONTACTS)
                != PackageManager.PERMISSION_GRANTED) {
            String readableNumber = getReadableNumber(context, number);
            return new PhoneNumberInfo(number, readableNumber, readableNumber, null, null, null,
                    null);
        }

        if (InMemoryPhoneBook.isInitialized()) {
            Contact contact = InMemoryPhoneBook.get().lookupContactEntry(number);
            if (contact != null) {
                String name = contact.getDisplayName();
                String nameAlt = contact.getDisplayNameAlt();
                if (TextUtils.isEmpty(name)) {
                    name = getReadableNumber(context, number);
                }
                if (TextUtils.isEmpty(nameAlt)) {
                    nameAlt = name;
                }

                PhoneNumber phoneNumber = contact.getPhoneNumber(context, number);
                CharSequence typeLabel = phoneNumber == null ? "" : phoneNumber.getReadableLabel(
                        context.getResources());

                return new PhoneNumberInfo(
                        number,
                        name,
                        nameAlt,
                        contact.getInitials(),
                        contact.getAvatarUri(),
                        typeLabel.toString(),
                        contact.getLookupKey());
            }
        } else {
          L.d(TAG, "InMemoryPhoneBook not initialized.");
        }

        String name = null;
        String nameAlt = null;
        String initials = null;
        String photoUriString = null;
        CharSequence typeLabel = "";
        String lookupKey = null;

        ContentResolver cr = context.getContentResolver();
        try (Cursor cursor = cr.query(
                Uri.withAppendedPath(PhoneLookup.CONTENT_FILTER_URI, Uri.encode(number)),
                new String[]{
                        PhoneLookup.DISPLAY_NAME,
                        PhoneLookup.DISPLAY_NAME_ALTERNATIVE,
                        PhoneLookup.PHOTO_URI,
                        PhoneLookup.TYPE,
                        PhoneLookup.LABEL,
                        PhoneLookup.LOOKUP_KEY,
                },
                null, null, null)) {

            if (cursor != null && cursor.moveToFirst()) {
                int nameColumn = cursor.getColumnIndex(PhoneLookup.DISPLAY_NAME);
                int altNameColumn = cursor.getColumnIndex(PhoneLookup.DISPLAY_NAME_ALTERNATIVE);
                int photoUriColumn = cursor.getColumnIndex(PhoneLookup.PHOTO_URI);
                int typeColumn = cursor.getColumnIndex(PhoneLookup.TYPE);
                int labelColumn = cursor.getColumnIndex(PhoneLookup.LABEL);
                int lookupKeyColumn = cursor.getColumnIndex(PhoneLookup.LOOKUP_KEY);

                name = cursor.getString(nameColumn);
                nameAlt = cursor.getString(altNameColumn);
                photoUriString = cursor.getString(photoUriColumn);
                initials = getInitials(name, nameAlt);

                int type = cursor.getInt(typeColumn);
                String label = cursor.getString(labelColumn);
                typeLabel = Phone.getTypeLabel(context.getResources(), type, label);

                lookupKey = cursor.getString(lookupKeyColumn);
            }
        }

        if (TextUtils.isEmpty(name)) {
            name = getReadableNumber(context, number);
        }
        if (TextUtils.isEmpty(nameAlt)) {
            nameAlt = name;
        }

        return new PhoneNumberInfo(
                number,
                name,
                nameAlt,
                initials,
                TextUtils.isEmpty(photoUriString) ? null : Uri.parse(photoUriString),
                typeLabel.toString(),
                lookupKey);
    }

    private static String getReadableNumber(Context context, String number) {
        String readableNumber = getFormattedNumber(context, number);

        if (readableNumber == null) {
            readableNumber = context.getString(R.string.unknown);
        }
        return readableNumber;
    }

    /**
     * @return A string representation of the call state that can be presented to a user.
     */
    public static String callStateToUiString(Context context, int state) {
        Resources res = context.getResources();
        switch (state) {
            case Call.STATE_ACTIVE:
                return res.getString(R.string.call_state_call_active);
            case Call.STATE_HOLDING:
                return res.getString(R.string.call_state_hold);
            case Call.STATE_NEW:
            case Call.STATE_CONNECTING:
                return res.getString(R.string.call_state_connecting);
            case Call.STATE_SELECT_PHONE_ACCOUNT:
            case Call.STATE_DIALING:
                return res.getString(R.string.call_state_dialing);
            case Call.STATE_DISCONNECTED:
                return res.getString(R.string.call_state_call_ended);
            case Call.STATE_RINGING:
                return res.getString(R.string.call_state_call_ringing);
            case Call.STATE_DISCONNECTING:
                return res.getString(R.string.call_state_call_ending);
            default:
                throw new IllegalStateException("Unknown Call State: " + state);
        }
    }

    /**
     * Returns true if the telephony network is available.
     */
    public static boolean isNetworkAvailable(Context context) {
        TelephonyManager tm =
                (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        return tm.getNetworkType() != TelephonyManager.NETWORK_TYPE_UNKNOWN
                && tm.getSimState() == TelephonyManager.SIM_STATE_READY;
    }

    /**
     * Returns true if airplane mode is on.
     */
    public static boolean isAirplaneModeOn(Context context) {
        return Settings.System.getInt(context.getContentResolver(),
                Settings.Global.AIRPLANE_MODE_ON, 0) != 0;
    }

    /**
     * Sets a Contact avatar onto the provided {@code icon}. The first letter or both letters of the
     * contact's initials.
     *
     * @param sortMethod can be either {@link #SORT_BY_FIRST_NAME} or {@link #SORT_BY_LAST_NAME}.
     */
    public static void setContactBitmapAsync(
            Context context,
            @Nullable final ImageView icon,
            @Nullable final Contact contact,
            Integer sortMethod) {
        setContactBitmapAsync(context, icon, contact, null, sortMethod);
    }

    /**
     * Sets a Contact avatar onto the provided {@code icon}. The first letter or both letters of the
     * contact's initials. Will start with first name by default.
     */
    public static void setContactBitmapAsync(
            Context context,
            @Nullable final ImageView icon,
            @Nullable final Contact contact,
            @Nullable final String fallbackDisplayName) {
        setContactBitmapAsync(context, icon, contact, fallbackDisplayName, SORT_BY_FIRST_NAME);
    }

    /**
     * Sets a Contact avatar onto the provided {@code icon}. The first letter or both letters of the
     * contact's initials or {@code fallbackDisplayName} will be used as a fallback resource if
     * avatar loading fails.
     *
     * @param sortMethod can be either {@link #SORT_BY_FIRST_NAME} or {@link #SORT_BY_LAST_NAME}. If
     *                   the value is {@link #SORT_BY_FIRST_NAME}, the name and initials order will
     *                   be first name first. Otherwise, the order will be last name first.
     */
    public static void setContactBitmapAsync(
            Context context,
            @Nullable final ImageView icon,
            @Nullable final Contact contact,
            @Nullable final String fallbackDisplayName,
            Integer sortMethod) {
        Uri avatarUri = contact != null ? contact.getAvatarUri() : null;
        boolean startWithFirstName = isSortByFirstName(sortMethod);
        String initials = contact != null
                ? contact.getInitialsBasedOnDisplayOrder(startWithFirstName)
                : (fallbackDisplayName == null ? null : getInitials(fallbackDisplayName, null));
        String identifier = contact == null ? fallbackDisplayName : contact.getDisplayName();

        setContactBitmapAsync(context, icon, avatarUri, initials, identifier);
    }

    /**
     * Sets a Contact avatar onto the provided {@code icon}. A letter tile base on the contact's
     * initials and identifier will be used as a fallback resource if avatar loading fails.
     */
    public static void setContactBitmapAsync(
            Context context,
            @Nullable final ImageView icon,
            @Nullable final Uri avatarUri,
            @Nullable final String initials,
            @Nullable final String identifier) {
        if (icon == null) {
            return;
        }

        LetterTileDrawable letterTileDrawable = createLetterTile(context, initials, identifier);

        Glide.with(context)
                .load(avatarUri)
                .apply(new RequestOptions().centerCrop().error(letterTileDrawable))
                .into(icon);
    }

    /**
     * Create a {@link LetterTileDrawable} for the given initials.
     *
     * @param initials   is the letters that will be drawn on the canvas. If it is null, then an
     *                   avatar anonymous icon will be drawn
     * @param identifier will decide the color for the drawable. If null, a default color will be
     *                   used.
     */
    public static LetterTileDrawable createLetterTile(
            Context context,
            @Nullable String initials,
            @Nullable String identifier) {
        int numberOfLetter = context.getResources().getInteger(
                R.integer.config_number_of_letters_shown_for_avatar);
        String letters = initials != null
                ? initials.substring(0, Math.min(initials.length(), numberOfLetter)) : null;
        LetterTileDrawable letterTileDrawable = new LetterTileDrawable(context.getResources(),
                letters, identifier);
        return letterTileDrawable;
    }

    /**
     * Set the given phone number as the primary phone number for its associated contact.
     */
    public static void setAsPrimaryPhoneNumber(Context context, PhoneNumber phoneNumber) {
        if (context.checkSelfPermission(Manifest.permission.WRITE_CONTACTS)
                != PackageManager.PERMISSION_GRANTED) {
            L.w(TAG, "Missing WRITE_CONTACTS permission, not setting primary number.");
            return;
        }
        // Update the primary values in the data record.
        ContentValues values = new ContentValues(1);
        values.put(ContactsContract.Data.IS_SUPER_PRIMARY, 1);
        values.put(ContactsContract.Data.IS_PRIMARY, 1);

        context.getContentResolver().update(
                ContentUris.withAppendedId(ContactsContract.Data.CONTENT_URI, phoneNumber.getId()),
                values, null, null);
    }

    /**
     * Mark missed call log matching given phone number as read. If phone number string is not
     * valid, it will mark all new missed call log as read.
     */
    public static void markCallLogAsRead(Context context, String phoneNumberString) {
        markCallLogAsRead(context, CallLog.Calls.NUMBER, phoneNumberString);
    }

    /**
     * Mark missed call log matching given call log id as read. If phone number string is not
     * valid, it will mark all new missed call log as read.
     */
    public static void markCallLogAsRead(Context context, long callLogId) {
        markCallLogAsRead(context, CallLog.Calls._ID,
                callLogId < 0 ? null : String.valueOf(callLogId));
    }

    /**
     * Mark missed call log matching given column name and selection argument as read. If the column
     * name or the selection argument is not valid, mark all new missed call log as read.
     */
    private static void markCallLogAsRead(Context context, String columnName,
            String selectionArg) {
        if (context.checkSelfPermission(Manifest.permission.WRITE_CALL_LOG)
                != PackageManager.PERMISSION_GRANTED) {
            L.w(TAG, "Missing WRITE_CALL_LOG permission; not marking missed calls as read.");
            return;
        }
        ContentValues contentValues = new ContentValues();
        contentValues.put(CallLog.Calls.NEW, 0);
        contentValues.put(CallLog.Calls.IS_READ, 1);

        List<String> selectionArgs = new ArrayList<>();
        StringBuilder where = new StringBuilder();
        where.append(CallLog.Calls.NEW);
        where.append(" = 1 AND ");
        where.append(CallLog.Calls.TYPE);
        where.append(" = ?");
        selectionArgs.add(Integer.toString(CallLog.Calls.MISSED_TYPE));
        if (!TextUtils.isEmpty(columnName) && !TextUtils.isEmpty(selectionArg)) {
            where.append(" AND ");
            where.append(columnName);
            where.append(" = ?");
            selectionArgs.add(selectionArg);
        }
        String[] selectionArgsArray = new String[0];
        try {
            ContentResolver contentResolver = context.getContentResolver();
            contentResolver.update(
                    CallLog.Calls.CONTENT_URI,
                    contentValues,
                    where.toString(),
                    selectionArgs.toArray(selectionArgsArray));
            // #update doesn't notify change any more. Notify change to rerun query from database.
            contentResolver.notifyChange(CallLog.Calls.CONTENT_URI, null);
        } catch (IllegalArgumentException e) {
            L.e(TAG, "markCallLogAsRead failed", e);
        }
    }

    /**
     * Returns the initials based on the name and nameAlt.
     *
     * @param name    should be the display name of a contact.
     * @param nameAlt should be alternative display name of a contact.
     */
    public static String getInitials(String name, String nameAlt) {
        StringBuilder initials = new StringBuilder();
        if (!TextUtils.isEmpty(name) && Character.isLetter(name.charAt(0))) {
            initials.append(Character.toUpperCase(name.charAt(0)));
        }
        if (!TextUtils.isEmpty(nameAlt)
                && !TextUtils.equals(name, nameAlt)
                && Character.isLetter(nameAlt.charAt(0))) {
            initials.append(Character.toUpperCase(nameAlt.charAt(0)));
        }
        return initials.toString();
    }

    /**
     * Creates a Letter Tile Icon that will display the given initials. If the initials are null,
     * then an avatar anonymous icon will be drawn.
     **/
    public static Icon createLetterTile(Context context, @Nullable String initials,
            String identifier, int avatarSize, float cornerRadiusPercent) {
        LetterTileDrawable letterTileDrawable = TelecomUtils.createLetterTile(context, initials,
                identifier);
        RoundedBitmapDrawable roundedBitmapDrawable = RoundedBitmapDrawableFactory.create(
                context.getResources(), letterTileDrawable.toBitmap(avatarSize));
        return createFromRoundedBitmapDrawable(roundedBitmapDrawable, avatarSize,
                cornerRadiusPercent);
    }

    /** Creates an Icon based on the given roundedBitmapDrawable. **/
    public static Icon createFromRoundedBitmapDrawable(RoundedBitmapDrawable roundedBitmapDrawable,
            int avatarSize, float cornerRadiusPercent) {
        float radius = avatarSize * cornerRadiusPercent;
        roundedBitmapDrawable.setCornerRadius(radius);

        final Bitmap result = Bitmap.createBitmap(avatarSize, avatarSize,
                Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(result);
        roundedBitmapDrawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        roundedBitmapDrawable.draw(canvas);
        return Icon.createWithBitmap(result);
    }

    /**
     * Sets the direction of a string, used for displaying phone numbers.
     */
    public static String getBidiWrappedNumber(String string) {
        return BidiFormatter.getInstance().unicodeWrap(string, TextDirectionHeuristics.LTR);
    }

    private static Uri makeResourceUri(Context context, int resourceId) {
        return new Uri.Builder()
                .scheme(ContentResolver.SCHEME_ANDROID_RESOURCE)
                .encodedAuthority(context.getPackageName())
                .appendEncodedPath(String.valueOf(resourceId))
                .build();
    }

    /**
     * This is a workaround for Log.Pii(). It will only show the last {@link #PII_STRING_LENGTH}
     * characters.
     */
    public static String piiLog(Object pii) {
        String piiString = String.valueOf(pii);
        return piiString.length() >= PII_STRING_LENGTH ? "*" + piiString.substring(
                piiString.length() - PII_STRING_LENGTH) : piiString;
    }

    /**
     * Returns true if contacts are sorted by their first names. Returns false if they are sorted by
     * last names.
     */
    public static boolean isSortByFirstName(Integer sortMethod) {
        return SORT_BY_FIRST_NAME.equals(sortMethod);
    }
}
