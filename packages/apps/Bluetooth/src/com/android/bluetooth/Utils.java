/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.android.bluetooth;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.app.AppOpsManager;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.ContentValues;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;
import android.content.pm.UserInfo;
import android.location.LocationManager;
import android.net.Uri;
import android.os.Binder;
import android.os.Build;
import android.os.ParcelUuid;
import android.os.Process;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Telephony;
import android.util.Log;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.time.Instant;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.UUID;
import java.util.concurrent.TimeUnit;

/**
 * @hide
 */

public final class Utils {
    private static final String TAG = "BluetoothUtils";
    private static final int MICROS_PER_UNIT = 625;
    private static final String PTS_TEST_MODE_PROPERTY = "persist.bluetooth.pts";

    static final int BD_ADDR_LEN = 6; // bytes
    static final int BD_UUID_LEN = 16; // bytes

    /*
     * Special characters
     *
     * (See "What is a phone number?" doc)
     * 'p' --- GSM pause character, same as comma
     * 'n' --- GSM wild character
     * 'w' --- GSM wait character
     */
    public static final char PAUSE = ',';
    public static final char WAIT = ';';

    private static boolean isPause(char c) {
        return c == 'p' || c == 'P';
    }

    private static boolean isToneWait(char c) {
        return c == 'w' || c == 'W';
    }

    public static String getAddressStringFromByte(byte[] address) {
        if (address == null || address.length != BD_ADDR_LEN) {
            return null;
        }

        return String.format("%02X:%02X:%02X:%02X:%02X:%02X", address[0], address[1], address[2],
                address[3], address[4], address[5]);
    }

    public static byte[] getByteAddress(BluetoothDevice device) {
        return getBytesFromAddress(device.getAddress());
    }

    public static byte[] addressToBytes(String address) {
        return getBytesFromAddress(address);
    }

    public static byte[] getBytesFromAddress(String address) {
        int i, j = 0;
        byte[] output = new byte[BD_ADDR_LEN];

        for (i = 0; i < address.length(); i++) {
            if (address.charAt(i) != ':') {
                output[j] = (byte) Integer.parseInt(address.substring(i, i + 2), BD_UUID_LEN);
                j++;
                i++;
            }
        }

        return output;
    }

    public static int byteArrayToInt(byte[] valueBuf) {
        return byteArrayToInt(valueBuf, 0);
    }

    public static short byteArrayToShort(byte[] valueBuf) {
        ByteBuffer converter = ByteBuffer.wrap(valueBuf);
        converter.order(ByteOrder.nativeOrder());
        return converter.getShort();
    }

    public static int byteArrayToInt(byte[] valueBuf, int offset) {
        ByteBuffer converter = ByteBuffer.wrap(valueBuf);
        converter.order(ByteOrder.nativeOrder());
        return converter.getInt(offset);
    }

    public static String byteArrayToString(byte[] valueBuf) {
        StringBuilder sb = new StringBuilder();
        for (int idx = 0; idx < valueBuf.length; idx++) {
            if (idx != 0) {
                sb.append(" ");
            }
            sb.append(String.format("%02x", valueBuf[idx]));
        }
        return sb.toString();
    }

    /**
     * A parser to transfer a byte array to a UTF8 string
     *
     * @param valueBuf the byte array to transfer
     * @return the transferred UTF8 string
     */
    public static String byteArrayToUtf8String(byte[] valueBuf) {
        CharsetDecoder decoder = Charset.forName("UTF8").newDecoder();
        ByteBuffer byteBuffer = ByteBuffer.wrap(valueBuf);
        String valueStr = "";
        try {
            valueStr = decoder.decode(byteBuffer).toString();
        } catch (Exception ex) {
            Log.e(TAG, "Error when parsing byte array to UTF8 String. " + ex);
        }
        return valueStr;
    }

    public static byte[] intToByteArray(int value) {
        ByteBuffer converter = ByteBuffer.allocate(4);
        converter.order(ByteOrder.nativeOrder());
        converter.putInt(value);
        return converter.array();
    }

