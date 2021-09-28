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

package com.android.textclassifier.notification;

/** Configurations for the smart actions feature. */
public interface SmartSuggestionsConfig {
  /** To generate contextual replies for notifications or not. */
  boolean shouldGenerateReplies();

  /** To generate contextual actions for notifications or not. */
  boolean shouldGenerateActions();

  /** The maximum number of suggestions to generate for a conversation. */
  int getMaxSuggestions();

  /**
   * The maximum number of messages to should be extracted from a conversation when constructing
   * suggestions for that conversation.
   */
  int getMaxMessagesToExtract();
}
