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
import com.google.common.base.Objects;
import com.google.common.base.Preconditions;

/** A representation of an identified entity with the confidence score */
public final class Entity implements Comparable<Entity> {

  private final String entityType;
  private final float score;

  public Entity(String entityType, float score) {
    this.entityType = Preconditions.checkNotNull(entityType);
    this.score = score;
  }

  public String getEntityType() {
    return entityType;
  }

  /**
   * Returns the confidence score of the entity, which ranged from 0.0 (low confidence) to 1.0 (high
   * confidence).
   */
  @FloatRange(from = 0.0, to = 1.0)
  public Float getScore() {
    return score;
  }

  @Override
  public int hashCode() {
    return Objects.hashCode(entityType, score);
  }

  @Override
  public boolean equals(Object o) {
    if (this == o) {
      return true;
    }
    if (o == null || getClass() != o.getClass()) {
      return false;
    }
    Entity entity = (Entity) o;
    return Float.compare(entity.score, score) == 0
        && java.util.Objects.equals(entityType, entity.entityType);
  }

  @Override
  public String toString() {
    return "Entity{" + entityType + ": " + score + "}";
  }

  @Override
  public int compareTo(Entity entity) {
    // This method is implemented for sorting Entity. Sort the entities by the confidence score
    // in descending order firstly. If the scores are the same, then sort them by the entity
    // type in ascending order.
    int result = Float.compare(entity.getScore(), score);
    if (result == 0) {
      return entityType.compareTo(entity.getEntityType());
    }
    return result;
  }
}
