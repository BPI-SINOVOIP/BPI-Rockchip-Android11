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
 * Java wrapper for ActionsSuggestions native library interface. This library is used to suggest
 * actions and replies in a given conversation.
 *
 * @hide
 */
public final class ActionsSuggestionsModel implements AutoCloseable {
  private final AtomicBoolean isClosed = new AtomicBoolean(false);

  static {
    System.loadLibrary("textclassifier");
  }

  private long actionsModelPtr;

  /**
   * Creates a new instance of Actions predictor, using the provided model image, given as a file
   * descriptor.
   */
  public ActionsSuggestionsModel(int fileDescriptor, byte[] serializedPreconditions) {
    actionsModelPtr = nativeNewActionsModel(fileDescriptor, serializedPreconditions);
    if (actionsModelPtr == 0L) {
      throw new IllegalArgumentException("Couldn't initialize actions model from file descriptor.");
    }
  }

  public ActionsSuggestionsModel(int fileDescriptor) {
    this(fileDescriptor, /* serializedPreconditions= */ null);
  }

  /**
   * Creates a new instance of Actions predictor, using the provided model image, given as a file
   * path.
   */
  public ActionsSuggestionsModel(String path, byte[] serializedPreconditions) {
    actionsModelPtr = nativeNewActionsModelFromPath(path, serializedPreconditions);
    if (actionsModelPtr == 0L) {
      throw new IllegalArgumentException("Couldn't initialize actions model from given file.");
    }
  }

  public ActionsSuggestionsModel(String path) {
    this(path, /* serializedPreconditions= */ null);
  }

  /** Suggests actions / replies to the given conversation. */
  public ActionSuggestion[] suggestActions(
      Conversation conversation, ActionSuggestionOptions options, AnnotatorModel annotator) {
    return nativeSuggestActions(
        actionsModelPtr,
        conversation,
        options,
        (annotator != null ? annotator.getNativeAnnotatorPointer() : 0),
        /* appContext= */ null,
        /* deviceLocales= */ null,
        /* generateAndroidIntents= */ false);
  }

  public ActionSuggestion[] suggestActionsWithIntents(
      Conversation conversation,
      ActionSuggestionOptions options,
      Object appContext,
      String deviceLocales,
      AnnotatorModel annotator) {
    return nativeSuggestActions(
        actionsModelPtr,
        conversation,
        options,
        (annotator != null ? annotator.getNativeAnnotatorPointer() : 0),
        appContext,
        deviceLocales,
        /* generateAndroidIntents= */ true);
  }

  /** Frees up the allocated memory. */
  @Override
  public void close() {
    if (isClosed.compareAndSet(false, true)) {
      nativeCloseActionsModel(actionsModelPtr);
      actionsModelPtr = 0L;
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

  /** Returns a comma separated list of locales supported by the model as BCP 47 tags. */
  public static String getLocales(int fd) {
    return nativeGetLocales(fd);
  }

  /** Returns the version of the model. */
  public static int getVersion(int fd) {
    return nativeGetVersion(fd);
  }

  /** Returns the name of the model. */
  public static String getName(int fd) {
    return nativeGetName(fd);
  }

  /** Action suggestion that contains a response text and the type of the response. */
  public static final class ActionSuggestion {
    private final String responseText;
    private final String actionType;
    private final float score;
    private final NamedVariant[] entityData;
    private final byte[] serializedEntityData;
    private final RemoteActionTemplate[] remoteActionTemplates;

    public ActionSuggestion(
        String responseText,
        String actionType,
        float score,
        NamedVariant[] entityData,
        byte[] serializedEntityData,
        RemoteActionTemplate[] remoteActionTemplates) {
      this.responseText = responseText;
      this.actionType = actionType;
      this.score = score;
      this.entityData = entityData;
      this.serializedEntityData = serializedEntityData;
      this.remoteActionTemplates = remoteActionTemplates;
    }

    public String getResponseText() {
      return responseText;
    }

    public String getActionType() {
      return actionType;
    }

    /** Confidence score between 0 and 1 */
    public float getScore() {
      return score;
    }

    public NamedVariant[] getEntityData() {
      return entityData;
    }

    public byte[] getSerializedEntityData() {
      return serializedEntityData;
    }

    public RemoteActionTemplate[] getRemoteActionTemplates() {
      return remoteActionTemplates;
    }
  }

  /** Represents a single message in the conversation. */
  public static final class ConversationMessage {
    private final int userId;
    private final String text;
    private final long referenceTimeMsUtc;
    private final String referenceTimezone;
    private final String detectedTextLanguageTags;

    public ConversationMessage(
        int userId,
        String text,
        long referenceTimeMsUtc,
        String referenceTimezone,
        String detectedTextLanguageTags) {
      this.userId = userId;
      this.text = text;
      this.referenceTimeMsUtc = referenceTimeMsUtc;
      this.referenceTimezone = referenceTimezone;
      this.detectedTextLanguageTags = detectedTextLanguageTags;
    }

    /** The identifier of the sender */
    public int getUserId() {
      return userId;
    }

    public String getText() {
      return text;
    }

    /**
     * Return the reference time of the message, for example, it could be compose time or send time.
     * {@code 0} means unspecified.
     */
    public long getReferenceTimeMsUtc() {
      return referenceTimeMsUtc;
    }

    public String getReferenceTimezone() {
      return referenceTimezone;
    }

    /** Returns a comma separated list of BCP 47 language tags. */
    public String getDetectedTextLanguageTags() {
      return detectedTextLanguageTags;
    }
  }

  /** Represents conversation between multiple users. */
  public static final class Conversation {
    public final ConversationMessage[] conversationMessages;

    public Conversation(ConversationMessage[] conversationMessages) {
      this.conversationMessages = conversationMessages;
    }

    public ConversationMessage[] getConversationMessages() {
      return conversationMessages;
    }
  }

  /** Represents options for the SuggestActions call. */
  public static final class ActionSuggestionOptions {
    public ActionSuggestionOptions() {}
  }

  private static native long nativeNewActionsModel(int fd, byte[] serializedPreconditions);

  private static native long nativeNewActionsModelFromPath(
      String path, byte[] preconditionsOverwrite);

  private static native long nativeNewActionsModelWithOffset(
      int fd, long offset, long size, byte[] preconditionsOverwrite);

  private static native String nativeGetLocales(int fd);

  private static native String nativeGetLocalesWithOffset(int fd, long offset, long size);

  private static native int nativeGetVersion(int fd);

  private static native int nativeGetVersionWithOffset(int fd, long offset, long size);

  private static native String nativeGetName(int fd);

  private static native String nativeGetNameWithOffset(int fd, long offset, long size);

  private native ActionSuggestion[] nativeSuggestActions(
      long context,
      Conversation conversation,
      ActionSuggestionOptions options,
      long annotatorPtr,
      Object appContext,
      String deviceLocales,
      boolean generateAndroidIntents);

  private native void nativeCloseActionsModel(long ptr);
}
