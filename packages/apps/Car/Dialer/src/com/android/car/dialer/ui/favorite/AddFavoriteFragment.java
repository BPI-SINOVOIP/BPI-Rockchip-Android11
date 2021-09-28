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

package com.android.car.dialer.ui.favorite;

import android.app.AlertDialog;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.widget.Toast;

import androidx.lifecycle.ViewModelProviders;

import com.android.car.dialer.R;
import com.android.car.dialer.ui.search.ContactResultsFragment;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.PhoneNumber;
import com.android.car.telephony.common.TelecomUtils;
import com.android.car.ui.AlertDialogBuilder;
import com.android.car.ui.recyclerview.CarUiContentListItem;
import com.android.car.ui.recyclerview.CarUiListItem;
import com.android.car.ui.recyclerview.CarUiListItemAdapter;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A fragment that allows the user to search for and select favorite phone numbers
 */
public class AddFavoriteFragment extends ContactResultsFragment {

    /**
     * Creates a new instance of AddFavoriteFragment
     */
    public static AddFavoriteFragment newInstance() {
        return new AddFavoriteFragment();
    }

    private AlertDialog mCurrentDialog;
    private CarUiListItemAdapter mDialogAdapter;
    private List<CarUiListItem> mFavoritePhoneNumberList;
    private Set<PhoneNumber> mSelectedNumbers;
    private Contact mSelectedContact;
    private Drawable mFavoriteIcon;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        FavoriteViewModel favoriteViewModel = ViewModelProviders.of(getActivity()).get(
                FavoriteViewModel.class);
        mSelectedNumbers = new HashSet<>();

        mFavoriteIcon = getResources().getDrawable(R.drawable.ic_favorite_activatable, null);
        mFavoriteIcon.setTintList(
                getResources().getColorStateList(R.color.primary_icon_selector, null));

        mFavoritePhoneNumberList = new ArrayList<>();
        mDialogAdapter = new CarUiListItemAdapter(mFavoritePhoneNumberList);

        mCurrentDialog = new AlertDialogBuilder(getContext())
                .setTitle(R.string.select_number_dialog_title)
                .setAdapter(mDialogAdapter)
                .setNegativeButton(R.string.cancel_add_favorites_dialog, null)
                .setPositiveButton(R.string.confirm_add_favorites_dialog,
                        (d, which) -> {
                            for (PhoneNumber number : mSelectedNumbers) {
                                favoriteViewModel.addToFavorite(mSelectedContact,
                                        number);
                            }
                            getParentFragmentManager().popBackStackImmediate();
                        })
                .setOnDismissListener(dialog -> mSelectedNumbers.clear())
                .create();
    }

    private void setPhoneNumbers(List<PhoneNumber> phoneNumbers) {
        mFavoritePhoneNumberList.clear();
        for (PhoneNumber number : phoneNumbers) {
            CarUiContentListItem item = new CarUiContentListItem(CarUiContentListItem.Action.ICON);
            item.setTitle(TelecomUtils.getBidiWrappedNumber(number.getNumber()));
            item.setSupplementalIcon(mFavoriteIcon.getConstantState().newDrawable());
            setFavoriteItemState(item, number);

            item.setOnItemClickedListener((listItem) -> {
                if (mSelectedNumbers.contains(number)) {
                    mSelectedNumbers.remove(number);
                    listItem.setActivated(false);
                } else {
                    mSelectedNumbers.add(number);
                    listItem.setActivated(true);
                }
                mDialogAdapter.notifyItemChanged(mFavoritePhoneNumberList.indexOf(listItem));
            });
            mFavoritePhoneNumberList.add(item);
        }
        mDialogAdapter.notifyDataSetChanged();
    }

    private void setFavoriteItemState(CarUiContentListItem item, PhoneNumber number) {
        CharSequence readableLabel = number.getReadableLabel(getResources());

        if (number.isFavorite()) {
            // This phone number is marked as favorite locally. Disable the favorite action button.
            item.setEnabled(false);
            item.setActivated(true);
            item.setBody(getResources().getString(R.string.favorite_number_description,
                    readableLabel));
        } else if (mSelectedContact.isStarred()) {
            // This contact is downloaded from phone, all phone numbers under this contact will show
            // under the favorite tab. Disable the favorite action button.
            item.setActivated(false);
            item.setEnabled(false);
            item.setBody(getResources().getString(R.string.favorite_number_description,
                    readableLabel));
        } else {
            item.setEnabled(true);
            item.setActivated(false);
            item.setBody(readableLabel);
        }
    }

    @Override
    public void onShowContactDetail(Contact contact) {
        if (contact == null) {
            mCurrentDialog.dismiss();
            return;
        }

        mSelectedContact = contact;
        if (contact.getNumbers().isEmpty()) {
            Toast.makeText(getContext(), R.string.no_phone_numbers, Toast.LENGTH_SHORT).show();
            mCurrentDialog.dismiss();
        } else {
            setPhoneNumbers(contact.getNumbers());
            mCurrentDialog.show();
        }
    }
}
