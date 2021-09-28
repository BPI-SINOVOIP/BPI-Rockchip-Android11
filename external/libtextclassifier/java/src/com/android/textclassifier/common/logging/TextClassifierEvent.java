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

package com.android.textclassifier.common.logging;

import android.os.Bundle;
import androidx.annotation.IntDef;
import com.google.common.base.Preconditions;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.Locale;
import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/**
 * This class represents events that are sent by components to a TextClassifier to report something
 * of note that relates to a feature powered by the TextClassifier. The TextClassifier may log these
 * events or use them to improve future responses to queries.
 *
 * <p>Each category of events has its their own subclass. Events of each type have an associated set
 * of related properties. You can find their specification in the subclasses.
 */
public abstract class TextClassifierEvent {

  /** Category of the event */
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({
    CATEGORY_SELECTION,
    CATEGORY_LINKIFY,
    CATEGORY_CONVERSATION_ACTIONS,
    CATEGORY_LANGUAGE_DETECTION
  })
  public @interface Category {
    // For custom event categories, use range 1000+.
  }

  /**
   * Smart selection
   *
   * @see TextSelectionEvent
   */
  public static final int CATEGORY_SELECTION = 1;
  /**
   * Linkify
   *
   * @see TextLinkifyEvent
   */
  public static final int CATEGORY_LINKIFY = 2;
  /**
   * Conversation actions
   *
   * @see ConversationActionsEvent
   */
  public static final int CATEGORY_CONVERSATION_ACTIONS = 3;
  /**
   * Language detection
   *
   * @see LanguageDetectionEvent
   */
  public static final int CATEGORY_LANGUAGE_DETECTION = 4;

  /** Type of the event */
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({
    TYPE_SELECTION_STARTED,
    TYPE_SELECTION_MODIFIED,
    TYPE_SMART_SELECTION_SINGLE,
    TYPE_SMART_SELECTION_MULTI,
    TYPE_AUTO_SELECTION,
    TYPE_ACTIONS_SHOWN,
    TYPE_LINK_CLICKED,
    TYPE_OVERTYPE,
    TYPE_COPY_ACTION,
    TYPE_PASTE_ACTION,
    TYPE_CUT_ACTION,
    TYPE_SHARE_ACTION,
    TYPE_SMART_ACTION,
    TYPE_SELECTION_DRAG,
    TYPE_SELECTION_DESTROYED,
    TYPE_OTHER_ACTION,
    TYPE_SELECT_ALL,
    TYPE_SELECTION_RESET,
    TYPE_MANUAL_REPLY,
    TYPE_ACTIONS_GENERATED,
    TYPE_LINKS_GENERATED
  })
  public @interface Type {
    // For custom event types, use range 1,000,000+.
  }

  // All these event type constants are required to match with those defined in
  // textclassifier_enums.proto.
  /** User started a new selection. */
  public static final int TYPE_SELECTION_STARTED = 1;
  /** User modified an existing selection. */
  public static final int TYPE_SELECTION_MODIFIED = 2;
  /** Smart selection triggered for a single token (word). */
  public static final int TYPE_SMART_SELECTION_SINGLE = 3;
  /** Smart selection triggered spanning multiple tokens (words). */
  public static final int TYPE_SMART_SELECTION_MULTI = 4;
  /** Something else other than user or the default TextClassifier triggered a selection. */
  public static final int TYPE_AUTO_SELECTION = 5;
  /** Smart actions shown to the user. */
  public static final int TYPE_ACTIONS_SHOWN = 6;
  /** User clicked a link. */
  public static final int TYPE_LINK_CLICKED = 7;
  /** User typed over the selection. */
  public static final int TYPE_OVERTYPE = 8;
  /** User clicked on Copy action. */
  public static final int TYPE_COPY_ACTION = 9;
  /** User clicked on Paste action. */
  public static final int TYPE_PASTE_ACTION = 10;
  /** User clicked on Cut action. */
  public static final int TYPE_CUT_ACTION = 11;
  /** User clicked on Share action. */
  public static final int TYPE_SHARE_ACTION = 12;
  /** User clicked on a Smart action. */
  public static final int TYPE_SMART_ACTION = 13;
  /** User dragged+dropped the selection. */
  public static final int TYPE_SELECTION_DRAG = 14;
  /** Selection is destroyed. */
  public static final int TYPE_SELECTION_DESTROYED = 15;
  /** User clicked on a custom action. */
  public static final int TYPE_OTHER_ACTION = 16;
  /** User clicked on Select All action */
  public static final int TYPE_SELECT_ALL = 17;
  /** User reset the smart selection. */
  public static final int TYPE_SELECTION_RESET = 18;
  /** User composed a reply. */
  public static final int TYPE_MANUAL_REPLY = 19;
  /** TextClassifier generated some actions */
  public static final int TYPE_ACTIONS_GENERATED = 20;
  /** Some text links were generated. */
  public static final int TYPE_LINKS_GENERATED = 21;

