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

package com.android.textclassifier;

import static java.util.stream.Collectors.toCollection;

import android.app.PendingIntent;
import android.app.RemoteAction;
import android.content.Context;
import android.content.Intent;
import android.icu.util.ULocale;
import android.os.Bundle;
import android.os.LocaleList;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.util.ArrayMap;
import android.view.View.OnClickListener;
import android.view.textclassifier.ConversationAction;
import android.view.textclassifier.ConversationActions;
import android.view.textclassifier.SelectionEvent;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassificationSessionId;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextClassifierEvent;
import android.view.textclassifier.TextLanguage;
import android.view.textclassifier.TextLinks;
import android.view.textclassifier.TextSelection;
import androidx.annotation.GuardedBy;
import androidx.annotation.WorkerThread;
import androidx.core.util.Pair;
import com.android.textclassifier.ModelFileManager.ModelFile;
import com.android.textclassifier.common.base.TcLog;
import com.android.textclassifier.common.intent.LabeledIntent;
import com.android.textclassifier.common.intent.TemplateIntentFactory;
import com.android.textclassifier.common.logging.ResultIdUtils;
import com.android.textclassifier.common.logging.ResultIdUtils.ModelInfo;
import com.android.textclassifier.common.statsd.GenerateLinksLogger;
import com.android.textclassifier.common.statsd.SelectionEventConverter;
import com.android.textclassifier.common.statsd.TextClassificationSessionIdConverter;
import com.android.textclassifier.common.statsd.TextClassifierEventConverter;
import com.android.textclassifier.common.statsd.TextClassifierEventLogger;
import com.android.textclassifier.utils.IndentingPrintWriter;
import com.google.android.textclassifier.ActionsSuggestionsModel;
import com.google.android.textclassifier.AnnotatorModel;
import com.google.android.textclassifier.LangIdModel;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.collect.FluentIterable;
import com.google.common.collect.ImmutableList;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.time.ZoneId;
import java.time.ZonedDateTime;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import javax.annotation.Nullable;

/**
 * A text classifier that is running locally.
 *
 * <p>This class uses machine learning to recognize entities in text. Unless otherwise stated,
 * methods of this class are blocking operations and should most likely not be called on the UI
 * thread.
 */
final class TextClassifierImpl {

  private static final String TAG = "TextClassifierImpl";

  private static final File FACTORY_MODEL_DIR = new File("/etc/textclassifier/");
  // Annotator
  private static final String ANNOTATOR_FACTORY_MODEL_FILENAME_REGEX =
      "textclassifier\\.(.*)\\.model";
  private static final File ANNOTATOR_UPDATED_MODEL_FILE =
      new File("/data/misc/textclassifier/textclassifier.model");

  // LangIdModel
  private static final String LANG_ID_FACTORY_MODEL_FILENAME_REGEX = "lang_id.model";
  private static final File UPDATED_LANG_ID_MODEL_FILE =
      new File("/data/misc/textclassifier/lang_id.model");

  // Actions
  private static final String ACTIONS_FACTORY_MODEL_FILENAME_REGEX =
      "actions_suggestions\\.(.*)\\.model";
  private static final File UPDATED_ACTIONS_MODEL =
      new File("/data/misc/textclassifier/actions_suggestions.model");

  private final Context context;
  private final TextClassifier fallback;
  private final GenerateLinksLogger generateLinksLogger;

  private final Object lock = new Object();

  @GuardedBy("lock")
  private ModelFileManager.ModelFile annotatorModelInUse;

  @GuardedBy("lock")
  private AnnotatorModel annotatorImpl;

  @GuardedBy("lock")
  private ModelFileManager.ModelFile langIdModelInUse;

  @GuardedBy("lock")
  private LangIdModel langIdImpl;

  @GuardedBy("lock")
  private ModelFileManager.ModelFile actionModelInUse;

  @GuardedBy("lock")
  private ActionsSuggestionsModel actionsImpl;

  private final TextClassifierEventLogger textClassifierEventLogger =
      new TextClassifierEventLogger();

  private final TextClassifierSettings settings;

