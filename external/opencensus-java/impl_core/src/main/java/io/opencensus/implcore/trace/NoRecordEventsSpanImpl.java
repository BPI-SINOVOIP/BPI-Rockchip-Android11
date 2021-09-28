/*
 * Copyright 2018, OpenCensus Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package io.opencensus.implcore.trace;

import com.google.common.base.Preconditions;
import io.opencensus.trace.Annotation;
import io.opencensus.trace.AttributeValue;
import io.opencensus.trace.EndSpanOptions;
import io.opencensus.trace.Link;
import io.opencensus.trace.Span;
import io.opencensus.trace.SpanContext;
import io.opencensus.trace.Status;
import java.util.EnumSet;
import java.util.Map;

/** Implementation for the {@link Span} class that does not record trace events. */
final class NoRecordEventsSpanImpl extends Span {

  private static final EnumSet<Options> NOT_RECORD_EVENTS_SPAN_OPTIONS =
      EnumSet.noneOf(Span.Options.class);

  static NoRecordEventsSpanImpl create(SpanContext context) {
    return new NoRecordEventsSpanImpl(context);
  }

  @Override
  public void addAnnotation(String description, Map<String, AttributeValue> attributes) {
    Preconditions.checkNotNull(description, "description");
    Preconditions.checkNotNull(attributes, "attribute");
  }

  @Override
  public void addAnnotation(Annotation annotation) {
    Preconditions.checkNotNull(annotation, "annotation");
  }

  @Override
  public void putAttribute(String key, AttributeValue value) {
    Preconditions.checkNotNull(key, "key");
    Preconditions.checkNotNull(value, "value");
  }

  @Override
  public void putAttributes(Map<String, AttributeValue> attributes) {
    Preconditions.checkNotNull(attributes, "attributes");
  }

  @Override
  public void addMessageEvent(io.opencensus.trace.MessageEvent messageEvent) {
    Preconditions.checkNotNull(messageEvent, "messageEvent");
  }

  @Override
  public void addLink(Link link) {
    Preconditions.checkNotNull(link, "link");
  }

  @Override
  public void setStatus(Status status) {
    Preconditions.checkNotNull(status, "status");
  }

  @Override
  public void end(EndSpanOptions options) {
    Preconditions.checkNotNull(options, "options");
  }

  private NoRecordEventsSpanImpl(SpanContext context) {
    super(context, NOT_RECORD_EVENTS_SPAN_OPTIONS);
  }
}