  @Category private final int eventCategory;
  @Type private final int eventType;
  @Nullable private final String[] entityTypes;
  @Nullable private TextClassificationContext eventContext;
  @Nullable private final String resultId;
  private final int eventIndex;
  private final float[] scores;
  @Nullable private final String modelName;
  private final int[] actionIndices;
  @Nullable private final Locale locale;
  private final Bundle extras;

  private TextClassifierEvent(Builder<?> builder) {
    eventCategory = builder.eventCategory;
    eventType = builder.eventType;
    entityTypes = builder.entityTypes;
    eventContext = builder.eventContext;
    resultId = builder.resultId;
    eventIndex = builder.eventIndex;
    scores = builder.scores;
    modelName = builder.modelName;
    actionIndices = builder.actionIndices;
    locale = builder.locale;
    extras = builder.extras == null ? Bundle.EMPTY : builder.extras;
  }

  /** Returns the event category. e.g. {@link #CATEGORY_SELECTION}. */
  @Category
  public int getEventCategory() {
    return eventCategory;
  }

  /** Returns the event type. e.g. {@link #TYPE_SELECTION_STARTED}. */
  @Type
  public int getEventType() {
    return eventType;
  }

  /**
   * Returns an array of entity types. e.g. {@link TextClassifier#TYPE_ADDRESS}.
   *
   * @see Builder#setEntityTypes(String...) for supported types.
   */
  @Nonnull
  public String[] getEntityTypes() {
    return entityTypes;
  }

  /** Returns the event context. */
  @Nullable
  public TextClassificationContext getEventContext() {
    return eventContext;
  }

  /**
   * Sets the event context.
   *
   * <p>Package-private for SystemTextClassifier's use.
   */
  void setEventContext(@Nullable TextClassificationContext eventContext) {
    this.eventContext = eventContext;
  }

  /** Returns the id of the text classifier result related to this event. */
  @Nullable
  public String getResultId() {
    return resultId;
  }

  /** Returns the index of this event in the series of event it belongs to. */
  public int getEventIndex() {
    return eventIndex;
  }

  /** Returns the scores of the suggestions. */
  public float[] getScores() {
    return scores;
  }

  /** Returns the model name. */
  @Nullable
  public String getModelName() {
    return modelName;
  }

  /**
   * Returns the indices of the actions relating to this event. Actions are usually returned by the
   * text classifier in priority order with the most preferred action at index 0. This list gives an
   * indication of the position of the actions that are being reported.
   *
   * @see Builder#setActionIndices(int...)
   */
  public int[] getActionIndices() {
    return actionIndices;
  }

  /** Returns the detected locale. */
  @Nullable
  public Locale getLocale() {
    return locale;
  }

  /**
   * Returns a bundle containing non-structured extra information about this event.
   *
   * <p><b>NOTE: </b>Do not modify this bundle.
   */
  @Nonnull
  public Bundle getExtras() {
    return extras;
  }

