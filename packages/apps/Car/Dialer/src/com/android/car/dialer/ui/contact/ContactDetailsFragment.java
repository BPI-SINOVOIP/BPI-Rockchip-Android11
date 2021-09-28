/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.car.dialer.ui.contact;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.apps.common.LetterTileDrawable;
import com.android.car.arch.common.FutureData;
import com.android.car.dialer.R;
import com.android.car.dialer.ui.common.DialerListBaseFragment;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.PhoneNumber;
import com.android.car.telephony.common.TelecomUtils;
import com.android.car.ui.core.CarUi;
import com.android.car.ui.toolbar.Toolbar;
import com.android.car.ui.toolbar.ToolbarController;

import com.bumptech.glide.Glide;
import com.bumptech.glide.request.RequestOptions;
import com.bumptech.glide.request.target.SimpleTarget;
import com.bumptech.glide.request.transition.Transition;

/**
 * A fragment that shows the name of the contact, the photo and all listed phone numbers. It is
 * primarily used to respond to the results of search queries but supplyig it with the content://
 * uri of a contact should work too.
 */
public class ContactDetailsFragment extends DialerListBaseFragment implements
        ContactDetailsAdapter.PhoneNumberPresenter {
    private static final String TAG = "CD.ContactDetailsFragment";
    public static final String FRAGMENT_TAG = "CONTACT_DETAIL_FRAGMENT_TAG";

    // Key to load and save the contact entity instance.
    private static final String KEY_CONTACT_ENTITY = "ContactEntity";

    private Contact mContact;
    private LiveData<FutureData<Contact>> mContactDetailsLiveData;
    private ContactDetailsViewModel mContactDetailsViewModel;

    private boolean mShowActionBarView;
    private boolean mShowActionBarAvatar;

    /**
     * Creates a new ContactDetailsFragment using a {@link Contact}.
     */
    public static ContactDetailsFragment newInstance(Contact contact) {
        ContactDetailsFragment fragment = new ContactDetailsFragment();
        Bundle args = new Bundle();
        args.putParcelable(KEY_CONTACT_ENTITY, contact);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mContact = getArguments().getParcelable(KEY_CONTACT_ENTITY);
        if (mContact == null && savedInstanceState != null) {
            mContact = savedInstanceState.getParcelable(KEY_CONTACT_ENTITY);
        }
        mContactDetailsViewModel = ViewModelProviders.of(this).get(
                ContactDetailsViewModel.class);
        mContactDetailsLiveData = mContactDetailsViewModel.getContactDetails(mContact);

        mShowActionBarView = getResources().getBoolean(
                R.bool.config_show_contact_details_action_bar_view);
        mShowActionBarAvatar = getResources().getBoolean(
                R.bool.config_show_contact_details_action_bar_avatar);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        ContactDetailsAdapter contactDetailsAdapter = new ContactDetailsAdapter(getContext(),
                mContact, this);
        getRecyclerView().setAdapter(contactDetailsAdapter);
        mContactDetailsLiveData.observe(this, contact -> {
            if (contact.isLoading()) {
                showLoading();
            } else {
                onContactChanged(contact.getData());
                contactDetailsAdapter.setContact(contact.getData());
                showContent();
            }
        });
    }

    private void onContactChanged(Contact contact) {
        getArguments().clear();
        ToolbarController toolbar = CarUi.getToolbar(getActivity());
        // Null check to have unit tests to pass.
        if (toolbar == null) {
            return;
        }
        toolbar.setTitle(null);
        toolbar.setLogo(null);
        if (mShowActionBarView) {
            toolbar.setTitle(contact == null ? getString(R.string.error_contact_deleted)
                    : contact.getDisplayName());
            if (mShowActionBarAvatar) {
                int avatarSize = getResources().getDimensionPixelSize(
                        R.dimen.contact_details_action_bar_avatar_size);
                LetterTileDrawable letterTile = TelecomUtils.createLetterTile(getContext(),
                        contact == null ? null : contact.getInitials(),
                        contact == null ? null : contact.getDisplayName());
                Uri avatarUri = contact == null ? null : contact.getAvatarUri();
                Glide.with(this)
                        .asBitmap()
                        .load(avatarUri)
                        .apply(new RequestOptions().override(avatarSize).error(letterTile))
                        .into(new SimpleTarget<Bitmap>() {
                            @Override
                            public void onResourceReady(Bitmap bitmap,
                                    Transition<? super Bitmap> transition) {
                                RoundedBitmapDrawable roundedBitmapDrawable = createFromBitmap(
                                        bitmap, avatarSize);
                                toolbar.setLogo(roundedBitmapDrawable);
                            }

                            @Override
                            public void onLoadFailed(@Nullable Drawable errorDrawable) {
                                RoundedBitmapDrawable roundedBitmapDrawable = createFromLetterTile(
                                        letterTile, avatarSize);
                                toolbar.setLogo(roundedBitmapDrawable);
                            }
                        });
            }
        }
    }

    @Override
    protected void setupToolbar(@NonNull ToolbarController toolbar) {
        toolbar.setState(getToolbarState());
        toolbar.setMenuItems(null);
    }

    @Override
    protected Toolbar.State getToolbarState() {
        return Toolbar.State.SUBPAGE;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putParcelable(KEY_CONTACT_ENTITY, mContact);
    }

    @Override
    public void onClick(Contact contact, PhoneNumber phoneNumber) {
        boolean isFavorite = phoneNumber.isFavorite();
        if (isFavorite) {
            mContactDetailsViewModel.removeFromFavorite(contact, phoneNumber);
        } else {
            mContactDetailsViewModel.addToFavorite(contact, phoneNumber);
        }
    }

    private RoundedBitmapDrawable createFromLetterTile(LetterTileDrawable letterTileDrawable,
            int avatarSize) {
        return createFromBitmap(letterTileDrawable.toBitmap(avatarSize), avatarSize);
    }

    private RoundedBitmapDrawable createFromBitmap(Bitmap bitmap, int avatarSize) {
        RoundedBitmapDrawable roundedBitmapDrawable = RoundedBitmapDrawableFactory.create(
                getResources(), bitmap);
        float radiusPercent = getResources()
                .getFloat(R.dimen.contact_avatar_corner_radius_percent);
        float radius = avatarSize * radiusPercent;
        roundedBitmapDrawable.setCornerRadius(radius);
        return roundedBitmapDrawable;
    }
}
