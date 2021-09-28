/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.graphics.fonts;

import android.os.LocaleList;
import android.util.Pair;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;

public class NativeSystemFontHelper {
    static {
        System.loadLibrary("ctsgraphics_jni");
    }

    /**
     *  Helper class for representing the system font obtained in native code.
     */
    public static class FontDescriptor {
        String mFilePath;
        int mWeight;
        int mSlant;
        int mIndex;
        FontVariationAxis[] mAxes;
        LocaleList mLocale;

        @Override
        public boolean equals(Object o) {
            if (o == this) {
                return true;
            }
            if (o == null || !(o instanceof FontDescriptor)) {
                return false;
            }
            FontDescriptor f = (FontDescriptor) o;
            return f.mFilePath.equals(mFilePath)
                && f.mWeight == mWeight
                && f.mSlant == mSlant
                && f.mIndex == mIndex
                && Arrays.equals(f.mAxes, mAxes)
                && Objects.equals(f.mLocale, mLocale);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mFilePath, mWeight, mSlant, mIndex, Arrays.hashCode(mAxes),
                mLocale);
        }

        @Override
        public String toString() {
            return "Font {"
                + " path = " + mFilePath
                + " weight = " + mWeight
                + " slant = " + mSlant
                + " index = " + mIndex
                + " axes = " + FontVariationAxis.toFontVariationSettings(mAxes)
                + " locale = " + mLocale
                + "}";
        }
    }

    private static String tagToStr(int tag) {
        char[] buf = new char[4];
        buf[0] = (char) ((tag >> 24) & 0xFF);
        buf[1] = (char) ((tag >> 16) & 0xFF);
        buf[2] = (char) ((tag >> 8) & 0xFF);
        buf[3] = (char) (tag & 0xFF);
        return String.valueOf(buf);
    }

    public static Set<FontDescriptor> getAvailableFonts() {
        long iterPtr = nOpenIterator();
        HashSet<FontDescriptor> nativeFonts = new HashSet<>();
        try {
            for (long fontPtr = nNext(iterPtr); fontPtr != 0; fontPtr = nNext(iterPtr)) {
                try {
                    FontDescriptor font = new FontDescriptor();
                    font.mFilePath = nGetFilePath(fontPtr);
                    font.mWeight = nGetWeight(fontPtr);
                    font.mSlant = nIsItalic(fontPtr)
                        ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;
                    font.mIndex = nGetCollectionIndex(fontPtr);
                    font.mAxes = new FontVariationAxis[nGetAxisCount(fontPtr)];
                    for (int i = 0; i < font.mAxes.length; ++i) {
                        font.mAxes[i] = new FontVariationAxis(
                            tagToStr(nGetAxisTag(fontPtr, i)), nGetAxisValue(fontPtr, i));
                    }
                    font.mLocale = LocaleList.forLanguageTags(nGetLocale(fontPtr));
                    nativeFonts.add(font);
                } finally {
                    nCloseFont(fontPtr);
                }
            }
        } finally {
            nCloseIterator(iterPtr);
        }
        return nativeFonts;
    }

    public static Pair<File, Integer> matchFamilyStyleCharacter(String familyName, int weight,
            boolean italic, String languageTags, int familyVariant, String text) {
        final long fontPtr = nMatchFamilyStyleCharacter(familyName, weight, italic, languageTags,
                familyVariant, text);
        final int runLength = nMatchFamilyStyleCharacter_runLength(familyName, weight, italic,
                languageTags, familyVariant, text);
        try {
            return new Pair<>(new File(nGetFilePath(fontPtr)), runLength);
        } finally {
            nCloseFont(fontPtr);
        }
    }

    private static native long nOpenIterator();
    private static native void nCloseIterator(long ptr);
    private static native long nNext(long ptr);
    private static native void nCloseFont(long ptr);
    private static native String nGetFilePath(long ptr);
    private static native int nGetWeight(long ptr);
    private static native boolean nIsItalic(long ptr);
    private static native String nGetLocale(long ptr);
    private static native int nGetCollectionIndex(long ptr);
    private static native int nGetAxisCount(long ptr);
    private static native int nGetAxisTag(long ptr, int index);
    private static native float nGetAxisValue(long ptr, int index);
    private static native long nMatchFamilyStyleCharacter(String familyName, int weight,
            boolean italic, String languageTags, int familyVariant, String text);
    private static native int nMatchFamilyStyleCharacter_runLength(String familyName, int weight,
            boolean italic, String languageTags, int familyVariant, String text);
}
