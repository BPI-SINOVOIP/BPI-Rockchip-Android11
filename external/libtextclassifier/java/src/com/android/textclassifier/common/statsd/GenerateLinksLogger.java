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

package com.android.textclassifier.common.statsd;

import android.util.StatsEvent;
import android.util.StatsLog;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextLinks;
import androidx.collection.ArrayMap;
import com.android.textclassifier.common.base.TcLog;
import com.android.textclassifier.common.logging.ResultIdUtils.ModelInfo;
import com.android.textclassifier.common.logging.TextClassifierEvent;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableList;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.UUID;
import java.util.function.Supplier;
import javax.annotation.Nullable;

/** A helper for logging calls to generateLinks. */
public final class GenerateLinksLogger {

  private static final String LOG_TAG = "GenerateLinksLogger";

  private final Random random;
  private final int sampleRate;
  private final Supplier<String> randomUuidSupplier;

  /**
   * @param sampleRate the rate at which log events are written. (e.g. 100 means there is a 0.01
   *     chance that a call to logGenerateLinks results in an event being written). To write all
   *     events, pass 1.
   */
  public GenerateLinksLogger(int sampleRate) {
    this(sampleRate, () -> UUID.randomUUID().toString());
  }

  /**
   * @param sampleRate the rate at which log events are written. (e.g. 100 means there is a 0.01
   *     chance that a call to logGenerateLinks results in an event being written). To write all
   *     events, pass 1.
   * @param randomUuidSupplier supplies random UUIDs.
   */
  @VisibleForTesting
  GenerateLinksLogger(int sampleRate, Supplier<String> randomUuidSupplier) {
    this.sampleRate = sampleRate;
    random = new Random();
    this.randomUuidSupplier = Preconditions.checkNotNull(randomUuidSupplier);
  }

  /** Logs statistics about a call to generateLinks. */
  public void logGenerateLinks(
      CharSequence text,
      TextLinks links,
      String callingPackageName,
      long latencyMs,
      Optional<ModelInfo> annotatorModel,
      Optional<ModelInfo> langIdModel) {
    Preconditions.checkNotNull(text);
    Preconditions.checkNotNull(links);
    Preconditions.checkNotNull(callingPackageName);
    if (!shouldLog()) {
      return;
    }

    // Always populate the total stats, and per-entity stats for each entity type detected.
    final LinkifyStats totalStats = new LinkifyStats();
    final Map<String, LinkifyStats> perEntityTypeStats = new ArrayMap<>();
    for (TextLinks.TextLink link : links.getLinks()) {
      if (link.getEntityCount() == 0) {
        continue;
      }
      final String entityType = link.getEntity(0);
      if (entityType == null
          || TextClassifier.TYPE_OTHER.equals(entityType)
          || TextClassifier.TYPE_UNKNOWN.equals(entityType)) {
        continue;
      }
      totalStats.countLink(link);
      perEntityTypeStats.computeIfAbsent(entityType, k -> new LinkifyStats()).countLink(link);
    }

    final String callId = randomUuidSupplier.get();
    writeStats(
        callId, callingPackageName, null, totalStats, text, latencyMs, annotatorModel, langIdModel);
    // Sort the entity types to ensure the logging order is deterministic.
    ImmutableList<String> sortedEntityTypes =
        ImmutableList.sortedCopyOf(perEntityTypeStats.keySet());
    for (String entityType : sortedEntityTypes) {
      writeStats(
          callId,
          callingPackageName,
          entityType,
          perEntityTypeStats.get(entityType),
          text,
          latencyMs,
          annotatorModel,
          langIdModel);
    }
  }

  /**
   * Returns whether this particular event should be logged.
   *
   * <p>Sampling is used to reduce the amount of logging data generated.
   */
  private boolean shouldLog() {
    if (sampleRate <= 1) {
      return true;
    } else {
      return random.nextInt(sampleRate) == 0;
    }
  }

  /** Writes a log event for the given stats. */
  private static void writeStats(
      String callId,
      String callingPackageName,
      @Nullable String entityType,
      LinkifyStats stats,
      CharSequence text,
      long latencyMs,
      Optional<ModelInfo> annotatorModel,
      Optional<ModelInfo> langIdModel) {
    String annotatorModelName = annotatorModel.transform(ModelInfo::toModelName).or("");
    String langIdModelName = langIdModel.transform(ModelInfo::toModelName).or("");
    StatsEvent statsEvent =
        StatsEvent.newBuilder()
            .setAtomId(TextClassifierEventLogger.TEXT_LINKIFY_EVENT_ATOM_ID)
            .writeString(callId)
            .writeInt(TextClassifierEvent.TYPE_LINKS_GENERATED)
            .writeString(annotatorModelName)
            .writeInt(TextClassifierEventLogger.WidgetType.WIDGET_TYPE_UNKNOWN)
            .writeInt(/* eventIndex */ 0)
            .writeString(entityType)
            .writeInt(stats.numLinks)
            .writeInt(stats.numLinksTextLength)
            .writeInt(text.length())
            .writeLong(latencyMs)
            .writeString(callingPackageName)
            .writeString(langIdModelName)
            .usePooledBuffer()
            .build();
    StatsLog.write(statsEvent);

    if (TcLog.ENABLE_FULL_LOGGING) {
      TcLog.v(
          LOG_TAG,
          String.format(
              Locale.US,
              "%s:%s %d links (%d/%d chars) %dms %s annotator=%s langid=%s",
              callId,
              entityType,
              stats.numLinks,
              stats.numLinksTextLength,
              text.length(),
              latencyMs,
              callingPackageName,
              annotatorModelName,
              langIdModelName));
    }
  }

  /** Helper class for storing per-entity type statistics. */
  private static final class LinkifyStats {
    int numLinks;
    int numLinksTextLength;

    void countLink(TextLinks.TextLink link) {
      numLinks += 1;
      numLinksTextLength += link.getEnd() - link.getStart();
    }
  }
}
