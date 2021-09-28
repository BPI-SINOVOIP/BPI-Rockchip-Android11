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

package com.android.car.dialer.ui.common.entity;

import android.content.Context;
import android.util.Pair;

import com.android.car.dialer.R;
import com.android.car.dialer.livedata.SharedPreferencesLiveData;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.TelecomUtils;

import java.util.Comparator;

/**
 * Information about how Contacts are sorted.
 */
public class ContactSortingInfo {
    /**
     * Sort by the default display order of a name. For western names it will be "Given Family".
     * For unstructured names like east asian this will be the only order.
     * Phone Dialer uses the same method for sorting given names.
     *
     * @see android.provider.ContactsContract.Contacts#DISPLAY_NAME_PRIMARY
     */
    private static final Comparator<Contact> sFirstNameComparator =
            (o1, o2) -> o1.compareBySortKeyPrimary(o2);

    /**
     * Sort by the alternative display order of a name. For western names it will be "Family,
     * Given". For unstructured names like east asian this order will be ignored and treated as
     * primary.
     * Phone Dialer uses the same method for sorting family names.
     *
     * @see android.provider.ContactsContract.Contacts#DISPLAY_NAME_ALTERNATIVE
     */
    private static final Comparator<Contact> sLastNameComparator =
            (o1, o2) -> o1.compareBySortKeyAlt(o2);

    /**
     * A static method that return how Contacts are sorted
     * The first parameter is the comparator that is used for sorting Contacts
     * The second parameter is a reference to keep track of the soring method.
     */
    public static Pair<Comparator<Contact>, Integer> getSortingInfo(Context context,
            SharedPreferencesLiveData preferencesLiveData) {
        String key = preferencesLiveData.getKey();
        String defaultValue = context.getResources().getString(R.string.sort_order_default_value);
        String firstNameSort = context.getResources().getString(
                R.string.given_name_first_key);

        Comparator<Contact> comparator;
        Integer sortMethod;
        if (preferencesLiveData.getValue() == null
                || firstNameSort.equals(
                preferencesLiveData.getValue().getString(key, defaultValue))) {
            comparator = sFirstNameComparator;
            sortMethod = TelecomUtils.SORT_BY_FIRST_NAME;
        } else {
            comparator = sLastNameComparator;
            sortMethod = TelecomUtils.SORT_BY_LAST_NAME;
        }

        return new Pair<>(comparator, sortMethod);
    }
}