  @Override
  public String toString() {
    StringBuilder out = new StringBuilder(128);
    out.append(this.getClass().getSimpleName());
    out.append("{");
    out.append("mEventCategory=").append(eventCategory);
    out.append(", mEventType=").append(eventType);
    out.append(", mEntityTypes=").append(Arrays.toString(entityTypes));
    out.append(", mEventContext=").append(eventContext);
    out.append(", mResultId=").append(resultId);
    out.append(", mEventIndex=").append(eventIndex);
    out.append(", mExtras=").append(extras);
    out.append(", mScores=").append(Arrays.toString(scores));
    out.append(", mModelName=").append(modelName);
    out.append(", mActionIndices=").append(Arrays.toString(actionIndices));
    toString(out);
    out.append("}");
    return out.toString();
  }

  /**
   * Overrides this to append extra fields to the output of {@link #toString()}.
   *
   * <p>Extra fields should be formatted like this: ", {field_name}={field_value}".
   */
  void toString(StringBuilder out) {}

  /**
   * Builder to build a text classifier event.
   *
   * @param <T> The subclass to be built.
   */
  public abstract static class Builder<T extends Builder<T>> {

    private final int eventCategory;
    private final int eventType;
    private String[] entityTypes = new String[0];
    @Nullable private TextClassificationContext eventContext;
    @Nullable private String resultId;
    private int eventIndex;
    private float[] scores = new float[0];
    @Nullable private String modelName;
    private int[] actionIndices = new int[0];
    @Nullable private Locale locale;
    @Nullable private Bundle extras;

    /**
     * Creates a builder for building {@link TextClassifierEvent}s.
     *
     * @param eventCategory The event category. e.g. {@link #CATEGORY_SELECTION}
     * @param eventType The event type. e.g. {@link #TYPE_SELECTION_STARTED}
     */
    private Builder(@Category int eventCategory, @Type int eventType) {
      this.eventCategory = eventCategory;
      this.eventType = eventType;
    }

    /**
     * Sets the entity types. e.g. {@link android.view.textclassifier.TextClassifier#TYPE_ADDRESS}.
     *
     * <p>Supported types:
     *
     * <p>See {@link android.view.textclassifier.TextClassifier.EntityType}
     *
     * <p>See {@link android.view.textclassifier.ConversationAction.ActionType}
     *
     * <p>See {@link Locale#toLanguageTag()}
     */
    public T setEntityTypes(String... entityTypes) {
      Preconditions.checkNotNull(entityTypes);
      this.entityTypes = new String[entityTypes.length];
      System.arraycopy(entityTypes, 0, this.entityTypes, 0, entityTypes.length);
      return self();
    }

    /** Sets the event context. */
    public T setEventContext(@Nullable TextClassificationContext eventContext) {
      this.eventContext = eventContext;
      return self();
    }

    /** Sets the id of the text classifier result related to this event. */
    @Nonnull
    public T setResultId(@Nullable String resultId) {
      this.resultId = resultId;
      return self();
    }

    /** Sets the index of this event in the series of events it belongs to. */
    @Nonnull
    public T setEventIndex(int eventIndex) {
      this.eventIndex = eventIndex;
      return self();
    }

    /** Sets the scores of the suggestions. */
    @Nonnull
    public T setScores(@Nonnull float... scores) {
      Preconditions.checkNotNull(scores);
      this.scores = new float[scores.length];
      System.arraycopy(scores, 0, this.scores, 0, scores.length);
      return self();
    }

    /** Sets the model name string. */
    @Nonnull
    public T setModelName(@Nullable String modelVersion) {
      modelName = modelVersion;
      return self();
    }

    /**
     * Sets the indices of the actions involved in this event. Actions are usually returned by the
     * text classifier in priority order with the most preferred action at index 0. These indices
     * give an indication of the position of the actions that are being reported.
     *
     * <p>E.g.
     *
     * <pre>
     *   // 3 smart actions are shown at index 0, 1, 2 respectively in response to a link click.
     *   new TextClassifierEvent.Builder(CATEGORY_LINKIFY, TYPE_ACTIONS_SHOWN)
     *       .setEventIndex(0, 1, 2)
     *       ...
     *       .build();
     *
     *   ...
     *
     *   // Smart action at index 1 is activated.
     *   new TextClassifierEvent.Builder(CATEGORY_LINKIFY, TYPE_SMART_ACTION)
     *       .setEventIndex(1)
     *       ...
     *       .build();
     * </pre>
     *
     * @see android.view.textclassifier.TextClassification#getActions()
     */
    @Nonnull
    public T setActionIndices(@Nonnull int... actionIndices) {
      this.actionIndices = new int[actionIndices.length];
      System.arraycopy(actionIndices, 0, this.actionIndices, 0, actionIndices.length);
      return self();
    }

