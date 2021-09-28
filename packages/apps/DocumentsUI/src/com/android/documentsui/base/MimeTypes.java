/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.documentsui.base;

import android.provider.DocumentsContract.Document;
import android.util.ArraySet;

import androidx.annotation.Nullable;

import java.util.List;
import java.util.Set;

public final class MimeTypes {

    private MimeTypes() {
    }

    public static final String APK_TYPE = "application/vnd.android.package-archive";
    public static final String GENERIC_TYPE = "application/*";

    public static final String IMAGE_MIME = "image/*";
    public static final String AUDIO_MIME = "audio/*";
    public static final String VIDEO_MIME = "video/*";

    private static final Set<String> sDocumentsMimeTypes = new ArraySet<>();

    static {
        // all lower case
        sDocumentsMimeTypes.add("application/epub+zip");
        sDocumentsMimeTypes.add("application/msword");
        sDocumentsMimeTypes.add("application/pdf");
        sDocumentsMimeTypes.add("application/rtf");
        sDocumentsMimeTypes.add("application/vnd.ms-excel");
        sDocumentsMimeTypes.add("application/vnd.ms-excel.addin.macroenabled.12");
        sDocumentsMimeTypes.add("application/vnd.ms-excel.sheet.binary.macroenabled.12");
        sDocumentsMimeTypes.add("application/vnd.ms-excel.sheet.macroenabled.12");
        sDocumentsMimeTypes.add("application/vnd.ms-excel.template.macroenabled.12");
        sDocumentsMimeTypes.add("application/vnd.ms-powerpoint");
        sDocumentsMimeTypes.add("application/vnd.ms-powerpoint.addin.macroenabled.12");
        sDocumentsMimeTypes.add("application/vnd.ms-powerpoint.presentation.macroenabled.12");
        sDocumentsMimeTypes.add("application/vnd.ms-powerpoint.slideshow.macroenabled.12");
        sDocumentsMimeTypes.add("application/vnd.ms-powerpoint.template.macroenabled.12");
        sDocumentsMimeTypes.add("application/vnd.ms-word.document.macroenabled.12");
        sDocumentsMimeTypes.add("application/vnd.ms-word.template.macroenabled.12");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.chart");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.database");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.formula");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.graphics");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.graphics-template");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.presentation");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.presentation-template");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.spreadsheet");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.spreadsheet-template");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.text");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.text-master");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.text-template");
        sDocumentsMimeTypes.add("application/vnd.oasis.opendocument.text-web");
        sDocumentsMimeTypes.add(
                "application/vnd.openxmlformats-officedocument.presentationml.presentation");
        sDocumentsMimeTypes.add(
                "application/vnd.openxmlformats-officedocument.presentationml.slideshow");
        sDocumentsMimeTypes.add(
                "application/vnd.openxmlformats-officedocument.presentationml.template");
        sDocumentsMimeTypes.add(
                "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
        sDocumentsMimeTypes.add(
                "application/vnd.openxmlformats-officedocument.spreadsheetml.template");
        sDocumentsMimeTypes.add(
                "application/vnd.openxmlformats-officedocument.wordprocessingml.document");
        sDocumentsMimeTypes.add(
                "application/vnd.openxmlformats-officedocument.wordprocessingml.template");
        sDocumentsMimeTypes.add("application/vnd.stardivision.calc");
        sDocumentsMimeTypes.add("application/vnd.stardivision.chart");
        sDocumentsMimeTypes.add("application/vnd.stardivision.draw");
        sDocumentsMimeTypes.add("application/vnd.stardivision.impress");
        sDocumentsMimeTypes.add("application/vnd.stardivision.impress-packed");
        sDocumentsMimeTypes.add("application/vnd.stardivision.mail");
        sDocumentsMimeTypes.add("application/vnd.stardivision.math");
        sDocumentsMimeTypes.add("application/vnd.stardivision.writer");
        sDocumentsMimeTypes.add("application/vnd.stardivision.writer-global");
        sDocumentsMimeTypes.add("application/vnd.sun.xml.calc");
        sDocumentsMimeTypes.add("application/vnd.sun.xml.calc.template");
        sDocumentsMimeTypes.add("application/vnd.sun.xml.draw");
        sDocumentsMimeTypes.add("application/vnd.sun.xml.draw.template");
        sDocumentsMimeTypes.add("application/vnd.sun.xml.impress");
        sDocumentsMimeTypes.add("application/vnd.sun.xml.impress.template");
        sDocumentsMimeTypes.add("application/vnd.sun.xml.math");
        sDocumentsMimeTypes.add("application/vnd.sun.xml.writer");
        sDocumentsMimeTypes.add("application/vnd.sun.xml.writer.global");
        sDocumentsMimeTypes.add("application/vnd.sun.xml.writer.template");
        sDocumentsMimeTypes.add("application/x-mspublisher");
        sDocumentsMimeTypes.add("text/*");
    }

    /**
     * Get the Document mime type array
     *
     * @return the mime type array of document
     */
    public static String[] getDocumentMimeTypeArray() {
        return sDocumentsMimeTypes.toArray((new String[0]));
    }

    /**
     * MIME types that are visual in nature. For example, they should always be shown as thumbnails
     * in list mode.
     */
    public static final String[] VISUAL_MIMES = new String[]{IMAGE_MIME, VIDEO_MIME};

    public static @Nullable String[] splitMimeType(String mimeType) {
        final String[] groups = mimeType.split("/");

        if (groups.length != 2 || groups[0].isEmpty() || groups[1].isEmpty()) {
            return null;
        }

        return groups;
    }

    public static String findCommonMimeType(List<String> mimeTypes) {
        String[] commonType = splitMimeType(mimeTypes.get(0));
        if (commonType == null) {
            return "*/*";
        }

        for (int i = 1; i < mimeTypes.size(); i++) {
            String[] type = mimeTypes.get(i).split("/");
            if (type.length != 2) continue;

            if (!commonType[1].equals(type[1])) {
                commonType[1] = "*";
            }

            if (!commonType[0].equals(type[0])) {
                commonType[0] = "*";
                commonType[1] = "*";
                break;
            }
        }

        return commonType[0] + "/" + commonType[1];
    }

    public static boolean mimeMatches(String[] filters, String[] tests) {
        if (tests == null) {
            return false;
        }
        for (String test : tests) {
            if (mimeMatches(filters, test)) {
                return true;
            }
        }
        return false;
    }

    public static boolean mimeMatches(String filter, String[] tests) {
        if (tests == null) {
            return true;
        }
        for (String test : tests) {
            if (mimeMatches(filter, test)) {
                return true;
            }
        }
        return false;
    }

    public static boolean mimeMatches(String[] filters, String test) {
        if (filters == null) {
            return true;
        }
        for (String filter : filters) {
            if (mimeMatches(filter, test)) {
                return true;
            }
        }
        return false;
    }

    public static boolean mimeMatches(String filter, String test) {
        if (test == null) {
            return false;
        } else if (filter == null || "*/*".equals(filter)) {
            return true;
        } else if (filter.equals(test)) {
            return true;
        } else if (filter.endsWith("/*")) {
            return filter.regionMatches(0, test, 0, filter.indexOf('/'));
        } else {
            return false;
        }
    }

    public static boolean isApkType(@Nullable String mimeType) {
        return APK_TYPE.equals(mimeType);
    }

    public static boolean isDirectoryType(@Nullable String mimeType) {
        return Document.MIME_TYPE_DIR.equals(mimeType);
    }
}
