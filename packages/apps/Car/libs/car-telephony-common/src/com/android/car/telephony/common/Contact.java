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

import android.content.Context;
import android.database.Cursor;
import android.icu.text.Collator;
import android.net.Uri;
import android.os.Parcel;
import android.os.Parcelable;
import android.provider.ContactsContract;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.car.apps.common.log.L;

import java.util.ArrayList;
import java.util.List;

/**
 * Encapsulates data about a phone Contact entry. Typically loaded from the local Contact store.
 */
public class Contact implements Parcelable, Comparable<Contact> {
    private static final String TAG = "CD.Contact";

    /**
     * Column name for phonebook label column.
     */
    private static final String PHONEBOOK_LABEL = "phonebook_label";
    /**
     * Column name for alternative phonebook label column.
     */
    private static final String PHONEBOOK_LABEL_ALT = "phonebook_label_alt";

    /**
     * Contact belongs to TYPE_LETTER if its display name starts with a letter
     */
    private static final int TYPE_LETTER = 1;
    /**
     * Contact belongs to TYPE_DIGIT if its display name starts with a digit
     */
    private static final int TYPE_DIGIT = 2;
    /**
     * Contact belongs to TYPE_OTHER if it does not belong to TYPE_LETTER or TYPE_DIGIT Such as
     * empty display name or the display name starts with "_"
     */
    private static final int TYPE_OTHER = 3;

    /**
     * A reference to the {@link ContactsContract.RawContacts#CONTACT_ID}.
     */
    private long mContactId;

    /**
     * A reference to the {@link ContactsContract.Data#RAW_CONTACT_ID}.
     */
    private long mRawContactId;

    /**
     * The name of the account instance to which this row belongs, which identifies a specific
     * account. See {@link ContactsContract.RawContacts#ACCOUNT_NAME}.
     */
    private String mAccountName;

    /**
     * The display name.
     * <p>
     * The standard text shown as the contact's display name, based on the best available
     * information for the contact.
     * </p>
     * <p>
     * See {@link ContactsContract.CommonDataKinds.Phone#DISPLAY_NAME}.
     */
    private String mDisplayName;

    /**
     * The alternative display name.
     * <p>
     * An alternative representation of the display name, such as "family name first" instead of
     * "given name first" for Western names.  If an alternative is not available, the values should
     * be the same as {@link #mDisplayName}.
     * </p>
     * <p>
     * See {@link ContactsContract.CommonDataKinds.Phone#DISPLAY_NAME_ALTERNATIVE}.
     */
    private String mDisplayNameAlt;

    /**
     * The given name for the contact. See
     * {@link ContactsContract.CommonDataKinds.StructuredName#GIVEN_NAME}.
     */
    private String mGivenName;

    /**
     * The family name for the contact. See
     * {@link ContactsContract.CommonDataKinds.StructuredName#FAMILY_NAME}.
     */
    private String mFamilyName;

    /**
     * The initials of the contact's name.
     */
    private String mInitials;

    /**
     * The phonebook label.
     * <p>
     * For {@link #mDisplayName}s starting with letters, label will be the first character of {@link
     * #mDisplayName}. For {@link #mDisplayName}s starting with numbers, the label will be "#". For
     * {@link #mDisplayName}s starting with other characters, the label will be "...".
     * </p>
     */
    private String mPhoneBookLabel;

    /**
     * The alternative phonebook label.
     * <p>
     * It is similar with {@link #mPhoneBookLabel}. But instead of generating from {@link
     * #mDisplayName}, it will use {@link #mDisplayNameAlt}.
     * </p>
     */
    private String mPhoneBookLabelAlt;

    /**
     * Sort key that takes into account locale-based traditions for sorting names in address books.
     * <p>
     * See {@link ContactsContract.CommonDataKinds.Phone#SORT_KEY_PRIMARY}.
     */
    private String mSortKeyPrimary;

    /**
     * Sort key based on the alternative representation of the full name.
     * <p>
     * See {@link ContactsContract.CommonDataKinds.Phone#SORT_KEY_ALTERNATIVE}.
     */
    private String mSortKeyAlt;

    /**
     * An opaque value that contains hints on how to find the contact if its row id changed as a
     * result of a sync or aggregation. If a contact has multiple phone numbers, all phone numbers
     * are recorded in a single entry and they all have the same look up key in a single load. See
     * {@link ContactsContract.Data#LOOKUP_KEY}.
     */
    private String mLookupKey;

    /**
     * A URI that can be used to retrieve a thumbnail of the contact's photo.
     */
    @Nullable
    private Uri mAvatarThumbnailUri;