  private final ModelFileManager annotatorModelFileManager;
  private final ModelFileManager langIdModelFileManager;
  private final ModelFileManager actionsModelFileManager;
  private final TemplateIntentFactory templateIntentFactory;

  TextClassifierImpl(Context context, TextClassifierSettings settings, TextClassifier fallback) {
    this.context = Preconditions.checkNotNull(context);
    this.fallback = Preconditions.checkNotNull(fallback);
    this.settings = Preconditions.checkNotNull(settings);
    generateLinksLogger = new GenerateLinksLogger(this.settings.getGenerateLinksLogSampleRate());
    annotatorModelFileManager =
        new ModelFileManager(
            new ModelFileManager.ModelFileSupplierImpl(
                FACTORY_MODEL_DIR,
                ANNOTATOR_FACTORY_MODEL_FILENAME_REGEX,
                ANNOTATOR_UPDATED_MODEL_FILE,
                AnnotatorModel::getVersion,
                AnnotatorModel::getLocales));
    langIdModelFileManager =
        new ModelFileManager(
            new ModelFileManager.ModelFileSupplierImpl(
                FACTORY_MODEL_DIR,
                LANG_ID_FACTORY_MODEL_FILENAME_REGEX,
                UPDATED_LANG_ID_MODEL_FILE,
                LangIdModel::getVersion,
                fd -> ModelFileManager.ModelFile.LANGUAGE_INDEPENDENT));
    actionsModelFileManager =
        new ModelFileManager(
            new ModelFileManager.ModelFileSupplierImpl(
                FACTORY_MODEL_DIR,
                ACTIONS_FACTORY_MODEL_FILENAME_REGEX,
                UPDATED_ACTIONS_MODEL,
                ActionsSuggestionsModel::getVersion,
                ActionsSuggestionsModel::getLocales));

    templateIntentFactory = new TemplateIntentFactory();
  }

  TextClassifierImpl(Context context, TextClassifierSettings settings) {
    this(context, settings, TextClassifier.NO_OP);
  }

  @WorkerThread
  TextSelection suggestSelection(TextSelection.Request request) {
    Preconditions.checkNotNull(request);
    checkMainThread();
    try {
      final int rangeLength = request.getEndIndex() - request.getStartIndex();
      final String string = request.getText().toString();
      if (string.length() > 0 && rangeLength <= settings.getSuggestSelectionMaxRangeLength()) {
        final String localesString = concatenateLocales(request.getDefaultLocales());
        final Optional<LangIdModel> langIdModel = getLangIdImpl();
        final String detectLanguageTags =
            String.join(",", detectLanguageTags(langIdModel, request.getText()));
        final ZonedDateTime refTime = ZonedDateTime.now(ZoneId.systemDefault());
        final AnnotatorModel annotatorImpl = getAnnotatorImpl(request.getDefaultLocales());
        final int[] startEnd =
            annotatorImpl.suggestSelection(
                string,
                request.getStartIndex(),
                request.getEndIndex(),
                new AnnotatorModel.SelectionOptions(localesString, detectLanguageTags));
        final int start = startEnd[0];
        final int end = startEnd[1];
        if (start < end
            && start >= 0
            && end <= string.length()
            && start <= request.getStartIndex()
            && end >= request.getEndIndex()) {
          final TextSelection.Builder tsBuilder = new TextSelection.Builder(start, end);
          final AnnotatorModel.ClassificationResult[] results =
              annotatorImpl.classifyText(
                  string,
                  start,
                  end,
                  new AnnotatorModel.ClassificationOptions(
                      refTime.toInstant().toEpochMilli(),
                      refTime.getZone().getId(),
                      localesString,
                      detectLanguageTags),
                  // Passing null here to suppress intent generation
                  // TODO: Use an explicit flag to suppress it.
                  /* appContext */ null,
                  /* deviceLocales */ null);
          final int size = results.length;
          for (int i = 0; i < size; i++) {
            tsBuilder.setEntityType(results[i].getCollection(), results[i].getScore());
          }
          final String resultId =
              createAnnotatorId(string, request.getStartIndex(), request.getEndIndex());
          return tsBuilder.setId(resultId).build();
        } else {
          // We can not trust the result. Log the issue and ignore the result.
          TcLog.d(TAG, "Got bad indices for input text. Ignoring result.");
        }
      }
    } catch (Throwable t) {
      // Avoid throwing from this method. Log the error.
      TcLog.e(TAG, "Error suggesting selection for text. No changes to selection suggested.", t);
    }
    // Getting here means something went wrong, return a NO_OP result.
    return fallback.suggestSelection(request);
  }

