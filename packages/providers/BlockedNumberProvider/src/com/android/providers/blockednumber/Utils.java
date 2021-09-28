/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * limitations under the License
 */
package com.android.providers.blockednumber;

import static android.util.Log.isLoggable;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.Context;
import android.location.Country;
import android.location.CountryDetector;
import android.net.Uri;
import android.os.Build;
import android.telecom.PhoneAccount;
import android.telephony.PhoneNumberUtils;
import android.text.TextUtils;

import java.util.Locale;

public class Utils {
    /**
     * When generating a bug report, include the last X dialable digits when logging phone numbers.
     */
    private static final int NUM_DIALABLE_DIGITS_TO_LOG = Build.IS_USER ? 0 : 2;

    private Utils() {
    }

    public static final int MIN_INDEX_LEN = 8;
    public static String TAG = "BlockedNumberProvider";
    public static boolean VERBOSE = isLoggable(TAG, android.util.Log.VERBOSE);

    /**
     * @return The current country code.
     */
    public static @NonNull String getCurrentCountryIso(@NonNull Context context) {
        final CountryDetector detector = (CountryDetector) context.getSystemService(
                Context.COUNTRY_DETECTOR);
        if (detector != null) {
            final Country country = detector.detectCountry();
            if (country != null) {
                return country.getCountryIso();
            }
        }
        final Locale locale = context.getResources().getConfiguration().locale;
        return locale.getCountry();
    }

    /**
     * Converts a phone number to an E164 number, assuming the current country.  If {@code
     * incomingE16Number} is provided, it'll just strip it and returns.  If the number is not valid,
     * it'll return "".
     *
     * <p>Special case: if {@code rawNumber} contains '@', it's considered as an email address and
     * returned unmodified.
     */
    public static @NonNull String getE164Number(@NonNull Context context,
            @Nullable String rawNumber, @Nullable String incomingE16Number) {
        if (rawNumber != null && rawNumber.contains("@")) {
            return rawNumber;
        }
        if (!TextUtils.isEmpty(incomingE16Number)) {
            return incomingE16Number;
        }
        if (TextUtils.isEmpty(rawNumber)) {
            return "";
        }
        final String e164 =
                PhoneNumberUtils.formatNumberToE164(rawNumber, getCurrentCountryIso(context));
        return e164 == null ? "" : e164;
    }

    public static @Nullable String wrapSelectionWithParens(@Nullable String selection) {
        return TextUtils.isEmpty(selection) ? null : "(" + selection + ")";
    }

    /**
     * Generates an obfuscated string for a calling handle in {@link Uri} format, or a raw phone
     * phone number in {@link String} format.
     * @param pii The information to obfuscate.
     * @return The obfuscated string.
     */
    public static String piiHandle(Object pii) {
        if (pii == null || VERBOSE) {
            return String.valueOf(pii);
        }

        StringBuilder sb = new StringBuilder();
        if (pii instanceof Uri) {
            Uri uri = (Uri) pii;
            String scheme = uri.getScheme();

            if (!TextUtils.isEmpty(scheme)) {
                sb.append(scheme).append(":");
            }

            String textToObfuscate = uri.getSchemeSpecificPart();
            if (PhoneAccount.SCHEME_TEL.equals(scheme)) {
                obfuscatePhoneNumber(sb, textToObfuscate);
            } else if (PhoneAccount.SCHEME_SIP.equals(scheme)) {
                for (int i = 0; i < textToObfuscate.length(); i++) {
                    char c = textToObfuscate.charAt(i);
                    if (c != '@' && c != '.') {
                        c = '*';
                    }
                    sb.append(c);
                }
            } else {
                sb.append(pii(pii));
            }
        } else if (pii instanceof String) {
            String number = (String) pii;
            obfuscatePhoneNumber(sb, number);
        }

        return sb.toString();
    }

    /**
     * Obfuscates a phone number, allowing NUM_DIALABLE_DIGITS_TO_LOG digits to be exposed for the
     * phone number.
     * @param sb String buffer to write obfuscated number to.
     * @param phoneNumber The number to obfuscate.
     */
    private static void obfuscatePhoneNumber(StringBuilder sb, String phoneNumber) {
        int numDigitsToObfuscate = getDialableCount(phoneNumber)
                - NUM_DIALABLE_DIGITS_TO_LOG;
        for (int i = 0; i < phoneNumber.length(); i++) {
            char c = phoneNumber.charAt(i);
            boolean isDialable = PhoneNumberUtils.isDialable(c);
            if (isDialable) {
                numDigitsToObfuscate--;
            }
            sb.append(isDialable && numDigitsToObfuscate >= 0 ? "*" : c);
        }
    }

    /**
     * Redact personally identifiable information for production users.
     * If we are running in verbose mode, return the original string,
     * and return "***" otherwise.
     */
    public static String pii(Object pii) {
        if (pii == null || VERBOSE) {
            return String.valueOf(pii);
        }
        return "***";
    }

    /**
     * Determines the number of dialable characters in a string.
     * @param toCount The string to count dialable characters in.
     * @return The count of dialable characters.
     */
    private static int getDialableCount(String toCount) {
        int numDialable = 0;
        for (char c : toCount.toCharArray()) {
            if (PhoneNumberUtils.isDialable(c)) {
                numDialable++;
            }
        }
        return numDialable;
    }
}