    /**
     * A URI that can be used to retrieve the contact's full-size photo.
     */
    @Nullable
    private Uri mAvatarUri;

    /**
     * Whether this contact entry is starred by user.
     */
    private boolean mIsStarred;

    /**
     * Contact-specific information about whether or not a contact has been pinned by the user at a
     * particular position within the system contact application's user interface.
     */
    private int mPinnedPosition;

    /**
     * This contact's primary phone number. Its value is null if a primary phone number is not set.
     */
    @Nullable
    private PhoneNumber mPrimaryPhoneNumber;

    /**
     * Whether this contact represents a voice mail.
     */
    private boolean mIsVoiceMail;

    /**
     * All phone numbers of this contact mapping to the unique primary key for the raw data entry.
     */
    private final List<PhoneNumber> mPhoneNumbers = new ArrayList<>();

    /**
     * All postal addresses of this contact mapping to the unique primary key for the raw data
     * entry
     */
    private final List<PostalAddress> mPostalAddresses = new ArrayList<>();

    /**
     * Parses a contact entry for a Cursor loaded from the Contact Database. A new contact will be
     * created and returned.
     */
    public static Contact fromCursor(Context context, Cursor cursor) {
        return fromCursor(context, cursor, null);
    }

    /**
     * Parses a contact entry for a Cursor loaded from the Contact Database.
     *
     * @param contact should have the same {@link #mLookupKey} and {@link #mAccountName} with the
     *                data read from the cursor, so all the data from the cursor can be loaded into
     *                this contact. If either of their {@link #mLookupKey} and {@link #mAccountName}
     *                is not the same or this contact is null, a new contact will be created and
     *                returned.
     */
    public static Contact fromCursor(Context context, Cursor cursor, @Nullable Contact contact) {
        int accountNameColumn = cursor.getColumnIndex(ContactsContract.RawContacts.ACCOUNT_NAME);
        int lookupKeyColumn = cursor.getColumnIndex(ContactsContract.Data.LOOKUP_KEY);
        String accountName = cursor.getString(accountNameColumn);
        String lookupKey = cursor.getString(lookupKeyColumn);

        if (contact == null) {
            contact = new Contact();
            contact.loadBasicInfo(cursor);
        }

        if (!TextUtils.equals(accountName, contact.mAccountName)
                || !TextUtils.equals(lookupKey, contact.mLookupKey)) {
            L.w(TAG, "A wrong contact is passed in. A new contact will be created.");
            contact = new Contact();
            contact.loadBasicInfo(cursor);
        }

        int mimetypeColumn = cursor.getColumnIndex(ContactsContract.Data.MIMETYPE);
        String mimeType = cursor.getString(mimetypeColumn);

        // More mimeType can be added here if more types of data needs to be loaded.
        switch (mimeType) {
            case ContactsContract.CommonDataKinds.StructuredName.CONTENT_ITEM_TYPE:
                contact.loadNameDetails(cursor);
                break;
            case ContactsContract.CommonDataKinds.Phone.CONTENT_ITEM_TYPE:
                contact.addPhoneNumber(context, cursor);
                break;
            case ContactsContract.CommonDataKinds.StructuredPostal.CONTENT_ITEM_TYPE:
                contact.addPostalAddress(cursor);
                break;
            default:
                L.d(TAG, String.format("This mimetype %s will not be loaded right now.", mimeType));
        }

        return contact;
    }