  @WorkerThread
  TextClassification classifyText(TextClassification.Request request) {
    Preconditions.checkNotNull(request);
    checkMainThread();
    try {
      Optional<LangIdModel> langId = getLangIdImpl();
      List<String> detectLanguageTags = detectLanguageTags(langId, request.getText());
      final int rangeLength = request.getEndIndex() - request.getStartIndex();
      final String string = request.getText().toString();
      if (string.length() > 0 && rangeLength <= settings.getClassifyTextMaxRangeLength()) {
        final String localesString = concatenateLocales(request.getDefaultLocales());
        final ZonedDateTime refTime =
            request.getReferenceTime() != null
                ? request.getReferenceTime()
                : ZonedDateTime.now(ZoneId.systemDefault());
        final AnnotatorModel.ClassificationResult[] results =
            getAnnotatorImpl(request.getDefaultLocales())
                .classifyText(
                    string,
                    request.getStartIndex(),
                    request.getEndIndex(),
                    new AnnotatorModel.ClassificationOptions(
                        refTime.toInstant().toEpochMilli(),
                        refTime.getZone().getId(),
                        localesString,
                        String.join(",", detectLanguageTags),
                        AnnotatorModel.AnnotationUsecase.SMART.getValue(),
                        LocaleList.getDefault().toLanguageTags()),
                    context,
                    getResourceLocalesString());
        if (results.length > 0) {
          return createClassificationResult(
              results, string, request.getStartIndex(), request.getEndIndex(), langId);
        }
      }
    } catch (Throwable t) {
      // Avoid throwing from this method. Log the error.
      TcLog.e(TAG, "Error getting text classification info.", t);
    }
    // Getting here means something went wrong, return a NO_OP result.
    return fallback.classifyText(request);
  }