    /** Sets the detected locale. */
    @Nonnull
    public T setLocale(@Nullable Locale locale) {
      this.locale = locale;
      return self();
    }

    /**
     * Sets a bundle containing non-structured extra information about the event.
     *
     * <p><b>NOTE: </b>Prefer to set only immutable values on the bundle otherwise, avoid updating
     * the internals of this bundle as it may have unexpected consequences on the clients of the
     * built event object. For similar reasons, avoid depending on mutable objects in this bundle.
     */
    @Nonnull
    public T setExtras(@Nonnull Bundle extras) {
      this.extras = Preconditions.checkNotNull(extras);
      return self();
    }

    abstract T self();
  }

  /**
   * This class represents events that are related to the smart text selection feature.
   *
   * <p>
   *
   * <pre>
   *     // User started a selection. e.g. "York" in text "New York City, NY".
   *     new TextSelectionEvent.Builder(TYPE_SELECTION_STARTED)
   *         .setEventContext(classificationContext)
   *         .setEventIndex(0)
   *         .build();
   *
   *     // System smart-selects a recognized entity. e.g. "New York City".
   *     new TextSelectionEvent.Builder(TYPE_SMART_SELECTION_MULTI)
   *         .setEventContext(classificationContext)
   *         .setResultId(textSelection.getId())
   *         .setRelativeWordStartIndex(-1) // Goes back one word to "New" from "York".
   *         .setRelativeWordEndIndex(2)    // Goes forward 2 words from "York" to start of ",".
   *         .setEntityTypes(textClassification.getEntity(0))
   *         .setScore(textClassification.getConfidenceScore(entityType))
   *         .setEventIndex(1)
   *         .build();
   *
   *     // User resets the selection to the original selection. i.e. "York".
   *     new TextSelectionEvent.Builder(TYPE_SELECTION_RESET)
   *         .setEventContext(classificationContext)
   *         .setResultId(textSelection.getId())
   *         .setRelativeSuggestedWordStartIndex(-1) // Repeated from above.
   *         .setRelativeSuggestedWordEndIndex(2)    // Repeated from above.
   *         .setRelativeWordStartIndex(0)           // Original selection is always at (0, 1].
   *         .setRelativeWordEndIndex(1)
   *         .setEntityTypes(textClassification.getEntity(0))
   *         .setScore(textClassification.getConfidenceScore(entityType))
   *         .setEventIndex(2)
   *         .build();
   *
   *     // User modified the selection. e.g. "New".
   *     new TextSelectionEvent.Builder(TYPE_SELECTION_MODIFIED)
   *         .setEventContext(classificationContext)
   *         .setResultId(textSelection.getId())
   *         .setRelativeSuggestedWordStartIndex(-1) // Repeated from above.
   *         .setRelativeSuggestedWordEndIndex(2)    // Repeated from above.
   *         .setRelativeWordStartIndex(-1)          // Goes backward one word from "York" to
   *         "New".
   *         .setRelativeWordEndIndex(0)             // Goes backward one word to exclude "York".
   *         .setEntityTypes(textClassification.getEntity(0))
   *         .setScore(textClassification.getConfidenceScore(entityType))
   *         .setEventIndex(3)
   *         .build();
   *
   *     // Smart (contextual) actions (at indices, 0, 1, 2) presented to the user.
   *     // e.g. "Map", "Ride share", "Explore".
   *     new TextSelectionEvent.Builder(TYPE_ACTIONS_SHOWN)
   *         .setEventContext(classificationContext)
   *         .setResultId(textClassification.getId())
   *         .setEntityTypes(textClassification.getEntity(0))
   *         .setScore(textClassification.getConfidenceScore(entityType))
   *         .setActionIndices(0, 1, 2)
   *         .setEventIndex(4)
   *         .build();
   *
   *     // User chooses the "Copy" action.
   *     new TextSelectionEvent.Builder(TYPE_COPY_ACTION)
   *         .setEventContext(classificationContext)
   *         .setResultId(textClassification.getId())
   *         .setEntityTypes(textClassification.getEntity(0))
   *         .setScore(textClassification.getConfidenceScore(entityType))
   *         .setEventIndex(5)
   *         .build();
   *
   *     // User chooses smart action at index 1. i.e. "Ride share".
   *     new TextSelectionEvent.Builder(TYPE_SMART_ACTION)
   *         .setEventContext(classificationContext)
   *         .setResultId(textClassification.getId())
   *         .setEntityTypes(textClassification.getEntity(0))
   *         .setScore(textClassification.getConfidenceScore(entityType))
   *         .setActionIndices(1)
   *         .setEventIndex(5)
   *         .build();
   *
   *     // Selection dismissed.
   *     new TextSelectionEvent.Builder(TYPE_SELECTION_DESTROYED)
   *         .setEventContext(classificationContext)
   *         .setResultId(textClassification.getId())
   *         .setEntityTypes(textClassification.getEntity(0))
   *         .setScore(textClassification.getConfidenceScore(entityType))
   *         .setEventIndex(6)
   *         .build();
   * </pre>
   *
   * <p>
   */
  public static final class TextSelectionEvent extends TextClassifierEvent {