    /**
     * The data columns that are the same in every cursor no matter what the mimetype is will be
     * loaded here.
     */
    private void loadBasicInfo(Cursor cursor) {
        int contactIdColumn = cursor.getColumnIndex(ContactsContract.RawContacts.CONTACT_ID);
        int rawContactIdColumn = cursor.getColumnIndex(ContactsContract.Data.RAW_CONTACT_ID);
        int accountNameColumn = cursor.getColumnIndex(ContactsContract.RawContacts.ACCOUNT_NAME);
        int displayNameColumn = cursor.getColumnIndex(ContactsContract.Data.DISPLAY_NAME);
        int displayNameAltColumn = cursor.getColumnIndex(
                ContactsContract.RawContacts.DISPLAY_NAME_ALTERNATIVE);
        int phoneBookLabelColumn = cursor.getColumnIndex(PHONEBOOK_LABEL);
        int phoneBookLabelAltColumn = cursor.getColumnIndex(PHONEBOOK_LABEL_ALT);
        int sortKeyPrimaryColumn = cursor.getColumnIndex(
                ContactsContract.RawContacts.SORT_KEY_PRIMARY);
        int sortKeyAltColumn = cursor.getColumnIndex(
                ContactsContract.RawContacts.SORT_KEY_ALTERNATIVE);
        int lookupKeyColumn = cursor.getColumnIndex(ContactsContract.Data.LOOKUP_KEY);

        int avatarUriColumn = cursor.getColumnIndex(ContactsContract.Data.PHOTO_URI);
        int avatarThumbnailColumn = cursor.getColumnIndex(
                ContactsContract.Data.PHOTO_THUMBNAIL_URI);
        int starredColumn = cursor.getColumnIndex(ContactsContract.CommonDataKinds.Phone.STARRED);
        int pinnedColumn = cursor.getColumnIndex(ContactsContract.CommonDataKinds.Phone.PINNED);

        mContactId = cursor.getLong(contactIdColumn);
        mRawContactId = cursor.getLong(rawContactIdColumn);
        mAccountName = cursor.getString(accountNameColumn);
        mDisplayName = cursor.getString(displayNameColumn);
        mDisplayNameAlt = cursor.getString(displayNameAltColumn);
        mSortKeyPrimary = cursor.getString(sortKeyPrimaryColumn);
        mSortKeyAlt = cursor.getString(sortKeyAltColumn);
        mPhoneBookLabel = cursor.getString(phoneBookLabelColumn);
        mPhoneBookLabelAlt = cursor.getString(phoneBookLabelAltColumn);
        mLookupKey = cursor.getString(lookupKeyColumn);

        String avatarUriStr = cursor.getString(avatarUriColumn);
        mAvatarUri = avatarUriStr == null ? null : Uri.parse(avatarUriStr);
        String avatarThumbnailStringUri = cursor.getString(avatarThumbnailColumn);
        mAvatarThumbnailUri = avatarThumbnailStringUri == null ? null : Uri.parse(
                avatarThumbnailStringUri);

        mIsStarred = cursor.getInt(starredColumn) > 0;
        mPinnedPosition = cursor.getInt(pinnedColumn);
    }

    /**
     * Loads the data whose mimetype is
     * {@link ContactsContract.CommonDataKinds.StructuredName#CONTENT_ITEM_TYPE}.
     */
    private void loadNameDetails(Cursor cursor) {
        int firstNameColumn = cursor.getColumnIndex(
                ContactsContract.CommonDataKinds.StructuredName.GIVEN_NAME);
        int lastNameColumn = cursor.getColumnIndex(
                ContactsContract.CommonDataKinds.StructuredName.FAMILY_NAME);

        mGivenName = cursor.getString(firstNameColumn);
        mFamilyName = cursor.getString(lastNameColumn);
    }

    /**
     * Loads the data whose mimetype is
     * {@link ContactsContract.CommonDataKinds.Phone#CONTENT_ITEM_TYPE}.
     */
    private void addPhoneNumber(Context context, Cursor cursor) {
        PhoneNumber newNumber = PhoneNumber.fromCursor(context, cursor);

        boolean hasSameNumber = false;
        for (PhoneNumber number : mPhoneNumbers) {
            if (newNumber.equals(number)) {
                hasSameNumber = true;
                number.merge(newNumber);
            }
        }

        if (!hasSameNumber) {
            mPhoneNumbers.add(newNumber);
        }

        if (newNumber.isPrimary()) {
            mPrimaryPhoneNumber = newNumber.merge(mPrimaryPhoneNumber);
        }

        // TODO: update voice mail number part when start to support voice mail.
        if (TelecomUtils.isVoicemailNumber(context, newNumber.getNumber())) {
            mIsVoiceMail = true;
        }
    }

    /**
     * Loads the data whose mimetype is
     * {@link ContactsContract.CommonDataKinds.StructuredPostal#CONTENT_ITEM_TYPE}.
     */
    private void addPostalAddress(Cursor cursor) {
        PostalAddress newAddress = PostalAddress.fromCursor(cursor);

        if (!mPostalAddresses.contains(newAddress)) {
            mPostalAddresses.add(newAddress);
        }
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof Contact && mLookupKey.equals(((Contact) obj).mLookupKey)
                && TextUtils.equals(((Contact) obj).mAccountName, mAccountName);
    }

    @Override
    public int hashCode() {
        return mLookupKey.hashCode();
    }

    @Override
    public String toString() {
        return mDisplayName + mPhoneNumbers;
    }

    /**
     * Returns the aggregated contact id.
     */
    public long getId() {
        return mContactId;
    }

    /**
     * Returns the raw contact id.
     */
    public long getRawContactId() {
        return mRawContactId;
    }