    public static byte[] uuidToByteArray(ParcelUuid pUuid) {
        int length = BD_UUID_LEN;
        ByteBuffer converter = ByteBuffer.allocate(length);
        converter.order(ByteOrder.BIG_ENDIAN);
        long msb, lsb;
        UUID uuid = pUuid.getUuid();
        msb = uuid.getMostSignificantBits();
        lsb = uuid.getLeastSignificantBits();
        converter.putLong(msb);
        converter.putLong(8, lsb);
        return converter.array();
    }

    public static byte[] uuidsToByteArray(ParcelUuid[] uuids) {
        int length = uuids.length * BD_UUID_LEN;
        ByteBuffer converter = ByteBuffer.allocate(length);
        converter.order(ByteOrder.BIG_ENDIAN);
        UUID uuid;
        long msb, lsb;
        for (int i = 0; i < uuids.length; i++) {
            uuid = uuids[i].getUuid();
            msb = uuid.getMostSignificantBits();
            lsb = uuid.getLeastSignificantBits();
            converter.putLong(i * BD_UUID_LEN, msb);
            converter.putLong(i * BD_UUID_LEN + 8, lsb);
        }
        return converter.array();
    }

    public static ParcelUuid[] byteArrayToUuid(byte[] val) {
        int numUuids = val.length / BD_UUID_LEN;
        ParcelUuid[] puuids = new ParcelUuid[numUuids];
        UUID uuid;
        int offset = 0;

        ByteBuffer converter = ByteBuffer.wrap(val);
        converter.order(ByteOrder.BIG_ENDIAN);

        for (int i = 0; i < numUuids; i++) {
            puuids[i] = new ParcelUuid(
                    new UUID(converter.getLong(offset), converter.getLong(offset + 8)));
            offset += BD_UUID_LEN;
        }
        return puuids;
    }

    public static String debugGetAdapterStateString(int state) {
        switch (state) {
            case BluetoothAdapter.STATE_OFF:
                return "STATE_OFF";
            case BluetoothAdapter.STATE_ON:
                return "STATE_ON";
            case BluetoothAdapter.STATE_TURNING_ON:
                return "STATE_TURNING_ON";
            case BluetoothAdapter.STATE_TURNING_OFF:
                return "STATE_TURNING_OFF";
            default:
                return "UNKNOWN";
        }
    }

    public static String ellipsize(String s) {
        // Only ellipsize release builds
        if (!Build.TYPE.equals("user")) {
            return s;
        }
        if (s == null) {
            return null;
        }
        if (s.length() < 3) {
            return s;
        }
        return s.charAt(0) + "⋯" + s.charAt(s.length() - 1);
    }

    public static void copyStream(InputStream is, OutputStream os, int bufferSize)
            throws IOException {
        if (is != null && os != null) {
            byte[] buffer = new byte[bufferSize];
            int bytesRead = 0;
            while ((bytesRead = is.read(buffer)) >= 0) {
                os.write(buffer, 0, bytesRead);
            }
        }
    }

    public static void safeCloseStream(InputStream is) {
        if (is != null) {
            try {
                is.close();
            } catch (Throwable t) {
                Log.d(TAG, "Error closing stream", t);
            }
        }
    }

    public static void safeCloseStream(OutputStream os) {
        if (os != null) {
            try {
                os.close();
            } catch (Throwable t) {
                Log.d(TAG, "Error closing stream", t);
            }
        }
    }

    static int sSystemUiUid = UserHandle.USER_NULL;
    public static void setSystemUiUid(int uid) {
        Utils.sSystemUiUid = uid;
    }

    static int sForegroundUserId = UserHandle.USER_NULL;
    public static void setForegroundUserId(int uid) {
        Utils.sForegroundUserId = uid;
    }

    public static void enforceBluetoothPermission(Context context) {
        context.enforceCallingOrSelfPermission(
                android.Manifest.permission.BLUETOOTH,
                "Need BLUETOOTH permission");
    }

    public static void enforceBluetoothAdminPermission(Context context) {
        context.enforceCallingOrSelfPermission(
                android.Manifest.permission.BLUETOOTH_ADMIN,
                "Need BLUETOOTH ADMIN permission");
    }