    final int relativeWordStartIndex;
    final int relativeWordEndIndex;
    final int relativeSuggestedWordStartIndex;
    final int relativeSuggestedWordEndIndex;

    private TextSelectionEvent(TextSelectionEvent.Builder builder) {
      super(builder);
      relativeWordStartIndex = builder.relativeWordStartIndex;
      relativeWordEndIndex = builder.relativeWordEndIndex;
      relativeSuggestedWordStartIndex = builder.relativeSuggestedWordStartIndex;
      relativeSuggestedWordEndIndex = builder.relativeSuggestedWordEndIndex;
    }

    /** Returns the relative word index of the start of the selection. */
    public int getRelativeWordStartIndex() {
      return relativeWordStartIndex;
    }

    /** Returns the relative word (exclusive) index of the end of the selection. */
    public int getRelativeWordEndIndex() {
      return relativeWordEndIndex;
    }

    /** Returns the relative word index of the start of the smart selection. */
    public int getRelativeSuggestedWordStartIndex() {
      return relativeSuggestedWordStartIndex;
    }

    /** Returns the relative word (exclusive) index of the end of the smart selection. */
    public int getRelativeSuggestedWordEndIndex() {
      return relativeSuggestedWordEndIndex;
    }

    @Override
    void toString(StringBuilder out) {
      out.append(", getRelativeWordStartIndex=").append(relativeWordStartIndex);
      out.append(", getRelativeWordEndIndex=").append(relativeWordEndIndex);
      out.append(", getRelativeSuggestedWordStartIndex=").append(relativeSuggestedWordStartIndex);
      out.append(", getRelativeSuggestedWordEndIndex=").append(relativeSuggestedWordEndIndex);
    }