  @WorkerThread
  TextLinks generateLinks(TextLinks.Request request) {
    Preconditions.checkNotNull(request);
    Preconditions.checkArgument(
        request.getText().length() <= getMaxGenerateLinksTextLength(),
        "text.length() cannot be greater than %s",
        getMaxGenerateLinksTextLength());
    checkMainThread();

    final String textString = request.getText().toString();
    final TextLinks.Builder builder = new TextLinks.Builder(textString);

    try {
      final long startTimeMs = System.currentTimeMillis();
      final ZonedDateTime refTime = ZonedDateTime.now(ZoneId.systemDefault());
      final Collection<String> entitiesToIdentify =
          request.getEntityConfig() != null
              ? request
                  .getEntityConfig()
                  .resolveEntityListModifications(
                      getEntitiesForHints(request.getEntityConfig().getHints()))
              : settings.getEntityListDefault();
      final String localesString = concatenateLocales(request.getDefaultLocales());
      Optional<LangIdModel> langId = getLangIdImpl();
      ImmutableList<String> detectLanguageTags = detectLanguageTags(langId, request.getText());
      final AnnotatorModel annotatorImpl = getAnnotatorImpl(request.getDefaultLocales());
      final boolean isSerializedEntityDataEnabled =
          ExtrasUtils.isSerializedEntityDataEnabled(request);
      final AnnotatorModel.AnnotatedSpan[] annotations =
          annotatorImpl.annotate(
              textString,
              new AnnotatorModel.AnnotationOptions(
                  refTime.toInstant().toEpochMilli(),
                  refTime.getZone().getId(),
                  localesString,
                  String.join(",", detectLanguageTags),
                  entitiesToIdentify,
                  AnnotatorModel.AnnotationUsecase.SMART.getValue(),
                  isSerializedEntityDataEnabled));
      for (AnnotatorModel.AnnotatedSpan span : annotations) {
        final AnnotatorModel.ClassificationResult[] results = span.getClassification();
        if (results.length == 0 || !entitiesToIdentify.contains(results[0].getCollection())) {
          continue;
        }
        final Map<String, Float> entityScores = new ArrayMap<>();
        for (int i = 0; i < results.length; i++) {
          entityScores.put(results[i].getCollection(), results[i].getScore());
        }
        Bundle extras = new Bundle();
        if (isSerializedEntityDataEnabled) {
          ExtrasUtils.putEntities(extras, results);
        }
        builder.addLink(span.getStartIndex(), span.getEndIndex(), entityScores, extras);
      }
      final TextLinks links = builder.build();
      final long endTimeMs = System.currentTimeMillis();
      final String callingPackageName =
          request.getCallingPackageName() == null
              ? context.getPackageName() // local (in process) TC.
              : request.getCallingPackageName();
      Optional<ModelInfo> annotatorModelInfo;
      Optional<ModelInfo> langIdModelInfo;
      synchronized (lock) {
        annotatorModelInfo =
            Optional.fromNullable(annotatorModelInUse).transform(ModelFile::toModelInfo);
        langIdModelInfo = Optional.fromNullable(langIdModelInUse).transform(ModelFile::toModelInfo);
      }
      generateLinksLogger.logGenerateLinks(
          request.getText(),
          links,
          callingPackageName,
          endTimeMs - startTimeMs,
          annotatorModelInfo,
          langIdModelInfo);
      return links;
    } catch (Throwable t) {
      // Avoid throwing from this method. Log the error.
      TcLog.e(TAG, "Error getting links info.", t);
    }
    return fallback.generateLinks(request);
  }

  int getMaxGenerateLinksTextLength() {
    return settings.getGenerateLinksMaxTextLength();
  }

  private Collection<String> getEntitiesForHints(Collection<String> hints) {
    final boolean editable = hints.contains(TextClassifier.HINT_TEXT_IS_EDITABLE);
    final boolean notEditable = hints.contains(TextClassifier.HINT_TEXT_IS_NOT_EDITABLE);

    // Use the default if there is no hint, or conflicting ones.
    final boolean useDefault = editable == notEditable;
    if (useDefault) {
      return settings.getEntityListDefault();
    } else if (editable) {
      return settings.getEntityListEditable();
    } else { // notEditable
      return settings.getEntityListNotEditable();
    }
  }

  void onSelectionEvent(SelectionEvent event) {
    TextClassifierEvent textClassifierEvent = SelectionEventConverter.toTextClassifierEvent(event);
    if (textClassifierEvent == null) {
      return;
    }
    onTextClassifierEvent(event.getSessionId(), textClassifierEvent);
  }

  void onTextClassifierEvent(
      @Nullable TextClassificationSessionId sessionId, TextClassifierEvent event) {
    textClassifierEventLogger.writeEvent(
        TextClassificationSessionIdConverter.fromPlatform(sessionId),
        TextClassifierEventConverter.fromPlatform(event));
  }

  TextLanguage detectLanguage(TextLanguage.Request request) {
    Preconditions.checkNotNull(request);
    checkMainThread();
    try {
      final TextLanguage.Builder builder = new TextLanguage.Builder();
      Optional<LangIdModel> langIdImpl = getLangIdImpl();
      if (langIdImpl.isPresent()) {
        final LangIdModel.LanguageResult[] langResults =
            langIdImpl.get().detectLanguages(request.getText().toString());
        for (int i = 0; i < langResults.length; i++) {
          builder.putLocale(
              ULocale.forLanguageTag(langResults[i].getLanguage()), langResults[i].getScore());
        }
        return builder.build();
      }
    } catch (Throwable t) {
      // Avoid throwing from this method. Log the error.
      TcLog.e(TAG, "Error detecting text language.", t);
    }
    return fallback.detectLanguage(request);
  }

