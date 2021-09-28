/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.cellbroadcastreceiver;

import android.content.Context;
import android.database.Cursor;
import android.provider.Telephony;
import android.telephony.SmsCbCmasInfo;
import android.telephony.SmsCbEtwsInfo;
import android.telephony.SmsCbLocation;
import android.telephony.SmsCbMessage;
import android.telephony.SubscriptionManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CursorAdapter;

/**
 * The back-end data adapter for {@link CellBroadcastListActivity}.
 */
public class CellBroadcastCursorAdapter extends CursorAdapter {

    public CellBroadcastCursorAdapter(Context context) {
        // don't set FLAG_AUTO_REQUERY or FLAG_REGISTER_CONTENT_OBSERVER
        super(context, null, 0);
    }

    /**
     * Makes a new view to hold the data pointed to by cursor.
     * @param context Interface to application's global information
     * @param cursor The cursor from which to get the data. The cursor is already
     * moved to the correct position.
     * @param parent The parent to which the new view is attached to
     * @return the newly created view.
     */
    @Override
    public View newView(Context context, Cursor cursor, ViewGroup parent) {
        SmsCbMessage message = createFromCursor(context, cursor);

        LayoutInflater factory = LayoutInflater.from(context);
        CellBroadcastListItem listItem = (CellBroadcastListItem) factory.inflate(
                    R.layout.cell_broadcast_list_item, parent, false);

        listItem.bind(message);
        return listItem;
    }