    /** Builder class for {@link TextSelectionEvent}. */
    public static final class Builder
        extends TextClassifierEvent.Builder<TextSelectionEvent.Builder> {
      int relativeWordStartIndex;
      int relativeWordEndIndex;
      int relativeSuggestedWordStartIndex;
      int relativeSuggestedWordEndIndex;

      /**
       * Creates a builder for building {@link TextSelectionEvent}s.
       *
       * @param eventType The event type. e.g. {@link #TYPE_SELECTION_STARTED}
       */
      public Builder(@Type int eventType) {
        super(CATEGORY_SELECTION, eventType);
      }

      /** Sets the relative word index of the start of the selection. */
      @Nonnull
      public Builder setRelativeWordStartIndex(int relativeWordStartIndex) {
        this.relativeWordStartIndex = relativeWordStartIndex;
        return this;
      }

      /** Sets the relative word (exclusive) index of the end of the selection. */
      @Nonnull
      public Builder setRelativeWordEndIndex(int relativeWordEndIndex) {
        this.relativeWordEndIndex = relativeWordEndIndex;
        return this;
      }

      /** Sets the relative word index of the start of the smart selection. */
      @Nonnull
      public Builder setRelativeSuggestedWordStartIndex(int relativeSuggestedWordStartIndex) {
        this.relativeSuggestedWordStartIndex = relativeSuggestedWordStartIndex;
        return this;
      }

      /** Sets the relative word (exclusive) index of the end of the smart selection. */
      @Nonnull
      public Builder setRelativeSuggestedWordEndIndex(int relativeSuggestedWordEndIndex) {
        this.relativeSuggestedWordEndIndex = relativeSuggestedWordEndIndex;
        return this;
      }

      @Override
      TextSelectionEvent.Builder self() {
        return this;
      }

      /** Builds and returns a {@link TextSelectionEvent}. */
      @Nonnull
      public TextSelectionEvent build() {
        return new TextSelectionEvent(this);
      }
    }
  }

  /**
   * This class represents events that are related to the smart linkify feature.
   *
   * <p>
   *
   * <pre>
   *     // User clicked on a link.
   *     new TextLinkifyEvent.Builder(TYPE_LINK_CLICKED)
   *         .setEventContext(classificationContext)
   *         .setResultId(textClassification.getId())
   *         .setEntityTypes(textClassification.getEntity(0))
   *         .setScore(textClassification.getConfidenceScore(entityType))
   *         .setEventIndex(0)
   *         .build();
   *
   *     // Smart (contextual) actions presented to the user in response to a link click.
   *     new TextLinkifyEvent.Builder(TYPE_ACTIONS_SHOWN)
   *         .setEventContext(classificationContext)
   *         .setResultId(textClassification.getId())
   *         .setEntityTypes(textClassification.getEntity(0))
   *         .setScore(textClassification.getConfidenceScore(entityType))
   *         .setActionIndices(range(textClassification.getActions().size()))
   *         .setEventIndex(1)
   *         .build();
   *
   *     // User chooses smart action at index 0.
   *     new TextLinkifyEvent.Builder(TYPE_SMART_ACTION)
   *         .setEventContext(classificationContext)
   *         .setResultId(textClassification.getId())
   *         .setEntityTypes(textClassification.getEntity(0))
   *         .setScore(textClassification.getConfidenceScore(entityType))
   *         .setActionIndices(0)
   *         .setEventIndex(2)
   *         .build();
   * </pre>
   */
  public static final class TextLinkifyEvent extends TextClassifierEvent {

    private TextLinkifyEvent(TextLinkifyEvent.Builder builder) {
      super(builder);
    }

    /** Builder class for {@link TextLinkifyEvent}. */
    public static final class Builder
        extends TextClassifierEvent.Builder<TextLinkifyEvent.Builder> {
      /**
       * Creates a builder for building {@link TextLinkifyEvent}s.
       *
       * @param eventType The event type. e.g. {@link #TYPE_SMART_ACTION}
       */
      public Builder(@Type int eventType) {
        super(TextClassifierEvent.CATEGORY_LINKIFY, eventType);
      }

      @Override
      Builder self() {
        return this;
      }

      /** Builds and returns a {@link TextLinkifyEvent}. */
      @Nonnull
      public TextLinkifyEvent build() {
        return new TextLinkifyEvent(this);
      }
    }
  }

