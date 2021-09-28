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

package com.google.android.textclassifier;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Java wrapper for LangId native library interface. This class is used to detect languages in text.
 *
 * @hide
 */
public final class LangIdModel implements AutoCloseable {
  private final AtomicBoolean isClosed = new AtomicBoolean(false);

  static {
    System.loadLibrary("textclassifier");
  }

  private long modelPtr;

  /** Creates a new instance of LangId predictor, using the provided model image. */
  public LangIdModel(int fd) {
    modelPtr = nativeNew(fd);
    if (modelPtr == 0L) {
      throw new IllegalArgumentException("Couldn't initialize LangId from given file descriptor.");
    }
  }

  /** Creates a new instance of LangId predictor, using the provided model image. */
  public LangIdModel(String modelPath) {
    modelPtr = nativeNewFromPath(modelPath);
    if (modelPtr == 0L) {
      throw new IllegalArgumentException("Couldn't initialize LangId from given file.");
    }
  }

  /** Detects the languages for given text. */
  public LanguageResult[] detectLanguages(String text) {
    return nativeDetectLanguages(modelPtr, text);
  }

  /** Frees up the allocated memory. */
  @Override
  public void close() {
    if (isClosed.compareAndSet(false, true)) {
      nativeClose(modelPtr);
      modelPtr = 0L;
    }
  }

  @Override
  protected void finalize() throws Throwable {
    try {
      close();
    } finally {
      super.finalize();
    }
  }

  /** Result for detectLanguages method. */
  public static final class LanguageResult {
    final String mLanguage;
    final float mScore;

    LanguageResult(String language, float score) {
      mLanguage = language;
      mScore = score;
    }

    public final String getLanguage() {
      return mLanguage;
    }

    public final float getScore() {
      return mScore;
    }
  }

  /** Returns the version of the LangId model used. */
  public int getVersion() {
    return nativeGetVersion(modelPtr);
  }

  public float getLangIdThreshold() {
    return nativeGetLangIdThreshold(modelPtr);
  }

  public static int getVersion(int fd) {
    return nativeGetVersionFromFd(fd);
  }

  /** Retrieves the pointer to the native object. */
  long getNativePointer() {
    return modelPtr;
  }

  // Visible for testing.
  float getLangIdNoiseThreshold() {
    return nativeGetLangIdNoiseThreshold(modelPtr);
  }

  // Visible for testing.
  int getMinTextSizeInBytes() {
    return nativeGetMinTextSizeInBytes(modelPtr);
  }

  /**
   * Returns the pointer to the native object. Note: Need to keep the LangIdModel alive as long as
   * the pointer is used.
   */
  long getNativeLangIdPointer() {
    return modelPtr;
  }

  private static native long nativeNew(int fd);

  private static native long nativeNewFromPath(String path);

  private native LanguageResult[] nativeDetectLanguages(long nativePtr, String text);

  private native void nativeClose(long nativePtr);

  private native int nativeGetVersion(long nativePtr);

  private static native int nativeGetVersionFromFd(int fd);

  private native float nativeGetLangIdThreshold(long nativePtr);

  private native float nativeGetLangIdNoiseThreshold(long nativePtr);

  private native int nativeGetMinTextSizeInBytes(long nativePtr);
}