  ConversationActions suggestConversationActions(ConversationActions.Request request) {
    Preconditions.checkNotNull(request);
    checkMainThread();
    try {
      ActionsSuggestionsModel actionsImpl = getActionsImpl();
      if (actionsImpl == null) {
        // Actions model is optional, fallback if it is not available.
        return fallback.suggestConversationActions(request);
      }
      Optional<LangIdModel> langId = getLangIdImpl();
      ActionsSuggestionsModel.ConversationMessage[] nativeMessages =
          ActionsSuggestionsHelper.toNativeMessages(
              request.getConversation(), text -> detectLanguageTags(langId, text));
      if (nativeMessages.length == 0) {
        return fallback.suggestConversationActions(request);
      }
      ActionsSuggestionsModel.Conversation nativeConversation =
          new ActionsSuggestionsModel.Conversation(nativeMessages);

      ActionsSuggestionsModel.ActionSuggestion[] nativeSuggestions =
          actionsImpl.suggestActionsWithIntents(
              nativeConversation,
              null,
              context,
              getResourceLocalesString(),
              getAnnotatorImpl(LocaleList.getDefault()));
      return createConversationActionResult(request, nativeSuggestions);
    } catch (Throwable t) {
      // Avoid throwing from this method. Log the error.
      TcLog.e(TAG, "Error suggesting conversation actions.", t);
    }
    return fallback.suggestConversationActions(request);
  }

  /**
   * Returns the {@link ConversationAction} result, with a non-null extras.
   *
   * <p>Whenever the RemoteAction is non-null, you can expect its corresponding intent with a
   * non-null component name is in the extras.
   */
  private ConversationActions createConversationActionResult(
      ConversationActions.Request request,
      ActionsSuggestionsModel.ActionSuggestion[] nativeSuggestions) {
    Collection<String> expectedTypes = resolveActionTypesFromRequest(request);
    List<ConversationAction> conversationActions = new ArrayList<>();
    for (ActionsSuggestionsModel.ActionSuggestion nativeSuggestion : nativeSuggestions) {
      String actionType = nativeSuggestion.getActionType();
      if (!expectedTypes.contains(actionType)) {
        continue;
      }
      LabeledIntent.Result labeledIntentResult =
          ActionsSuggestionsHelper.createLabeledIntentResult(
              context, templateIntentFactory, nativeSuggestion);
      RemoteAction remoteAction = null;
      Bundle extras = new Bundle();
      if (labeledIntentResult != null) {
        remoteAction = labeledIntentResult.remoteAction.toRemoteAction();
        ExtrasUtils.putActionIntent(
            extras, stripPackageInfoFromIntent(labeledIntentResult.resolvedIntent));
      }
      ExtrasUtils.putSerializedEntityData(extras, nativeSuggestion.getSerializedEntityData());
      ExtrasUtils.putEntitiesExtras(
          extras, TemplateIntentFactory.nameVariantsToBundle(nativeSuggestion.getEntityData()));
      conversationActions.add(
          new ConversationAction.Builder(actionType)
              .setConfidenceScore(nativeSuggestion.getScore())
              .setTextReply(nativeSuggestion.getResponseText())
              .setAction(remoteAction)
              .setExtras(extras)
              .build());
    }
    conversationActions = ActionsSuggestionsHelper.removeActionsWithDuplicates(conversationActions);
    if (request.getMaxSuggestions() >= 0
        && conversationActions.size() > request.getMaxSuggestions()) {
      conversationActions = conversationActions.subList(0, request.getMaxSuggestions());
    }
    synchronized (lock) {
      String resultId =
          ActionsSuggestionsHelper.createResultId(
              context,
              request.getConversation(),
              Optional.fromNullable(actionModelInUse),
              Optional.fromNullable(annotatorModelInUse),
              Optional.fromNullable(langIdModelInUse));
      return new ConversationActions(conversationActions, resultId);
    }
  }