  /**
   * This class represents events that are related to the language detection feature.
   * <p>
   * <pre>
   *     // Translate action shown for foreign text.
   *     new LanguageDetectionEvent.Builder(TYPE_ACTIONS_SHOWN)
   *         .setEventContext(classificationContext)
   *         .setResultId(textClassification.getId())
   *         .setEntityTypes(language)
   *         .setScore(score)
   *         .setActionIndices(textClassification.getActions().indexOf(translateAction))
   *         .setEventIndex(0)
   *         .build();
   *
   *     // Translate action selected.
   *     new LanguageDetectionEvent.Builder(TYPE_SMART_ACTION)
   *         .setEventContext(classificationContext)
   *         .setResultId(textClassification.getId())
   *         .setEntityTypes(language)
   *         .setScore(score)
   *         .setActionIndices(textClassification.getActions().indexOf(translateAction))
   *         .setEventIndex(1)
   *         .build();
   */
  public static final class LanguageDetectionEvent extends TextClassifierEvent {

    private LanguageDetectionEvent(LanguageDetectionEvent.Builder builder) {
      super(builder);
    }

    /** Builder class for {@link LanguageDetectionEvent}. */
    public static final class Builder
        extends TextClassifierEvent.Builder<LanguageDetectionEvent.Builder> {

      /**
       * Creates a builder for building {@link TextSelectionEvent}s.
       *
       * @param eventType The event type. e.g. {@link #TYPE_SMART_ACTION}
       */
      public Builder(@Type int eventType) {
        super(TextClassifierEvent.CATEGORY_LANGUAGE_DETECTION, eventType);
      }

      @Override
      Builder self() {
        return this;
      }

      /** Builds and returns a {@link LanguageDetectionEvent}. */
      @Nonnull
      public LanguageDetectionEvent build() {
        return new LanguageDetectionEvent(this);
      }
    }
  }

  /**
   * This class represents events that are related to the conversation actions feature.
   *
   * <p>
   *
   * <pre>
   *     // Conversation (contextual) actions/replies generated.
   *     new ConversationActionsEvent.Builder(TYPE_ACTIONS_GENERATED)
   *         .setEventContext(classificationContext)
   *         .setResultId(conversationActions.getId())
   *         .setEntityTypes(getTypes(conversationActions))
   *         .setActionIndices(range(conversationActions.getActions().size()))
   *         .setEventIndex(0)
   *         .build();
   *
   *     // Conversation actions/replies presented to user.
   *     new ConversationActionsEvent.Builder(TYPE_ACTIONS_SHOWN)
   *         .setEventContext(classificationContext)
   *         .setResultId(conversationActions.getId())
   *         .setEntityTypes(getTypes(conversationActions))
   *         .setActionIndices(range(conversationActions.getActions().size()))
   *         .setEventIndex(1)
   *         .build();
   *
   *     // User clicked the "Reply" button to compose their custom reply.
   *     new ConversationActionsEvent.Builder(TYPE_MANUAL_REPLY)
   *         .setEventContext(classificationContext)
   *         .setResultId(conversationActions.getId())
   *         .setEventIndex(2)
   *         .build();
   *
   *     // User selected a smart (contextual) action/reply.
   *     new ConversationActionsEvent.Builder(TYPE_SMART_ACTION)
   *         .setEventContext(classificationContext)
   *         .setResultId(conversationActions.getId())
   *         .setEntityTypes(conversationActions.get(1).getType())
   *         .setScore(conversationAction.get(1).getConfidenceScore())
   *         .setActionIndices(1)
   *         .setEventIndex(2)
   *         .build();
   * </pre>
   */
  public static final class ConversationActionsEvent extends TextClassifierEvent {

    private ConversationActionsEvent(ConversationActionsEvent.Builder builder) {
      super(builder);
    }

    /** Builder class for {@link ConversationActionsEvent}. */
    public static final class Builder
        extends TextClassifierEvent.Builder<ConversationActionsEvent.Builder> {
      /**
       * Creates a builder for building {@link TextSelectionEvent}s.
       *
       * @param eventType The event type. e.g. {@link #TYPE_SMART_ACTION}
       */
      public Builder(@Type int eventType) {
        super(TextClassifierEvent.CATEGORY_CONVERSATION_ACTIONS, eventType);
      }

      @Override
      Builder self() {
        return this;
      }

      /** Builds and returns a {@link ConversationActionsEvent}. */
      @Nonnull
      public ConversationActionsEvent build() {
        return new ConversationActionsEvent(this);
      }
    }
  }
}
