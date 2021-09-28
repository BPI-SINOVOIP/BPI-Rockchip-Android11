/*
 * Copyright 2017, OpenCensus Authors
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

import static com.google.common.base.Preconditions.checkNotNull;

import io.opencensus.common.Clock;
import io.opencensus.implcore.internal.TimestampConverter;
import io.opencensus.implcore.trace.internal.RandomHandler;
import io.opencensus.trace.Link;
import io.opencensus.trace.Link.Type;
import io.opencensus.trace.Sampler;
import io.opencensus.trace.Span;
import io.opencensus.trace.Span.Kind;
import io.opencensus.trace.SpanBuilder;
import io.opencensus.trace.SpanContext;
import io.opencensus.trace.SpanId;
import io.opencensus.trace.TraceId;
import io.opencensus.trace.TraceOptions;
import io.opencensus.trace.Tracestate;
import io.opencensus.trace.config.TraceConfig;
import io.opencensus.trace.config.TraceParams;
import java.util.Collections;
import java.util.List;
import java.util.Random;
import javax.annotation.Nullable;

/** Implementation of the {@link SpanBuilder}. */
final class SpanBuilderImpl extends SpanBuilder {
  private static final Tracestate TRACESTATE_DEFAULT = Tracestate.builder().build();

  private static final TraceOptions SAMPLED_TRACE_OPTIONS =
      TraceOptions.builder().setIsSampled(true).build();
  private static final TraceOptions NOT_SAMPLED_TRACE_OPTIONS =
      TraceOptions.builder().setIsSampled(false).build();

  private final Options options;
  private final String name;
  @Nullable private final Span parent;
  @Nullable private final SpanContext remoteParentSpanContext;
  @Nullable private Sampler sampler;
  private List<Span> parentLinks = Collections.<Span>emptyList();
  @Nullable private Boolean recordEvents;
  @Nullable private Kind kind;

  private Span startSpanInternal(
      @Nullable SpanContext parent,
      @Nullable Boolean hasRemoteParent,
      String name,
      @Nullable Sampler sampler,
      List<Span> parentLinks,
      @Nullable Boolean recordEvents,
      @Nullable Kind kind,
      @Nullable TimestampConverter timestampConverter) {
    TraceParams activeTraceParams = options.traceConfig.getActiveTraceParams();
    Random random = options.randomHandler.current();
    TraceId traceId;
    SpanId spanId = SpanId.generateRandomId(random);
    SpanId parentSpanId = null;
    // TODO(bdrutu): Handle tracestate correctly not just propagate.
    Tracestate tracestate = TRACESTATE_DEFAULT;
    if (parent == null || !parent.isValid()) {
      // New root span.
      traceId = TraceId.generateRandomId(random);
      // This is a root span so no remote or local parent.
      hasRemoteParent = null;
    } else {
      // New child span.
      traceId = parent.getTraceId();
      parentSpanId = parent.getSpanId();
      tracestate = parent.getTracestate();
    }
    TraceOptions traceOptions =
        makeSamplingDecision(
                parent,
                hasRemoteParent,
                name,
                sampler,
                parentLinks,
                traceId,
                spanId,
                activeTraceParams)
            ? SAMPLED_TRACE_OPTIONS
            : NOT_SAMPLED_TRACE_OPTIONS;
    Span span =
        (traceOptions.isSampled() || Boolean.TRUE.equals(recordEvents))
            ? RecordEventsSpanImpl.startSpan(
                SpanContext.create(traceId, spanId, traceOptions, tracestate),
                name,
                kind,
                parentSpanId,
                hasRemoteParent,
                activeTraceParams,
                options.startEndHandler,
                timestampConverter,
                options.clock)
            : NoRecordEventsSpanImpl.create(
                SpanContext.create(traceId, spanId, traceOptions, tracestate));
    linkSpans(span, parentLinks);
    return span;
  }

