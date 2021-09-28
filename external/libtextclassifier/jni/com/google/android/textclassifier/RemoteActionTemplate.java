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

/**
 * Represents a template for an Android RemoteAction.
 *
 * @hide
 */
public final class RemoteActionTemplate {
  /** Title shown for the action (see: RemoteAction.getTitle). */
  public final String titleWithoutEntity;

  /** Title with entity for the action. */
  public final String titleWithEntity;

  /** Description shown for the action (see: RemoteAction.getContentDescription). */
  public final String description;

  /**
   * Description shown for the action (see: RemoteAction.getContentDescription) when app name is
   * available. Caller is expected to replace the placeholder by the name of the app that is going
   * to handle the action.
   */
  public final String descriptionWithAppName;

  /** The action to set on the Intent (see: Intent.setAction). */
  public final String action;

  /** The data to set on the Intent (see: Intent.setData). */
  public final String data;

  /** The type to set on the Intent (see: Intent.setType). */
  public final String type;

  /** Flags for launching the Intent (see: Intent.setFlags). */
  public final Integer flags;

  /** Categories to set on the Intent (see: Intent.addCategory). */
  public final String[] category;

  /** Explicit application package to set on the Intent (see: Intent.setPackage). */
  public final String packageName;

  /** The list of all the extras to add to the Intent. */
  public final NamedVariant[] extras;

  /** Private request code to use for the Intent. */
  public final Integer requestCode;

  public RemoteActionTemplate(
      String titleWithoutEntity,
      String titleWithEntity,
      String description,
      String descriptionWithAppName,
      String action,
      String data,
      String type,
      Integer flags,
      String[] category,
      String packageName,
      NamedVariant[] extras,
      Integer requestCode) {
    this.titleWithoutEntity = titleWithoutEntity;
    this.titleWithEntity = titleWithEntity;
    this.description = description;
    this.descriptionWithAppName = descriptionWithAppName;
    this.action = action;
    this.data = data;
    this.type = type;
    this.flags = flags;
    this.category = category;
    this.packageName = packageName;
    this.extras = extras;
    this.requestCode = requestCode;
  }
}