    /**
     * Returns a lookup uri using {@link #mContactId} and {@link #mLookupKey}. Returns null if
     * unable to get a valid lookup URI from the provided parameters. See {@link
     * ContactsContract.Contacts#getLookupUri(long, String)}.
     */
    @Nullable
    public Uri getLookupUri() {
        return ContactsContract.Contacts.getLookupUri(mContactId, mLookupKey);
    }

    /**
     * Returns {@link #mAccountName}.
     */
    public String getAccountName() {
        return mAccountName;
    }

    /**
     * Returns {@link #mDisplayName}.
     */
    public String getDisplayName() {
        return mDisplayName;
    }

    /**
     * Returns {@link #mDisplayNameAlt}.
     */
    public String getDisplayNameAlt() {
        return mDisplayNameAlt;
    }

    /**
     * Returns {@link #mGivenName}.
     */
    public String getGivenName() {
        return mGivenName;
    }

    /**
     * Returns {@link #mFamilyName}.
     */
    public String getFamilyName() {
        return mFamilyName;
    }

    /**
     * Returns the initials of the contact's name.
     */
    //TODO: update how to get initials after refactoring. Could use last name and first name to
    // get initials after refactoring to avoid error for those names with prefix.
    public String getInitials() {
        if (mInitials == null) {
            mInitials = TelecomUtils.getInitials(mDisplayName, mDisplayNameAlt);
        }

        return mInitials;
    }

    /**
     * Returns the initials of the contact's name based on display order.
     */
    public String getInitialsBasedOnDisplayOrder(boolean startWithFirstName) {
        if (startWithFirstName) {
            return TelecomUtils.getInitials(mDisplayName, mDisplayNameAlt);
        } else {
            return TelecomUtils.getInitials(mDisplayNameAlt, mDisplayName);
        }
    }

    /**
     * Returns {@link #mPhoneBookLabel}
     */
    public String getPhonebookLabel() {
        return mPhoneBookLabel;
    }

    /**
     * Returns {@link #mPhoneBookLabelAlt}
     */
    public String getPhonebookLabelAlt() {
        return mPhoneBookLabelAlt;
    }

    /**
     * Returns {@link #mLookupKey}.
     */
    public String getLookupKey() {
        return mLookupKey;
    }

    /**
     * Returns the Uri for avatar.
     */
    @Nullable
    public Uri getAvatarUri() {
        return mAvatarUri != null ? mAvatarUri : mAvatarThumbnailUri;
    }

    /**
     * Return all phone numbers associated with this contact.
     */
    public List<PhoneNumber> getNumbers() {
        return mPhoneNumbers;
    }

    /**
     * Return all postal addresses associated with this contact.
     */
    public List<PostalAddress> getPostalAddresses() {
        return mPostalAddresses;
    }

    /**
     * Returns if this Contact represents a voice mail number.
     */
    public boolean isVoicemail() {
        return mIsVoiceMail;
    }

    /**
     * Returns if this contact has a primary phone number.
     */
    public boolean hasPrimaryPhoneNumber() {
        return mPrimaryPhoneNumber != null;
    }

    /**
     * Returns the primary phone number for this Contact. Returns null if there is not one.
     */
    @Nullable
    public PhoneNumber getPrimaryPhoneNumber() {
        return mPrimaryPhoneNumber;
    }

    /**
     * Returns if this Contact is starred.
     */
    public boolean isStarred() {
        return mIsStarred;
    }

    /**
     * Returns {@link #mPinnedPosition}.
     */
    public int getPinnedPosition() {
        return mPinnedPosition;
    }

    /**
     * Looks up a {@link PhoneNumber} of this contact for the given phone number string. Returns
     * {@code null} if this contact doesn't contain the given phone number.
     */
    @Nullable
    public PhoneNumber getPhoneNumber(Context context, String number) {
        I18nPhoneNumberWrapper i18nPhoneNumber = I18nPhoneNumberWrapper.Factory.INSTANCE.get(
                context, number);
        for (PhoneNumber phoneNumber : mPhoneNumbers) {
            if (phoneNumber.getI18nPhoneNumberWrapper().equals(i18nPhoneNumber)) {
                return phoneNumber;
            }
        }
        return null;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeLong(mContactId);
        dest.writeLong(mRawContactId);
        dest.writeString(mLookupKey);
        dest.writeString(mAccountName);
        dest.writeString(mDisplayName);
        dest.writeString(mDisplayNameAlt);
        dest.writeString(mSortKeyPrimary);
        dest.writeString(mSortKeyAlt);
        dest.writeString(mPhoneBookLabel);
        dest.writeString(mPhoneBookLabelAlt);
        dest.writeParcelable(mAvatarThumbnailUri, 0);
        dest.writeParcelable(mAvatarUri, 0);
        dest.writeBoolean(mIsStarred);
        dest.writeInt(mPinnedPosition);

        dest.writeBoolean(mIsVoiceMail);
        dest.writeParcelable(mPrimaryPhoneNumber, flags);
        dest.writeInt(mPhoneNumbers.size());
        for (PhoneNumber phoneNumber : mPhoneNumbers) {
            dest.writeParcelable(phoneNumber, flags);
        }

        dest.writeInt(mPostalAddresses.size());
        for (PostalAddress postalAddress : mPostalAddresses) {
            dest.writeParcelable(postalAddress, flags);
        }
    }

