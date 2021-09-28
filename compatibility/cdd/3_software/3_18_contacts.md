## 3.18\. Contacts

Android includes [`Contacts
Provider`](https://developer.android.com/guide/topics/providers/contacts-provider)
APIs to allow applications to manage contact information stored on the device.
Contact data that is entered directly into the device is typically synchronized
with a web service, but the data MAY also only reside locally on the device.
Contacts that are only stored on the device are referred to as **local
contacts.**

[RawContacts](https://developer.android.com/reference/android/provider/ContactsContract.RawContacts)
are "associated with" or "stored in" an
[Account](https://developer.android.com/reference/android/accounts/Account)
when the
[`ACCOUNT_NAME`](https://developer.android.com/reference/android/provider/ContactsContract.SyncColumns.html#ACCOUNT_NAME),
and
[`ACCOUNT_TYPE`](https://developer.android.com/reference/android/provider/ContactsContract.SyncColumns.html#ACCOUNT_TYPE),
columns for the raw contacts match the corresponding
[Account.name](https://developer.android.com/reference/android/accounts/Account#name)
and
[Account.type](https://developer.android.com/reference/android/accounts/Account#type)
fields of the account.

**Default local account**: an account for raw contacts that are only stored on
the device and not associated with an Account in the [AccountManager](
https://developer.android.com/reference/android/accounts/AccountManager), which are
created with *null* values for the
[`ACCOUNT_NAME`](https://developer.android.com/reference/android/provider/ContactsContract.SyncColumns.html#ACCOUNT_NAME),
and
[`ACCOUNT_TYPE`](https://developer.android.com/reference/android/provider/ContactsContract.SyncColumns.html#ACCOUNT_TYPE),
columns.

**Custom local account**: an account for raw contacts that are only stored on the
device and not associated with an Account in the AccountManager, which are
created with *at least one non-null value* for the
[`ACCOUNT_NAME`](https://developer.android.com/reference/android/provider/ContactsContract.SyncColumns.html#ACCOUNT_NAME),
and
[`ACCOUNT_TYPE`](https://developer.android.com/reference/android/provider/ContactsContract.SyncColumns.html#ACCOUNT_TYPE),
columns.

Device implementations:

*    [C-SR] Are STRONGLY RECOMMENDED to not create **custom local accounts**.

If device implementations use a **custom local account**:

*    [C-1-1] The
     [`ACCOUNT_NAME`](https://developer.android.com/reference/android/provider/ContactsContract.SyncColumns.html#ACCOUNT_NAME),
     of the **custom local account** MUST be returned by
     [`ContactsContract.RawContacts.getLocalAccountName`](https://developer.android.com/reference/android/provider/ContactsContract.RawContacts.html#getLocalAccountName\(\))
*    [C-1-2] The
     [`ACCOUNT_TYPE`](https://developer.android.com/reference/android/provider/ContactsContract.SyncColumns.html#ACCOUNT_TYPE),
     of the **custom local account** MUST be returned by
     [`ContactsContract.RawContacts.getLocalAccountType`](https://developer.android.com/reference/android/provider/ContactsContract.RawContacts.html#getLocalAccountType\(\))
*    [C-1-3] Raw contacts that are inserted by third party applications with
     the **default local account** (i.e. by setting null values for
     `ACCOUNT_NAME` and `ACCOUNT_TYPE`) MUST be inserted to the **custom local
     account**.
*    [C-1-4] Raw contacts inserted into the **custom local account** MUST not be
     removed when accounts are added or removed.
*    [C-1-5] Delete operations performed against the **custom local account**
     MUST result in raw contacts being purged immediately (as if the
     [`CALLER_IS_SYNCADAPTER`](https://developer.android.com/reference/android/provider/ContactsContract.html#CALLER_IS_SYNCADAPTER)
     param was set to true), even if the `CALLER\_IS\_SYNCADAPTER` param was set
     to false or not specified.