  private Collection<String> resolveActionTypesFromRequest(ConversationActions.Request request) {
    List<String> defaultActionTypes =
        request.getHints().contains(ConversationActions.Request.HINT_FOR_NOTIFICATION)
            ? settings.getNotificationConversationActionTypes()
            : settings.getInAppConversationActionTypes();
    return request.getTypeConfig().resolveEntityListModifications(defaultActionTypes);
  }

  private AnnotatorModel getAnnotatorImpl(LocaleList localeList) throws FileNotFoundException {
    synchronized (lock) {
      localeList = localeList == null ? LocaleList.getDefault() : localeList;
      final ModelFileManager.ModelFile bestModel =
          annotatorModelFileManager.findBestModelFile(localeList);
      if (bestModel == null) {
        throw new FileNotFoundException("No annotator model for " + localeList.toLanguageTags());
      }
      if (annotatorImpl == null || !Objects.equals(annotatorModelInUse, bestModel)) {
        TcLog.d(TAG, "Loading " + bestModel);
        final ParcelFileDescriptor pfd =
            ParcelFileDescriptor.open(
                new File(bestModel.getPath()), ParcelFileDescriptor.MODE_READ_ONLY);
        try {
          if (pfd != null) {
            // The current annotator model may be still used by another thread / model.
            // Do not call close() here, and let the GC to clean it up when no one else
            // is using it.
            annotatorImpl = new AnnotatorModel(pfd.getFd());
            Optional<LangIdModel> langIdModel = getLangIdImpl();
            if (langIdModel.isPresent()) {
              annotatorImpl.setLangIdModel(langIdModel.get());
            }
            annotatorModelInUse = bestModel;
          }
        } finally {
          maybeCloseAndLogError(pfd);
        }
      }
      return annotatorImpl;
    }
  }

  private Optional<LangIdModel> getLangIdImpl() {
    synchronized (lock) {
      final ModelFileManager.ModelFile bestModel = langIdModelFileManager.findBestModelFile(null);
      if (bestModel == null) {
        return Optional.absent();
      }
      if (langIdImpl == null || !Objects.equals(langIdModelInUse, bestModel)) {
        TcLog.d(TAG, "Loading " + bestModel);
        final ParcelFileDescriptor pfd;
        try {
          pfd =
              ParcelFileDescriptor.open(
                  new File(bestModel.getPath()), ParcelFileDescriptor.MODE_READ_ONLY);
        } catch (FileNotFoundException e) {
          TcLog.e(TAG, "Failed to open the LangID model file", e);
          return Optional.absent();
        }
        try {
          if (pfd != null) {
            langIdImpl = new LangIdModel(pfd.getFd());
            langIdModelInUse = bestModel;
          }
        } finally {
          maybeCloseAndLogError(pfd);
        }
      }
      return Optional.of(langIdImpl);
    }
  }

  @Nullable
  private ActionsSuggestionsModel getActionsImpl() throws FileNotFoundException {
    synchronized (lock) {
      // TODO: Use LangID to determine the locale we should use here?
      final ModelFileManager.ModelFile bestModel =
          actionsModelFileManager.findBestModelFile(LocaleList.getDefault());
      if (bestModel == null) {
        return null;
      }
      if (actionsImpl == null || !Objects.equals(actionModelInUse, bestModel)) {
        TcLog.d(TAG, "Loading " + bestModel);
        final ParcelFileDescriptor pfd =
            ParcelFileDescriptor.open(
                new File(bestModel.getPath()), ParcelFileDescriptor.MODE_READ_ONLY);
        try {
          if (pfd == null) {
            TcLog.d(TAG, "Failed to read the model file: " + bestModel.getPath());
            return null;
          }
          actionsImpl = new ActionsSuggestionsModel(pfd.getFd());
          actionModelInUse = bestModel;
        } finally {
          maybeCloseAndLogError(pfd);
        }
      }
      return actionsImpl;
    }
  }

  private String createAnnotatorId(String text, int start, int end) {
    synchronized (lock) {
      return ResultIdUtils.createId(
          context,
          text,
          start,
          end,
          ModelFile.toModelInfos(
              Optional.fromNullable(annotatorModelInUse), Optional.fromNullable(langIdModelInUse)));
    }
  }