    static SmsCbMessage createFromCursor(Context context, Cursor cursor) {
        int geoScope = cursor.getInt(
                cursor.getColumnIndexOrThrow(Telephony.CellBroadcasts.GEOGRAPHICAL_SCOPE));
        int serialNum = cursor.getInt(
                cursor.getColumnIndexOrThrow(Telephony.CellBroadcasts.SERIAL_NUMBER));
        int category = cursor.getInt(
                cursor.getColumnIndexOrThrow(Telephony.CellBroadcasts.SERVICE_CATEGORY));
        String language = cursor.getString(
                cursor.getColumnIndexOrThrow(Telephony.CellBroadcasts.LANGUAGE_CODE));
        String body = cursor.getString(
                cursor.getColumnIndexOrThrow(Telephony.CellBroadcasts.MESSAGE_BODY));
        int format = cursor.getInt(
                cursor.getColumnIndexOrThrow(Telephony.CellBroadcasts.MESSAGE_FORMAT));
        int priority = cursor.getInt(
                cursor.getColumnIndexOrThrow(Telephony.CellBroadcasts.MESSAGE_PRIORITY));
        int slotIndex = cursor.getInt(
                cursor.getColumnIndexOrThrow(Telephony.CellBroadcasts.SLOT_INDEX));

        String plmn;
        int plmnColumn = cursor.getColumnIndex(Telephony.CellBroadcasts.PLMN);
        if (plmnColumn != -1 && !cursor.isNull(plmnColumn)) {
            plmn = cursor.getString(plmnColumn);
        } else {
            plmn = null;
        }

        int lac;
        int lacColumn = cursor.getColumnIndex(Telephony.CellBroadcasts.LAC);
        if (lacColumn != -1 && !cursor.isNull(lacColumn)) {
            lac = cursor.getInt(lacColumn);
        } else {
            lac = -1;
        }

        int cid;
        int cidColumn = cursor.getColumnIndex(Telephony.CellBroadcasts.CID);
        if (cidColumn != -1 && !cursor.isNull(cidColumn)) {
            cid = cursor.getInt(cidColumn);
        } else {
            cid = -1;
        }

        SmsCbLocation location = new SmsCbLocation(plmn, lac, cid);

        SmsCbEtwsInfo etwsInfo;
        int etwsWarningTypeColumn = cursor.getColumnIndex(
                Telephony.CellBroadcasts.ETWS_WARNING_TYPE);
        if (etwsWarningTypeColumn != -1 && !cursor.isNull(etwsWarningTypeColumn)) {
            int warningType = cursor.getInt(etwsWarningTypeColumn);
            etwsInfo = new SmsCbEtwsInfo(warningType, false, false, false, null);
        } else {
            etwsInfo = null;
        }

        SmsCbCmasInfo cmasInfo;
        int cmasMessageClassColumn = cursor.getColumnIndex(
                Telephony.CellBroadcasts.CMAS_MESSAGE_CLASS);
        if (cmasMessageClassColumn != -1 && !cursor.isNull(cmasMessageClassColumn)) {
            int messageClass = cursor.getInt(cmasMessageClassColumn);

            int cmasCategory;
            int cmasCategoryColumn = cursor.getColumnIndex(
                    Telephony.CellBroadcasts.CMAS_CATEGORY);
            if (cmasCategoryColumn != -1 && !cursor.isNull(cmasCategoryColumn)) {
                cmasCategory = cursor.getInt(cmasCategoryColumn);
            } else {
                cmasCategory = SmsCbCmasInfo.CMAS_CATEGORY_UNKNOWN;
            }

            int responseType;
            int cmasResponseTypeColumn = cursor.getColumnIndex(
                    Telephony.CellBroadcasts.CMAS_RESPONSE_TYPE);
            if (cmasResponseTypeColumn != -1 && !cursor.isNull(cmasResponseTypeColumn)) {
                responseType = cursor.getInt(cmasResponseTypeColumn);
            } else {
                responseType = SmsCbCmasInfo.CMAS_RESPONSE_TYPE_UNKNOWN;
            }

            int severity;
            int cmasSeverityColumn = cursor.getColumnIndex(
                    Telephony.CellBroadcasts.CMAS_SEVERITY);
            if (cmasSeverityColumn != -1 && !cursor.isNull(cmasSeverityColumn)) {
                severity = cursor.getInt(cmasSeverityColumn);
            } else {
                severity = SmsCbCmasInfo.CMAS_SEVERITY_UNKNOWN;
            }

            int urgency;
            int cmasUrgencyColumn = cursor.getColumnIndex(
                    Telephony.CellBroadcasts.CMAS_URGENCY);
            if (cmasUrgencyColumn != -1 && !cursor.isNull(cmasUrgencyColumn)) {
                urgency = cursor.getInt(cmasUrgencyColumn);
            } else {
                urgency = SmsCbCmasInfo.CMAS_URGENCY_UNKNOWN;
            }

            int certainty;
            int cmasCertaintyColumn = cursor.getColumnIndex(
                    Telephony.CellBroadcasts.CMAS_CERTAINTY);
            if (cmasCertaintyColumn != -1 && !cursor.isNull(cmasCertaintyColumn)) {
                certainty = cursor.getInt(cmasCertaintyColumn);
            } else {
                certainty = SmsCbCmasInfo.CMAS_CERTAINTY_UNKNOWN;
            }

            cmasInfo = new SmsCbCmasInfo(messageClass, cmasCategory, responseType, severity,
                    urgency, certainty);
        } else {
            cmasInfo = null;
        }

        String timeColumn = null;
        if (cursor.getColumnIndex(Telephony.CellBroadcasts.DELIVERY_TIME) >= 0) {
            timeColumn = Telephony.CellBroadcasts.DELIVERY_TIME;
        } else if (cursor.getColumnIndex(Telephony.CellBroadcasts.RECEIVED_TIME) >= 0) {
            timeColumn = Telephony.CellBroadcasts.RECEIVED_TIME;
        }

        long time = cursor.getLong(cursor.getColumnIndexOrThrow(timeColumn));

        int dcs = 0;
        if (cursor.getColumnIndex(Telephony.CellBroadcasts.DATA_CODING_SCHEME) >= 0) {
            dcs = cursor.getInt(cursor.getColumnIndexOrThrow(
                    Telephony.CellBroadcasts.DATA_CODING_SCHEME));
        }

        SubscriptionManager sm = (SubscriptionManager) context.getSystemService(
                Context.TELEPHONY_SUBSCRIPTION_SERVICE);
        int subId = SubscriptionManager.DEFAULT_SUBSCRIPTION_ID;
        int[] subIds = sm.getSubscriptionIds(slotIndex);
        if (subIds != null && subIds.length > 0) {
            subId = subIds[0];
        }

        int maximumWaitTimeSec = 0;
        if (cursor.getColumnIndex(Telephony.CellBroadcasts.MAXIMUM_WAIT_TIME) >= 0) {
            maximumWaitTimeSec = cursor.getInt(cursor.getColumnIndexOrThrow(
                    Telephony.CellBroadcasts.MAXIMUM_WAIT_TIME));
        }

        return new SmsCbMessage(format, geoScope, serialNum, location, category, language, dcs,
                body, priority, etwsInfo, cmasInfo, maximumWaitTimeSec, null, time,
                slotIndex, subId);
    }

    /**
     * Bind an existing view to the data pointed to by cursor
     * @param view Existing view, returned earlier by newView
     * @param context Interface to application's global information
     * @param cursor The cursor from which to get the data. The cursor is already
     * moved to the correct position.
     */
    @Override
    public void bindView(View view, Context context, Cursor cursor) {
        SmsCbMessage message = createFromCursor(context, cursor);
        CellBroadcastListItem listItem = (CellBroadcastListItem) view;
        listItem.bind(message);
    }
}