  private static boolean makeSamplingDecision(
      @Nullable SpanContext parent,
      @Nullable Boolean hasRemoteParent,
      String name,
      @Nullable Sampler sampler,
      List<Span> parentLinks,
      TraceId traceId,
      SpanId spanId,
      TraceParams activeTraceParams) {
    // If users set a specific sampler in the SpanBuilder, use it.
    if (sampler != null) {
      return sampler.shouldSample(parent, hasRemoteParent, traceId, spanId, name, parentLinks);
    }
    // Use the default sampler if this is a root Span or this is an entry point Span (has remote
    // parent).
    if (Boolean.TRUE.equals(hasRemoteParent) || parent == null || !parent.isValid()) {
      return activeTraceParams
          .getSampler()
          .shouldSample(parent, hasRemoteParent, traceId, spanId, name, parentLinks);
    }
    // Parent is always different than null because otherwise we use the default sampler.
    return parent.getTraceOptions().isSampled() || isAnyParentLinkSampled(parentLinks);
  }

  private static boolean isAnyParentLinkSampled(List<Span> parentLinks) {
    for (Span parentLink : parentLinks) {
      if (parentLink.getContext().getTraceOptions().isSampled()) {
        return true;
      }
    }
    return false;
  }

  private static void linkSpans(Span span, List<Span> parentLinks) {
    if (!parentLinks.isEmpty()) {
      Link childLink = Link.fromSpanContext(span.getContext(), Type.CHILD_LINKED_SPAN);
      for (Span linkedSpan : parentLinks) {
        linkedSpan.addLink(childLink);
        span.addLink(Link.fromSpanContext(linkedSpan.getContext(), Type.PARENT_LINKED_SPAN));
      }
    }
  }

  static SpanBuilderImpl createWithParent(String spanName, @Nullable Span parent, Options options) {
    return new SpanBuilderImpl(spanName, null, parent, options);
  }

  static SpanBuilderImpl createWithRemoteParent(
      String spanName, @Nullable SpanContext remoteParentSpanContext, Options options) {
    return new SpanBuilderImpl(spanName, remoteParentSpanContext, null, options);
  }

  private SpanBuilderImpl(
      String name,
      @Nullable SpanContext remoteParentSpanContext,
      @Nullable Span parent,
      Options options) {
    this.name = checkNotNull(name, "name");
    this.parent = parent;
    this.remoteParentSpanContext = remoteParentSpanContext;
    this.options = options;
  }

  @Override
  public Span startSpan() {
    SpanContext parentContext = remoteParentSpanContext;
    Boolean hasRemoteParent = Boolean.TRUE;
    TimestampConverter timestampConverter = null;
    if (remoteParentSpanContext == null) {
      // This is not a child of a remote Span. Get the parent SpanContext from the parent Span if
      // any.
      Span parent = this.parent;
      hasRemoteParent = Boolean.FALSE;
      if (parent != null) {
        parentContext = parent.getContext();
        // Pass the timestamp converter from the parent to ensure that the recorded events are in
        // the right order. Implementation uses System.nanoTime() which is monotonically increasing.
        if (parent instanceof RecordEventsSpanImpl) {
          timestampConverter = ((RecordEventsSpanImpl) parent).getTimestampConverter();
        }
      } else {
        hasRemoteParent = null;
      }
    }
    return startSpanInternal(
        parentContext,
        hasRemoteParent,
        name,
        sampler,
        parentLinks,
        recordEvents,
        kind,
        timestampConverter);
  }

  static final class Options {
    private final RandomHandler randomHandler;
    private final RecordEventsSpanImpl.StartEndHandler startEndHandler;
    private final Clock clock;
    private final TraceConfig traceConfig;

    Options(
        RandomHandler randomHandler,
        RecordEventsSpanImpl.StartEndHandler startEndHandler,
        Clock clock,
        TraceConfig traceConfig) {
      this.randomHandler = checkNotNull(randomHandler, "randomHandler");
      this.startEndHandler = checkNotNull(startEndHandler, "startEndHandler");
      this.clock = checkNotNull(clock, "clock");
      this.traceConfig = checkNotNull(traceConfig, "traceConfig");
    }
  }

  @Override
  public SpanBuilderImpl setSampler(Sampler sampler) {
    this.sampler = checkNotNull(sampler, "sampler");
    return this;
  }

  @Override
  public SpanBuilderImpl setParentLinks(List<Span> parentLinks) {
    this.parentLinks = checkNotNull(parentLinks, "parentLinks");
    return this;
  }

  @Override
  public SpanBuilderImpl setRecordEvents(boolean recordEvents) {
    this.recordEvents = recordEvents;
    return this;
  }

  @Override
  public SpanBuilderImpl setSpanKind(@Nullable Kind kind) {
    this.kind = kind;
    return this;
  }
}
