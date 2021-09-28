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

import androidx.annotation.FloatRange;
import androidx.collection.ArrayMap;
import com.google.common.base.Preconditions;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/** Helper object for setting and getting entity scores for classified text. */
final class EntityConfidence {

  static final EntityConfidence EMPTY = new EntityConfidence(Collections.emptyMap());

  private final ArrayMap<String, Float> entityConfidence = new ArrayMap<>();
  private final ArrayList<String> sortedEntities = new ArrayList<>();

  /**
   * Constructs an EntityConfidence from a map of entity to confidence.
   *
   * <p>Map entries that have 0 confidence are removed, and values greater than 1 are clamped to 1.
   *
   * @param source a map from entity to a confidence value in the range 0 (low confidence) to 1
   *     (high confidence).
   */
  EntityConfidence(Map<String, Float> source) {
    Preconditions.checkNotNull(source);

    // Prune non-existent entities and clamp to 1.
    entityConfidence.ensureCapacity(source.size());
    for (Map.Entry<String, Float> it : source.entrySet()) {
      if (it.getValue() <= 0) {
        continue;
      }
      entityConfidence.put(it.getKey(), Math.min(1, it.getValue()));
    }
    resetSortedEntitiesFromMap();
  }

  /**
   * Returns an immutable list of entities found in the classified text ordered from high confidence
   * to low confidence.
   */
  public List<String> getEntities() {
    return Collections.unmodifiableList(sortedEntities);
  }

  /**
   * Returns the confidence score for the specified entity. The value ranges from 0 (low confidence)
   * to 1 (high confidence). 0 indicates that the entity was not found for the classified text.
   */
  @FloatRange(from = 0.0, to = 1.0)
  public float getConfidenceScore(String entity) {
    return entityConfidence.getOrDefault(entity, 0f);
  }

  @Override
  public String toString() {
    return entityConfidence.toString();
  }

  private void resetSortedEntitiesFromMap() {
    sortedEntities.clear();
    sortedEntities.ensureCapacity(entityConfidence.size());
    sortedEntities.addAll(entityConfidence.keySet());
    sortedEntities.sort(
        (e1, e2) -> {
          float score1 = entityConfidence.get(e1);
          float score2 = entityConfidence.get(e2);
          return Float.compare(score2, score1);
        });
  }
}
