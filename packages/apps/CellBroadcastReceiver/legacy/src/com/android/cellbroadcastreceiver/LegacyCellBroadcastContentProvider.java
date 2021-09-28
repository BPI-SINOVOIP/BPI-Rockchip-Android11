package com.android.cellbroadcastreceiver;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.provider.Telephony;
import android.provider.Telephony.CellBroadcasts;
import android.util.Log;
import java.util.Arrays;
import java.util.List;

/**
 * Very limited subset of function which is only used to surfaces data.
 */
public class LegacyCellBroadcastContentProvider extends ContentProvider {
    // shared preference under developer settings
    private static final String ENABLE_ALERT_MASTER_PREF = "enable_alerts_master_toggle";

    private static final String TAG = LegacyCellBroadcastContentProvider.class.getSimpleName();
    /** A list of preference supported by legacy app **/
    private static final List<String> PREF_KEYS = Arrays.asList(
            CellBroadcasts.Preference.ENABLE_CMAS_AMBER_PREF,
            CellBroadcasts.Preference.ENABLE_AREA_UPDATE_INFO_PREF,
            CellBroadcasts.Preference.ENABLE_TEST_ALERT_PREF,
            CellBroadcasts.Preference.ENABLE_STATE_LOCAL_TEST_PREF,
            CellBroadcasts.Preference.ENABLE_PUBLIC_SAFETY_PREF,
            CellBroadcasts.Preference.ENABLE_CMAS_SEVERE_THREAT_PREF,
            CellBroadcasts.Preference.ENABLE_CMAS_EXTREME_THREAT_PREF,
            CellBroadcasts.Preference.ENABLE_CMAS_PRESIDENTIAL_PREF,
            CellBroadcasts.Preference.ENABLE_EMERGENCY_PERF,
            CellBroadcasts.Preference.ENABLE_ALERT_VIBRATION_PREF,
            CellBroadcasts.Preference.ENABLE_CMAS_IN_SECOND_LANGUAGE_PREF,
            ENABLE_ALERT_MASTER_PREF
    );

    /** The database for this content provider. */
    private SQLiteOpenHelper mOpenHelper;

    @Override
    public boolean onCreate() {
        Log.d(TAG, "onCreate");
        mOpenHelper = new CellBroadcastDatabaseHelper(getContext(), true);
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projectionIn, String selection,
            String[] selectionArgs, String sortOrder) {
        Log.d(TAG, "query:"
                + " uri=" + uri
                + " values=" + Arrays.toString(projectionIn)
                + " selection=" + selection
                + " selectionArgs=" + Arrays.toString(selectionArgs));

        SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
        qb.setTables(CellBroadcastDatabaseHelper.TABLE_NAME);
        SQLiteDatabase db = mOpenHelper.getReadableDatabase();
        Cursor c = qb.query(db, projectionIn, selection, selectionArgs, null, null, sortOrder);
        Log.d(TAG, "query from legacy cellbroadcast, returned " + c.getCount() + " messages");
        return c;
    }

    @Override
    public Bundle call(String method, String name, Bundle args) {
        Log.d(TAG, "call:"
                + " method=" + method
                + " name=" + name
                + " args=" + args);
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(getContext());
        if (Telephony.CellBroadcasts.CALL_METHOD_GET_PREFERENCE.equals(method)) {
            if (PREF_KEYS.contains(name)) {
                // if preference value does not exists, return null.
                if (sp != null && sp.contains(name)) {
                    Bundle result = new Bundle();
                    result.putBoolean(name, sp.getBoolean(name, true));
                    Log.d(TAG, "migrate sharedpreference: " + name + " val: " + result.get(name));
                    return result;
                }
            } else {
                Log.e(TAG, "unsupported preference name" + name);
            }
        } else {
            Log.e(TAG, "unsuppprted call method: " + method);
        }
        return null;
    }

    @Override
    public String getType(Uri uri) {
        Log.d(TAG, "getType");
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException("insert not supported");
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException("delete not supported");
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException("update not supported");
    }
}