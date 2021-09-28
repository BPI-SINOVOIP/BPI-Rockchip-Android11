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

package android.net.util;

import android.icu.text.Transliterator;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import java.util.Collections;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;

/**
 * Transliterator to display a human-readable DHCP client hostname.
 */
public class HostnameTransliterator {
    private static final String TAG = "HostnameTransliterator";

    // Maximum length of hostname to be encoded in the DHCP message. Following RFC1035#2.3.4
    // and this transliterator converts the device name to a single label, so the label length
    // limit applies to the whole hostname.
    private static final int MAX_DNS_LABEL_LENGTH = 63;

    @Nullable
    private final Transliterator mTransliterator;

    public HostnameTransliterator() {
        final Enumeration<String> availableIDs = Transliterator.getAvailableIDs();
        final Set<String> actualIds = new HashSet<>(Collections.list(availableIDs));
        final StringBuilder rules = new StringBuilder();
        if (actualIds.contains("Any-ASCII")) {
            rules.append(":: Any-ASCII; ");
        } else if (actualIds.contains("Any-Latin") && actualIds.contains("Latin-ASCII")) {
            rules.append(":: Any-Latin; :: Latin-ASCII; ");
        } else {
            Log.e(TAG, "ICU Transliterator doesn't include supported ID");
            mTransliterator = null;
            return;
        }
        mTransliterator = Transliterator.createFromRules("", rules.toString(),
                Transliterator.FORWARD);
    }

    @VisibleForTesting
    public HostnameTransliterator(Transliterator transliterator) {
        mTransliterator = transliterator;
    }

    // RFC952 and RFC1123 stipulates an valid hostname should be:
    // 1. Only contain the alphabet (A-Z, a-z), digits (0-9), minus sign (-).
    // 2. No blank or space characters are permitted as part of a name.
    // 3. The first character must be an alpha character or digit.
    // 4. The last character must not be a minus sign (-).
    private String maybeRemoveRedundantSymbols(@NonNull String string) {
        String result = string.replaceAll("[^a-zA-Z0-9-]", "-");
        result = result.replaceAll("-+", "-");
        if (result.startsWith("-")) {
            result = result.replaceFirst("-", "");
        }
        if (result.endsWith("-")) {
            result = result.substring(0, result.length() - 1);
        }
        return result;
    }

    /**
     *  Transliterate the device name to valid hostname that could be human-readable string.
     */
    public String transliterate(@NonNull String deviceName) {
        if (deviceName == null) return null;
        if (mTransliterator == null) {
            if (!deviceName.matches("\\p{ASCII}*")) return null;
            deviceName = maybeRemoveRedundantSymbols(deviceName);
            if (TextUtils.isEmpty(deviceName)) return null;
            return deviceName.length() > MAX_DNS_LABEL_LENGTH
                    ? deviceName.substring(0, MAX_DNS_LABEL_LENGTH) : deviceName;
        }

        String hostname = maybeRemoveRedundantSymbols(mTransliterator.transliterate(deviceName));
        if (TextUtils.isEmpty(hostname)) return null;
        return hostname.length() > MAX_DNS_LABEL_LENGTH
                ? hostname.substring(0, MAX_DNS_LABEL_LENGTH) : hostname;
    }
}