  private static String concatenateLocales(@Nullable LocaleList locales) {
    return (locales == null) ? "" : locales.toLanguageTags();
  }

  private TextClassification createClassificationResult(
      AnnotatorModel.ClassificationResult[] classifications,
      String text,
      int start,
      int end,
      Optional<LangIdModel> langId) {
    final String classifiedText = text.substring(start, end);
    final TextClassification.Builder builder =
        new TextClassification.Builder().setText(classifiedText);

    final int typeCount = classifications.length;
    AnnotatorModel.ClassificationResult highestScoringResult =
        typeCount > 0 ? classifications[0] : null;
    for (int i = 0; i < typeCount; i++) {
      builder.setEntityType(classifications[i].getCollection(), classifications[i].getScore());
      if (classifications[i].getScore() > highestScoringResult.getScore()) {
        highestScoringResult = classifications[i];
      }
    }

    boolean isPrimaryAction = true;
    final ImmutableList<LabeledIntent> labeledIntents =
        highestScoringResult == null
            ? ImmutableList.of()
            : templateIntentFactory.create(highestScoringResult.getRemoteActionTemplates());
    final LabeledIntent.TitleChooser titleChooser =
        (labeledIntent, resolveInfo) -> labeledIntent.titleWithoutEntity;

    ArrayList<Intent> actionIntents = new ArrayList<>();
    for (LabeledIntent labeledIntent : labeledIntents) {
      final LabeledIntent.Result result = labeledIntent.resolve(context, titleChooser);
      if (result == null) {
        continue;
      }

      final Intent intent = result.resolvedIntent;
      final RemoteAction action = result.remoteAction.toRemoteAction();
      if (isPrimaryAction) {
        // For O backwards compatibility, the first RemoteAction is also written to the
        // legacy API fields.
        builder.setIcon(action.getIcon().loadDrawable(context));
        builder.setLabel(action.getTitle().toString());
        builder.setIntent(intent);
        builder.setOnClickListener(
            createIntentOnClickListener(
                createPendingIntent(context, intent, labeledIntent.requestCode)));
        isPrimaryAction = false;
      }
      builder.addAction(action);
      actionIntents.add(intent);
    }
    Bundle extras = new Bundle();
    Optional<Bundle> foreignLanguageExtra =
        langId
            .transform(model -> maybeCreateExtrasForTranslate(actionIntents, model))
            .or(Optional.<Bundle>absent());
    if (foreignLanguageExtra.isPresent()) {
      ExtrasUtils.putForeignLanguageExtra(extras, foreignLanguageExtra.get());
    }
    if (actionIntents.stream().anyMatch(Objects::nonNull)) {
      ArrayList<Intent> strippedIntents =
          actionIntents.stream()
              .map(TextClassifierImpl::stripPackageInfoFromIntent)
              .collect(toCollection(ArrayList::new));
      ExtrasUtils.putActionsIntents(extras, strippedIntents);
    }
    ExtrasUtils.putEntities(extras, classifications);
    builder.setExtras(extras);
    String resultId = createAnnotatorId(text, start, end);
    return builder.setId(resultId).build();
  }

  private static OnClickListener createIntentOnClickListener(final PendingIntent intent) {
    Preconditions.checkNotNull(intent);
    return v -> {
      try {
        intent.send();
      } catch (PendingIntent.CanceledException e) {
        TcLog.e(TAG, "Error sending PendingIntent", e);
      }
    };
  }

  private static Optional<Bundle> maybeCreateExtrasForTranslate(
      List<Intent> intents, LangIdModel langId) {
    Optional<Intent> translateIntent =
        FluentIterable.from(intents)
            .filter(Objects::nonNull)
            .filter(intent -> Intent.ACTION_TRANSLATE.equals(intent.getAction()))
            .first();
    if (!translateIntent.isPresent()) {
      return Optional.absent();
    }
    Pair<String, Float> topLanguageWithScore = ExtrasUtils.getTopLanguage(translateIntent.get());
    if (topLanguageWithScore == null) {
      return Optional.absent();
    }
    return Optional.of(
        ExtrasUtils.createForeignLanguageExtra(
            topLanguageWithScore.first, topLanguageWithScore.second, langId.getVersion()));
  }