    public static final Creator<Contact> CREATOR = new Creator<Contact>() {
        @Override
        public Contact createFromParcel(Parcel source) {
            return Contact.fromParcel(source);
        }

        @Override
        public Contact[] newArray(int size) {
            return new Contact[size];
        }
    };

    /**
     * Create {@link Contact} object from saved parcelable.
     */
    private static Contact fromParcel(Parcel source) {
        Contact contact = new Contact();
        contact.mContactId = source.readLong();
        contact.mRawContactId = source.readLong();
        contact.mLookupKey = source.readString();
        contact.mAccountName = source.readString();
        contact.mDisplayName = source.readString();
        contact.mDisplayNameAlt = source.readString();
        contact.mSortKeyPrimary = source.readString();
        contact.mSortKeyAlt = source.readString();
        contact.mPhoneBookLabel = source.readString();
        contact.mPhoneBookLabelAlt = source.readString();
        contact.mAvatarThumbnailUri = source.readParcelable(Uri.class.getClassLoader());
        contact.mAvatarUri = source.readParcelable(Uri.class.getClassLoader());
        contact.mIsStarred = source.readBoolean();
        contact.mPinnedPosition = source.readInt();

        contact.mIsVoiceMail = source.readBoolean();
        contact.mPrimaryPhoneNumber = source.readParcelable(PhoneNumber.class.getClassLoader());
        int phoneNumberListLength = source.readInt();
        for (int i = 0; i < phoneNumberListLength; i++) {
            PhoneNumber phoneNumber = source.readParcelable(PhoneNumber.class.getClassLoader());
            contact.mPhoneNumbers.add(phoneNumber);
            if (phoneNumber.isPrimary()) {
                contact.mPrimaryPhoneNumber = phoneNumber;
            }
        }

        int postalAddressListLength = source.readInt();
        for (int i = 0; i < postalAddressListLength; i++) {
            PostalAddress address = source.readParcelable(PostalAddress.class.getClassLoader());
            contact.mPostalAddresses.add(address);
        }

        return contact;
    }

    @Override
    public int compareTo(Contact otherContact) {
        // Use a helper function to classify Contacts
        // and by default, it should be compared by first name order.
        return compareBySortKeyPrimary(otherContact);
    }

    /**
     * Compares contacts by their {@link #mSortKeyPrimary} in an order of letters, numbers, then
     * special characters.
     */
    public int compareBySortKeyPrimary(@NonNull Contact otherContact) {
        return compareNames(mSortKeyPrimary, otherContact.mSortKeyPrimary,
                mPhoneBookLabel, otherContact.getPhonebookLabel());
    }

    /**
     * Compares contacts by their {@link #mSortKeyAlt} in an order of letters, numbers, then special
     * characters.
     */
    public int compareBySortKeyAlt(@NonNull Contact otherContact) {
        return compareNames(mSortKeyAlt, otherContact.mSortKeyAlt,
                mPhoneBookLabelAlt, otherContact.getPhonebookLabelAlt());
    }

    /**
     * Compares two strings in an order of letters, numbers, then special characters.
     */
    private int compareNames(String name, String otherName, String label, String otherLabel) {
        int type = getNameType(label);
        int otherType = getNameType(otherLabel);
        if (type != otherType) {
            return Integer.compare(type, otherType);
        }
        Collator collator = Collator.getInstance();
        return collator.compare(name == null ? "" : name, otherName == null ? "" : otherName);
    }

    /**
     * Returns the type of the name string. Types can be {@link #TYPE_LETTER}, {@link #TYPE_DIGIT}
     * and {@link #TYPE_OTHER}.
     */
    private static int getNameType(String label) {
        // A helper function to classify Contacts
        if (!TextUtils.isEmpty(label)) {
            if (Character.isLetter(label.charAt(0))) {
                return TYPE_LETTER;
            }
            if (label.contains("#")) {
                return TYPE_DIGIT;
            }
        }
        return TYPE_OTHER;
    }
}
