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

package com.android.textclassifier.subjects;

import static com.google.common.truth.Truth.assertAbout;

import com.android.textclassifier.Entity;
import com.google.common.truth.FailureMetadata;
import com.google.common.truth.MathUtil;
import com.google.common.truth.Subject;
import javax.annotation.Nullable;

/** Test helper for checking {@link com.android.textclassifier.Entity} results. */
public final class EntitySubject extends Subject<EntitySubject, Entity> {

  private static final float TOLERANCE = 0.0001f;

  private final Entity entity;

  public static EntitySubject assertThat(@Nullable Entity entity) {
    return assertAbout(EntitySubject::new).that(entity);
  }

  private EntitySubject(FailureMetadata failureMetadata, @Nullable Entity entity) {
    super(failureMetadata, entity);
    this.entity = entity;
  }

  public void isMatchWithinTolerance(@Nullable Entity entity) {
    if (!entity.getEntityType().equals(this.entity.getEntityType())) {
      failWithActual("expected to have type", entity.getEntityType());
    }
    if (!MathUtil.equalWithinTolerance(entity.getScore(), this.entity.getScore(), TOLERANCE)) {
      failWithActual("expected to have confidence score", entity.getScore());
    }
  }
}
