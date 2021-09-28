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

import android.app.Notification;
import android.app.Notification.Action;
import com.google.common.collect.ImmutableList;
import java.util.List;

/** Suggestions on a given conversation. */
public final class SmartSuggestions {
  private final ImmutableList<CharSequence> replies;
  private final ImmutableList<Notification.Action> actions;

  public SmartSuggestions(List<CharSequence> replies, List<Notification.Action> actions) {
    this.replies = ImmutableList.copyOf(replies);
    this.actions = ImmutableList.copyOf(actions);
  }

  /** Gets a list of suggested replies. */
  public ImmutableList<CharSequence> getReplies() {
    return replies;
  }

  /** Gets a list of suggested actions. */
  public ImmutableList<Action> getActions() {
    return actions;
  }
}
