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

import android.view.textclassifier.SelectionEvent;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassifierEvent;
import javax.annotation.Nullable;

/** Helper class to convert a {@link SelectionEvent} to a {@link TextClassifierEvent}. */
public final class SelectionEventConverter {

  /** Converts a {@link SelectionEvent} to a {@link TextClassifierEvent}. */
  @Nullable
  public static TextClassifierEvent toTextClassifierEvent(SelectionEvent selectionEvent) {
    TextClassificationContext textClassificationContext = null;
    if (selectionEvent.getPackageName() != null && selectionEvent.getWidgetType() != null) {
      textClassificationContext =
          new TextClassificationContext.Builder(
                  selectionEvent.getPackageName(), selectionEvent.getWidgetType())
              .setWidgetVersion(selectionEvent.getWidgetVersion())
              .build();
    }
    if (selectionEvent.getInvocationMethod() == SelectionEvent.INVOCATION_LINK) {
      return new TextClassifierEvent.TextLinkifyEvent.Builder(
              convertEventType(selectionEvent.getEventType()))
          .setEventContext(textClassificationContext)
          .setResultId(selectionEvent.getResultId())
          .setEventIndex(selectionEvent.getEventIndex())
          .setEntityTypes(selectionEvent.getEntityType())
          .build();
    }
    if (selectionEvent.getInvocationMethod() == SelectionEvent.INVOCATION_MANUAL) {
      return new TextClassifierEvent.TextSelectionEvent.Builder(
              convertEventType(selectionEvent.getEventType()))
          .setEventContext(textClassificationContext)
          .setResultId(selectionEvent.getResultId())
          .setEventIndex(selectionEvent.getEventIndex())
          .setEntityTypes(selectionEvent.getEntityType())
          .setRelativeWordStartIndex(selectionEvent.getStart())
          .setRelativeWordEndIndex(selectionEvent.getEnd())
          .setRelativeSuggestedWordStartIndex(selectionEvent.getSmartStart())
          .setRelativeSuggestedWordEndIndex(selectionEvent.getSmartEnd())
          .build();
    }
    return null;
  }

  private static int convertEventType(int eventType) {
    switch (eventType) {
      case SelectionEvent.EVENT_SELECTION_STARTED:
        return TextClassifierEvent.TYPE_SELECTION_STARTED;
      case SelectionEvent.EVENT_SELECTION_MODIFIED:
        return TextClassifierEvent.TYPE_SELECTION_MODIFIED;
      case SelectionEvent.EVENT_SMART_SELECTION_SINGLE:
        return SelectionEvent.EVENT_SMART_SELECTION_SINGLE;
      case SelectionEvent.EVENT_SMART_SELECTION_MULTI:
        return SelectionEvent.EVENT_SMART_SELECTION_MULTI;
      case SelectionEvent.EVENT_AUTO_SELECTION:
        return SelectionEvent.EVENT_AUTO_SELECTION;
      case SelectionEvent.ACTION_OVERTYPE:
        return TextClassifierEvent.TYPE_OVERTYPE;
      case SelectionEvent.ACTION_COPY:
        return TextClassifierEvent.TYPE_COPY_ACTION;
      case SelectionEvent.ACTION_PASTE:
        return TextClassifierEvent.TYPE_PASTE_ACTION;
      case SelectionEvent.ACTION_CUT:
        return TextClassifierEvent.TYPE_CUT_ACTION;
      case SelectionEvent.ACTION_SHARE:
        return TextClassifierEvent.TYPE_SHARE_ACTION;
      case SelectionEvent.ACTION_SMART_SHARE:
        return TextClassifierEvent.TYPE_SMART_ACTION;
      case SelectionEvent.ACTION_DRAG:
        return TextClassifierEvent.TYPE_SELECTION_DRAG;
      case SelectionEvent.ACTION_ABANDON:
        return TextClassifierEvent.TYPE_SELECTION_DESTROYED;
      case SelectionEvent.ACTION_OTHER:
        return TextClassifierEvent.TYPE_OTHER_ACTION;
      case SelectionEvent.ACTION_SELECT_ALL:
        return TextClassifierEvent.TYPE_SELECT_ALL;
      case SelectionEvent.ACTION_RESET:
        return TextClassifierEvent.TYPE_SELECTION_RESET;
      default:
        return 0;
    }
  }

  private SelectionEventConverter() {}
}
