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

package com.android.car.dialer.ui.common;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.dialer.R;
import com.android.car.dialer.log.L;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.PhoneNumber;
import com.android.car.telephony.common.TelecomUtils;
import com.android.car.ui.AlertDialogBuilder;
import com.android.car.ui.recyclerview.CarUiRadioButtonListItem;
import com.android.car.ui.recyclerview.CarUiRadioButtonListItemAdapter;

import java.util.ArrayList;
import java.util.List;

/** Ui Utilities for dialer */
public class DialerUtils {

    private static final String TAG = "CD.DialerUtils";

    /**
     * Callback interface for
     * {@link DialerUtils#showPhoneNumberSelector(Context, List, PhoneNumberSelectionCallback)} and
     * {@link DialerUtils#promptForPrimaryNumber(Context, Contact, PhoneNumberSelectionCallback)}
     */
    public interface PhoneNumberSelectionCallback {
        /**
         * Called when a phone number is chosen.
         *
         * @param phoneNumber The phone number
         * @param always      Whether the user pressed "aways" or "just once"
         */
        void onPhoneNumberSelected(PhoneNumber phoneNumber, boolean always);
    }

    /**
     * Shows a dialog asking the user to pick a phone number.
     * Has buttons for selecting always or just once.
     */
    public static void showPhoneNumberSelector(Context context,
            List<PhoneNumber> numbers,
            PhoneNumberSelectionCallback callback) {

        final List<PhoneNumber> selectedPhoneNumber = new ArrayList<>();
        List<CarUiRadioButtonListItem> items = new ArrayList<>();
        CarUiRadioButtonListItemAdapter adapter = new CarUiRadioButtonListItemAdapter(items);

        for (PhoneNumber number : numbers) {
            CharSequence readableLabel = number.getReadableLabel(context.getResources());
            CarUiRadioButtonListItem item = new CarUiRadioButtonListItem();
            item.setTitle(number.isPrimary()
                    ? context.getString(R.string.primary_number_description, readableLabel)
                    : readableLabel);
            item.setBody(TelecomUtils.getBidiWrappedNumber(number.getNumber()));
            item.setOnCheckedChangeListener((i, isChecked) -> {
                selectedPhoneNumber.clear();
                selectedPhoneNumber.add(number);
            });
            items.add(item);
        }

        new AlertDialogBuilder(context)
                .setTitle(R.string.select_number_dialog_title)
                .setSingleChoiceItems(adapter, null)
                .setNeutralButton(R.string.select_number_dialog_just_once_button,
                        (dialog, which) -> {
                            if (!selectedPhoneNumber.isEmpty()) {
                                callback.onPhoneNumberSelected(selectedPhoneNumber.get(0), false);
                            }
                        })
                .setPositiveButton(R.string.select_number_dialog_always_button,
                        (dialog, which) -> {
                            if (!selectedPhoneNumber.isEmpty()) {
                                callback.onPhoneNumberSelected(selectedPhoneNumber.get(0), true);
                            }
                        })
                .show();
    }

    /**
     * Gets the primary phone number from the contact.
     * If no primary number is set, a dialog will pop up asking the user to select one.
     * If the user presses the "always" button, the phone number will become their
     * primary phone number. The "always" parameter of the callback will always be false
     * using this method.
     */
    public static void promptForPrimaryNumber(
            Context context,
            Contact contact,
            PhoneNumberSelectionCallback callback) {
        if (contact.hasPrimaryPhoneNumber()) {
            callback.onPhoneNumberSelected(contact.getPrimaryPhoneNumber(), false);
        } else if (contact.getNumbers().size() == 1) {
            callback.onPhoneNumberSelected(contact.getNumbers().get(0), false);
        } else if (contact.getNumbers().size() > 0) {
            showPhoneNumberSelector(context, contact.getNumbers(), (phoneNumber, always) -> {
                if (always) {
                    TelecomUtils.setAsPrimaryPhoneNumber(context, phoneNumber);
                }

                callback.onPhoneNumberSelected(phoneNumber, false);
            });
        } else {
            L.w(TAG, "contact %s doesn't have any phone number", contact.getDisplayName());
        }
    }

    /**
     * Returns true if the contact has postal address and show postal address config is true.
     */
    private static boolean hasPostalAddress(Resources resources, @NonNull Contact contact) {
        boolean showPostalAddress = resources.getBoolean(
                R.bool.config_show_postal_address);
        return showPostalAddress && (!contact.getPostalAddresses().isEmpty());
    }

    /**
     * Returns true if the contact has either phone number or postal address to show.
     */
    public static boolean hasContactDetail(Resources resources, @Nullable Contact contact) {
        boolean hasContactDetail = contact != null
                && (!contact.getNumbers().isEmpty() || DialerUtils.hasPostalAddress(
                resources, contact));
        return hasContactDetail;
    }

    /**
     * Return the first visible item position in a {@link LinearLayoutManager}.
     */
    public static int getFirstVisibleItemPosition(@NonNull LinearLayoutManager layoutManager) {
        int firstItem = layoutManager.findFirstCompletelyVisibleItemPosition();
        if (firstItem == RecyclerView.NO_POSITION) {
            firstItem = layoutManager.findFirstVisibleItemPosition();
        }
        return firstItem;
    }

    /**
     * Validates whether the old anchor point is still in the new data set range.
     * If so, return the old anchor index, otherwise return 0;
     */
    public static int validateListLimitingAnchor(int newSize, int oldAnchorIndex) {
        return newSize < oldAnchorIndex ? 0 : oldAnchorIndex;
    }
}
