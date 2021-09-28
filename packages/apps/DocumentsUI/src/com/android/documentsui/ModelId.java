package com.android.documentsui;

import static com.android.documentsui.base.DocumentInfo.getCursorInt;
import static com.android.documentsui.base.DocumentInfo.getCursorString;

import android.database.Cursor;
import android.provider.DocumentsContract;

import com.android.documentsui.base.UserId;
import com.android.documentsui.roots.RootCursorWrapper;

public class ModelId {

    public static final String build(Cursor cursor) {
        if (cursor == null) {
            return null;
        }
        return ModelId.build(UserId.of(getCursorInt(cursor, RootCursorWrapper.COLUMN_USER_ID)),
                getCursorString(cursor, RootCursorWrapper.COLUMN_AUTHORITY),
                getCursorString(cursor, DocumentsContract.Document.COLUMN_DOCUMENT_ID));
    }

    public static final String build(UserId userId, String authority, String docId) {
        if (userId == null || authority == null || authority.isEmpty() || docId == null
                || docId.isEmpty()) {
            return null;
        }
        return userId + "|" + authority + "|" + docId;
    }
}