  private ImmutableList<String> detectLanguageTags(
      Optional<LangIdModel> langId, CharSequence text) {
    return langId
        .transform(
            model -> {
              float threshold = getLangIdThreshold(model);
              EntityConfidence languagesConfidence = detectLanguages(model, text, threshold);
              return ImmutableList.copyOf(languagesConfidence.getEntities());
            })
        .or(ImmutableList.of());
  }

  /**
   * Detects languages for the specified text. Only returns languages with score that is higher than
   * or equal to the specified threshold.
   */
  private static EntityConfidence detectLanguages(
      LangIdModel langId, CharSequence text, float threshold) {
    final LangIdModel.LanguageResult[] langResults = langId.detectLanguages(text.toString());
    final Map<String, Float> languagesMap = new ArrayMap<>();
    for (LangIdModel.LanguageResult langResult : langResults) {
      if (langResult.getScore() >= threshold) {
        languagesMap.put(langResult.getLanguage(), langResult.getScore());
      }
    }
    return new EntityConfidence(languagesMap);
  }

  private float getLangIdThreshold(LangIdModel langId) {
    return settings.getLangIdThresholdOverride() >= 0
        ? settings.getLangIdThresholdOverride()
        : langId.getLangIdThreshold();
  }

  void dump(IndentingPrintWriter printWriter) {
    synchronized (lock) {
      printWriter.println("TextClassifierImpl:");
      printWriter.increaseIndent();
      printWriter.println("Annotator model file(s):");
      printWriter.increaseIndent();
      for (ModelFileManager.ModelFile modelFile : annotatorModelFileManager.listModelFiles()) {
        printWriter.println(modelFile.toString());
      }
      printWriter.decreaseIndent();
      printWriter.println("LangID model file(s):");
      printWriter.increaseIndent();
      for (ModelFileManager.ModelFile modelFile : langIdModelFileManager.listModelFiles()) {
        printWriter.println(modelFile.toString());
      }
      printWriter.decreaseIndent();
      printWriter.println("Actions model file(s):");
      printWriter.increaseIndent();
      for (ModelFileManager.ModelFile modelFile : actionsModelFileManager.listModelFiles()) {
        printWriter.println(modelFile.toString());
      }
      printWriter.decreaseIndent();
      printWriter.printPair("mFallback", fallback);
      printWriter.decreaseIndent();
      printWriter.println();
      settings.dump(printWriter);
    }
  }

  /** Returns the locales string for the current resources configuration. */
  private String getResourceLocalesString() {
    try {
      return context.getResources().getConfiguration().getLocales().toLanguageTags();
    } catch (NullPointerException e) {

      // NPE is unexpected. Erring on the side of caution.
      return LocaleList.getDefault().toLanguageTags();
    }
  }

  /** Closes the ParcelFileDescriptor, if non-null, and logs any errors that occur. */
  private static void maybeCloseAndLogError(@Nullable ParcelFileDescriptor fd) {
    if (fd == null) {
      return;
    }

    try {
      fd.close();
    } catch (IOException e) {
      TcLog.e(TAG, "Error closing file.", e);
    }
  }

  private static void checkMainThread() {
    if (Looper.myLooper() == Looper.getMainLooper()) {
      TcLog.e(TAG, "TextClassifier called on main thread", new Exception());
    }
  }

  private static PendingIntent createPendingIntent(
      final Context context, final Intent intent, int requestCode) {
    return PendingIntent.getActivity(
        context, requestCode, intent, PendingIntent.FLAG_UPDATE_CURRENT);
  }

  @Nullable
  private static Intent stripPackageInfoFromIntent(@Nullable Intent intent) {
    if (intent == null) {
      return null;
    }
    Intent strippedIntent = new Intent(intent);
    strippedIntent.setPackage(null);
    strippedIntent.setComponent(null);
    return strippedIntent;
  }
}
