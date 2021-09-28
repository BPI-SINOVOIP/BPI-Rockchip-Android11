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

package android.telephonyprovider.cts;

/**
 * Replicates some constants from {@link com.google.android.mms.pdu.PduHeaders}, which we do not use
 * in order to avoid needing to access private platform APIs from this test package. These constants
 * are standardized externally so the duplication should be robust.
 */
public interface PduHeaders {
    /**
     * X-Mms-Message-Type field types.
     */
    int MESSAGE_TYPE_SEND_REQ           = 0x80;
    int MESSAGE_TYPE_SEND_CONF          = 0x81;
    int MESSAGE_TYPE_NOTIFICATION_IND   = 0x82;
    int MESSAGE_TYPE_NOTIFYRESP_IND     = 0x83;
    int MESSAGE_TYPE_RETRIEVE_CONF      = 0x84;
    int MESSAGE_TYPE_ACKNOWLEDGE_IND    = 0x85;
    int MESSAGE_TYPE_DELIVERY_IND       = 0x86;
    int MESSAGE_TYPE_READ_REC_IND       = 0x87;
    int MESSAGE_TYPE_READ_ORIG_IND      = 0x88;
    int MESSAGE_TYPE_FORWARD_REQ        = 0x89;
    int MESSAGE_TYPE_FORWARD_CONF       = 0x8A;
    int MESSAGE_TYPE_MBOX_STORE_REQ     = 0x8B;
    int MESSAGE_TYPE_MBOX_STORE_CONF    = 0x8C;
    int MESSAGE_TYPE_MBOX_VIEW_REQ      = 0x8D;
    int MESSAGE_TYPE_MBOX_VIEW_CONF     = 0x8E;
    int MESSAGE_TYPE_MBOX_UPLOAD_REQ    = 0x8F;
    int MESSAGE_TYPE_MBOX_UPLOAD_CONF   = 0x90;
    int MESSAGE_TYPE_MBOX_DELETE_REQ    = 0x91;
    int MESSAGE_TYPE_MBOX_DELETE_CONF   = 0x92;
    int MESSAGE_TYPE_MBOX_DESCR         = 0x93;
    int MESSAGE_TYPE_DELETE_REQ         = 0x94;
    int MESSAGE_TYPE_DELETE_CONF        = 0x95;
    int MESSAGE_TYPE_CANCEL_REQ         = 0x96;
    int MESSAGE_TYPE_CANCEL_CONF        = 0x97;
}