    public static void enforceBluetoothPrivilegedPermission(Context context) {
        context.enforceCallingOrSelfPermission(
                android.Manifest.permission.BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH PRIVILEGED permission");
    }

    public static void enforceLocalMacAddressPermission(Context context) {
        context.enforceCallingOrSelfPermission(
                android.Manifest.permission.LOCAL_MAC_ADDRESS,
                "Need LOCAL_MAC_ADDRESS permission");
    }

    public static void enforceDumpPermission(Context context) {
        context.enforceCallingOrSelfPermission(
                android.Manifest.permission.DUMP,
                "Need DUMP permission");
    }

    public static boolean callerIsSystemOrActiveUser(String tag, String method) {
        if (!checkCaller()) {
          Log.w(TAG, method + "() - Not allowed for non-active user and non-system user");
          return false;
        }
        return true;
    }

    public static boolean callerIsSystemOrActiveOrManagedUser(Context context, String tag, String method) {
        if (!checkCallerAllowManagedProfiles(context)) {
          Log.w(TAG, method + "() - Not allowed for non-active user and non-system and non-managed user");
          return false;
        }
        return true;
    }

    public static boolean checkCaller() {
        int callingUser = UserHandle.getCallingUserId();
        int callingUid = Binder.getCallingUid();
        return (sForegroundUserId == callingUser)
                || (UserHandle.getAppId(sSystemUiUid) == UserHandle.getAppId(callingUid))
                || (UserHandle.getAppId(Process.SYSTEM_UID) == UserHandle.getAppId(callingUid));
    }

    public static boolean checkCallerAllowManagedProfiles(Context mContext) {
        if (mContext == null) {
            return checkCaller();
        }
        int callingUser = UserHandle.getCallingUserId();
        int callingUid = Binder.getCallingUid();

        // Use the Bluetooth process identity when making call to get parent user
        long ident = Binder.clearCallingIdentity();
        try {
            UserManager um = (UserManager) mContext.getSystemService(Context.USER_SERVICE);
            UserInfo ui = um.getProfileParent(callingUser);
            int parentUser = (ui != null) ? ui.id : UserHandle.USER_NULL;

            // Always allow SystemUI/System access.
            return (sForegroundUserId == callingUser) || (sForegroundUserId == parentUser)
                    || (UserHandle.getAppId(sSystemUiUid) == UserHandle.getAppId(callingUid))
                    || (UserHandle.getAppId(Process.SYSTEM_UID) == UserHandle.getAppId(callingUid));
        } catch (Exception ex) {
            Log.e(TAG, "checkCallerAllowManagedProfiles: Exception ex=" + ex);
            return false;
        } finally {
            Binder.restoreCallingIdentity(ident);
        }
    }

    /**
     * Enforce the context has android.Manifest.permission.BLUETOOTH_ADMIN permission. A
     * {@link SecurityException} would be thrown if neither the calling process or the application
     * does not have BLUETOOTH_ADMIN permission.
     *
     * @param context Context for the permission check.
     */
    public static void enforceAdminPermission(ContextWrapper context) {
        context.enforceCallingOrSelfPermission(android.Manifest.permission.BLUETOOTH_ADMIN,
                "Need BLUETOOTH_ADMIN permission");
    }

    /**
     * Checks whether location is off and must be on for us to perform some operation
     */
    public static boolean blockedByLocationOff(Context context, UserHandle userHandle) {
        return !context.getSystemService(LocationManager.class)
                .isLocationEnabledForUser(userHandle);
    }

    /**
     * Checks that calling process has android.Manifest.permission.ACCESS_COARSE_LOCATION and
     * OP_COARSE_LOCATION is allowed
     */
    public static boolean checkCallerHasCoarseLocation(Context context, AppOpsManager appOps,
            String callingPackage, @Nullable String callingFeatureId, UserHandle userHandle) {
        if (blockedByLocationOff(context, userHandle)) {
            Log.e(TAG, "Permission denial: Location is off.");
            return false;
        }

        // Check coarse, but note fine
        if (context.checkCallingOrSelfPermission(
                android.Manifest.permission.ACCESS_COARSE_LOCATION)
                        == PackageManager.PERMISSION_GRANTED
                && isAppOppAllowed(appOps, AppOpsManager.OPSTR_FINE_LOCATION, callingPackage,
                callingFeatureId)) {
            return true;
        }

        Log.e(TAG, "Permission denial: Need ACCESS_COARSE_LOCATION "
                + "permission to get scan results");
        return false;
    }

    /**
     * Checks that calling process has android.Manifest.permission.ACCESS_COARSE_LOCATION and
     * OP_COARSE_LOCATION is allowed or android.Manifest.permission.ACCESS_FINE_LOCATION and
     * OP_FINE_LOCATION is allowed
     */
    public static boolean checkCallerHasCoarseOrFineLocation(Context context, AppOpsManager appOps,
            String callingPackage, @Nullable String callingFeatureId, UserHandle userHandle) {
        if (blockedByLocationOff(context, userHandle)) {
            Log.e(TAG, "Permission denial: Location is off.");
            return false;
        }

        if (context.checkCallingOrSelfPermission(
                android.Manifest.permission.ACCESS_FINE_LOCATION)
                        == PackageManager.PERMISSION_GRANTED
                && isAppOppAllowed(appOps, AppOpsManager.OPSTR_FINE_LOCATION, callingPackage,
                callingFeatureId)) {
            return true;
        }

        // Check coarse, but note fine
        if (context.checkCallingOrSelfPermission(
                android.Manifest.permission.ACCESS_COARSE_LOCATION)
                        == PackageManager.PERMISSION_GRANTED
                && isAppOppAllowed(appOps, AppOpsManager.OPSTR_FINE_LOCATION, callingPackage,
                callingFeatureId)) {
            return true;
        }

        Log.e(TAG, "Permission denial: Need ACCESS_COARSE_LOCATION or ACCESS_FINE_LOCATION"
                + "permission to get scan results");
        return false;
    }

    /**
     * Checks that calling process has android.Manifest.permission.ACCESS_FINE_LOCATION and
     * OP_FINE_LOCATION is allowed
     */
    public static boolean checkCallerHasFineLocation(Context context, AppOpsManager appOps,
            String callingPackage, @Nullable String callingFeatureId, UserHandle userHandle) {
        if (blockedByLocationOff(context, userHandle)) {
            Log.e(TAG, "Permission denial: Location is off.");
            return false;
        }

        if (context.checkCallingOrSelfPermission(
                android.Manifest.permission.ACCESS_FINE_LOCATION)
                        == PackageManager.PERMISSION_GRANTED
                && isAppOppAllowed(appOps, AppOpsManager.OPSTR_FINE_LOCATION, callingPackage,
                callingFeatureId)) {
            return true;
        }

        Log.e(TAG, "Permission denial: Need ACCESS_FINE_LOCATION "
                + "permission to get scan results");
        return false;
    }

    /**
     * Returns true if the caller holds NETWORK_SETTINGS
     */
    public static boolean checkCallerHasNetworkSettingsPermission(Context context) {
        return context.checkCallingOrSelfPermission(android.Manifest.permission.NETWORK_SETTINGS)
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Returns true if the caller holds NETWORK_SETUP_WIZARD
     */
    public static boolean checkCallerHasNetworkSetupWizardPermission(Context context) {
        return context.checkCallingOrSelfPermission(
                android.Manifest.permission.NETWORK_SETUP_WIZARD)
                        == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Returns true if the caller holds RADIO_SCAN_WITHOUT_LOCATION
     */
    public static boolean checkCallerHasScanWithoutLocationPermission(Context context) {
        return context.checkCallingOrSelfPermission(
                android.Manifest.permission.RADIO_SCAN_WITHOUT_LOCATION)
                == PackageManager.PERMISSION_GRANTED;
    }

    public static boolean isQApp(Context context, String pkgName) {
        try {
            return context.getPackageManager().getApplicationInfo(pkgName, 0).targetSdkVersion
                    >= Build.VERSION_CODES.Q;
        } catch (PackageManager.NameNotFoundException e) {
            // In case of exception, assume Q app
        }
        return true;
    }

    private static boolean isAppOppAllowed(AppOpsManager appOps, String op, String callingPackage,
            @NonNull String callingFeatureId) {
        return appOps.noteOp(op, Binder.getCallingUid(), callingPackage, callingFeatureId, null)
                == AppOpsManager.MODE_ALLOWED;
    }

    /**
     * Converts {@code millisecond} to unit. Each unit is 0.625 millisecond.
     */
    public static int millsToUnit(int milliseconds) {
        return (int) (TimeUnit.MILLISECONDS.toMicros(milliseconds) / MICROS_PER_UNIT);
    }

    /**
     * Check if we are running in BluetoothInstrumentationTest context by trying to load
     * com.android.bluetooth.FileSystemWriteTest. If we are not in Instrumentation test mode, this
     * class should not be found. Thus, the assumption is that FileSystemWriteTest must exist.
     * If FileSystemWriteTest is removed in the future, another test class in
     * BluetoothInstrumentationTest should be used instead
     *
     * @return true if in BluetoothInstrumentationTest, false otherwise
     */
    public static boolean isInstrumentationTestMode() {
        try {
            return Class.forName("com.android.bluetooth.FileSystemWriteTest") != null;
        } catch (ClassNotFoundException exception) {
            return false;
        }
    }

    /**
     * Throws {@link IllegalStateException} if we are not in BluetoothInstrumentationTest. Useful
     * for ensuring certain methods only get called in BluetoothInstrumentationTest
     */
    public static void enforceInstrumentationTestMode() {
        if (!isInstrumentationTestMode()) {
            throw new IllegalStateException("Not in BluetoothInstrumentationTest");
        }
    }

    /**
     * Check if we are running in PTS test mode. To enable/disable PTS test mode, invoke
     * {@code adb shell setprop persist.bluetooth.pts true/false}
     *
     * @return true if in PTS Test mode, false otherwise
     */
    public static boolean isPtsTestMode() {
        return SystemProperties.getBoolean(PTS_TEST_MODE_PROPERTY, false);
    }

    /**
     * Get uid/pid string in a binder call
     *
     * @return "uid/pid=xxxx/yyyy"
     */
    public static String getUidPidString() {
        return "uid/pid=" + Binder.getCallingUid() + "/" + Binder.getCallingPid();
    }

    /**
     * Get system local time
     *
     * @return "MM-dd HH:mm:ss.SSS"
     */
    public static String getLocalTimeString() {
        return DateTimeFormatter.ofPattern("MM-dd HH:mm:ss.SSS")
                .withZone(ZoneId.systemDefault()).format(Instant.now());
    }

    public static void skipCurrentTag(XmlPullParser parser)
            throws XmlPullParserException, IOException {
        int outerDepth = parser.getDepth();
        int type;
        while ((type = parser.next()) != XmlPullParser.END_DOCUMENT
                && (type != XmlPullParser.END_TAG
                || parser.getDepth() > outerDepth)) {
        }
    }

    /**
     * Converts pause and tonewait pause characters
     * to Android representation.
     * RFC 3601 says pause is 'p' and tonewait is 'w'.
     */
    public static String convertPreDial(String phoneNumber) {
        if (phoneNumber == null) {
            return null;
        }
        int len = phoneNumber.length();
        StringBuilder ret = new StringBuilder(len);

        for (int i = 0; i < len; i++) {
            char c = phoneNumber.charAt(i);

            if (isPause(c)) {
                c = PAUSE;
            } else if (isToneWait(c)) {
                c = WAIT;
            }
            ret.append(c);
        }
        return ret.toString();
    }

    /**
     * Move a message to the given folder.
     *
     * @param context the context to use
     * @param uri the message to move
     * @param messageSent if the message is SENT or FAILED
     * @return true if the operation succeeded
     */
    public static boolean moveMessageToFolder(Context context, Uri uri, boolean messageSent) {
        if (uri == null) {
            return false;
        }

        ContentValues values = new ContentValues(3);
        if (messageSent) {
            values.put(Telephony.Sms.READ, 1);
            values.put(Telephony.Sms.TYPE, Telephony.Sms.MESSAGE_TYPE_SENT);
        } else {
            values.put(Telephony.Sms.READ, 0);
            values.put(Telephony.Sms.TYPE, Telephony.Sms.MESSAGE_TYPE_FAILED);
        }
        values.put(Telephony.Sms.ERROR_CODE, 0);

        return 1 == context.getContentResolver().update(uri, values, null, null);
    }
}
