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

package com.android.textclassifier.common.intent;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.textclassifier.TextClassifier;
import com.android.textclassifier.common.base.TcLog;
import com.google.android.textclassifier.NamedVariant;
import com.google.android.textclassifier.RemoteActionTemplate;
import com.google.common.collect.ImmutableList;
import javax.annotation.Nullable;

/** Creates intents based on {@link RemoteActionTemplate} objects. */
public final class TemplateIntentFactory {
  private static final String TAG = "TemplateIntentFactory";

  /** Constructs and returns a list of {@link LabeledIntent} based on the given templates. */
  public ImmutableList<LabeledIntent> create(
      @Nullable RemoteActionTemplate[] remoteActionTemplates) {
    if (remoteActionTemplates == null || remoteActionTemplates.length == 0) {
      return ImmutableList.of();
    }
    final ImmutableList.Builder<LabeledIntent> labeledIntents = ImmutableList.builder();
    for (RemoteActionTemplate remoteActionTemplate : remoteActionTemplates) {
      if (!isValidTemplate(remoteActionTemplate)) {
        TcLog.w(TAG, "Invalid RemoteActionTemplate skipped.");
        continue;
      }
      labeledIntents.add(
          new LabeledIntent(
              remoteActionTemplate.titleWithoutEntity,
              remoteActionTemplate.titleWithEntity,
              remoteActionTemplate.description,
              remoteActionTemplate.descriptionWithAppName,
              createIntent(remoteActionTemplate),
              remoteActionTemplate.requestCode == null
                  ? LabeledIntent.DEFAULT_REQUEST_CODE
                  : remoteActionTemplate.requestCode));
    }
    return labeledIntents.build();
  }

  private static boolean isValidTemplate(@Nullable RemoteActionTemplate remoteActionTemplate) {
    if (remoteActionTemplate == null) {
      TcLog.w(TAG, "Invalid RemoteActionTemplate: is null");
      return false;
    }
    if (TextUtils.isEmpty(remoteActionTemplate.titleWithEntity)
        && TextUtils.isEmpty(remoteActionTemplate.titleWithoutEntity)) {
      TcLog.w(TAG, "Invalid RemoteActionTemplate: title is null");
      return false;
    }
    if (TextUtils.isEmpty(remoteActionTemplate.description)) {
      TcLog.w(TAG, "Invalid RemoteActionTemplate: description is null");
      return false;
    }
    if (!TextUtils.isEmpty(remoteActionTemplate.packageName)) {
      TcLog.w(TAG, "Invalid RemoteActionTemplate: package name is set");
      return false;
    }
    if (TextUtils.isEmpty(remoteActionTemplate.action)) {
      TcLog.w(TAG, "Invalid RemoteActionTemplate: intent action not set");
      return false;
    }
    return true;
  }

  private static Intent createIntent(RemoteActionTemplate remoteActionTemplate) {
    final Intent intent = new Intent(remoteActionTemplate.action);
    final Uri uri =
        TextUtils.isEmpty(remoteActionTemplate.data)
            ? null
            : Uri.parse(remoteActionTemplate.data).normalizeScheme();
    final String type =
        TextUtils.isEmpty(remoteActionTemplate.type)
            ? null
            : Intent.normalizeMimeType(remoteActionTemplate.type);
    intent.setDataAndType(uri, type);
    intent.setFlags(remoteActionTemplate.flags == null ? 0 : remoteActionTemplate.flags);
    if (!TextUtils.isEmpty(remoteActionTemplate.packageName)) {
      intent.setPackage(remoteActionTemplate.packageName);
    }
    if (remoteActionTemplate.category != null) {
      for (String category : remoteActionTemplate.category) {
        if (category != null) {
          intent.addCategory(category);
        }
      }
    }
    intent.putExtras(nameVariantsToBundle(remoteActionTemplate.extras));
    // If the template does not have EXTRA_FROM_TEXT_CLASSIFIER, create one to indicate the result
    // is from the text classifier, so that client can handle the intent differently.
    if (!intent.hasExtra(TextClassifier.EXTRA_FROM_TEXT_CLASSIFIER)) {
      intent.putExtra(TextClassifier.EXTRA_FROM_TEXT_CLASSIFIER, Bundle.EMPTY);
    }
    return intent;
  }

  /** Converts an array of {@link NamedVariant} to a Bundle and returns it. */
  public static Bundle nameVariantsToBundle(@Nullable NamedVariant[] namedVariants) {
    if (namedVariants == null) {
      return Bundle.EMPTY;
    }
    Bundle bundle = new Bundle();
    for (NamedVariant namedVariant : namedVariants) {
      if (namedVariant == null) {
        continue;
      }
      switch (namedVariant.getType()) {
        case NamedVariant.TYPE_INT:
          bundle.putInt(namedVariant.getName(), namedVariant.getInt());
          break;
        case NamedVariant.TYPE_LONG:
          bundle.putLong(namedVariant.getName(), namedVariant.getLong());
          break;
        case NamedVariant.TYPE_FLOAT:
          bundle.putFloat(namedVariant.getName(), namedVariant.getFloat());
          break;
        case NamedVariant.TYPE_DOUBLE:
          bundle.putDouble(namedVariant.getName(), namedVariant.getDouble());
          break;
        case NamedVariant.TYPE_BOOL:
          bundle.putBoolean(namedVariant.getName(), namedVariant.getBool());
          break;
        case NamedVariant.TYPE_STRING:
          bundle.putString(namedVariant.getName(), namedVariant.getString());
          break;
        case NamedVariant.TYPE_STRING_ARRAY:
          bundle.putStringArray(namedVariant.getName(), namedVariant.getStringArray());
          break;
        case NamedVariant.TYPE_FLOAT_ARRAY:
          bundle.putFloatArray(namedVariant.getName(), namedVariant.getFloatArray());
          break;
        case NamedVariant.TYPE_INT_ARRAY:
          bundle.putIntArray(namedVariant.getName(), namedVariant.getIntArray());
          break;
        case NamedVariant.TYPE_NAMED_VARIANT_ARRAY:
          bundle.putBundle(
              namedVariant.getName(), nameVariantsToBundle(namedVariant.getNamedVariantArray()));
          break;
        default:
          TcLog.w(
              TAG, "Unsupported type found in nameVariantsToBundle : " + namedVariant.getType());
      }
    }
    return bundle;
  }
}
