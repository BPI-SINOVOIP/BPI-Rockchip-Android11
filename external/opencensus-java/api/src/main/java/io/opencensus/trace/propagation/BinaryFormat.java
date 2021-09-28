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

package io.opencensus.trace.propagation;

import io.opencensus.internal.Utils;
import io.opencensus.trace.SpanContext;
import java.text.ParseException;

/**
 * This is a helper class for {@link SpanContext} propagation on the wire using binary encoding.
 *
 * <p>Example of usage on the client:
 *
 * <pre>{@code
 * private static final Tracer tracer = Tracing.getTracer();
 * private static final BinaryFormat binaryFormat =
 *     Tracing.getPropagationComponent().getBinaryFormat();
 * void onSendRequest() {
 *   try (Scope ss = tracer.spanBuilder("Sent.MyRequest").startScopedSpan()) {
 *     byte[] binaryValue = binaryFormat.toByteArray(tracer.getCurrentContext().context());
 *     // Send the request including the binaryValue and wait for the response.
 *   }
 * }
 * }</pre>
 *
 * <p>Example of usage on the server:
 *
 * <pre>{@code
 * private static final Tracer tracer = Tracing.getTracer();
 * private static final BinaryFormat binaryFormat =
 *     Tracing.getPropagationComponent().getBinaryFormat();
 * void onRequestReceived() {
 *   // Get the binaryValue from the request.
 *   SpanContext spanContext = SpanContext.INVALID;
 *   try {
 *     if (binaryValue != null) {
 *       spanContext = binaryFormat.fromByteArray(binaryValue);
 *     }
 *   } catch (SpanContextParseException e) {
 *     // Maybe log the exception.
 *   }
 *   try (Scope ss =
 *            tracer.spanBuilderWithRemoteParent("Recv.MyRequest", spanContext).startScopedSpan()) {
 *     // Handle request and send response back.
 *   }
 * }
 * }</pre>
 *
 * @since 0.5
 */
public abstract class BinaryFormat {
  static final NoopBinaryFormat NOOP_BINARY_FORMAT = new NoopBinaryFormat();

  /**
   * Serializes a {@link SpanContext} into a byte array using the binary format.
   *
   * @deprecated use {@link #toByteArray(SpanContext)}.
   * @param spanContext the {@code SpanContext} to serialize.
   * @return the serialized binary value.
   * @throws NullPointerException if the {@code spanContext} is {@code null}.
   * @since 0.5
   */
  @Deprecated
  public byte[] toBinaryValue(SpanContext spanContext) {
    return toByteArray(spanContext);
  }

  /**
   * Serializes a {@link SpanContext} into a byte array using the binary format.
   *
   * @param spanContext the {@code SpanContext} to serialize.
   * @return the serialized binary value.
   * @throws NullPointerException if the {@code spanContext} is {@code null}.
   * @since 0.7
   */
  public byte[] toByteArray(SpanContext spanContext) {
    // Implementation must override this method.
    return toBinaryValue(spanContext);
  }

  /**
   * Parses the {@link SpanContext} from a byte array using the binary format.
   *
   * @deprecated use {@link #fromByteArray(byte[])}.
   * @param bytes a binary encoded buffer from which the {@code SpanContext} will be parsed.
   * @return the parsed {@code SpanContext}.
   * @throws NullPointerException if the {@code input} is {@code null}.
   * @throws ParseException if the version is not supported or the input is invalid
   * @since 0.5
   */
  @Deprecated
  public SpanContext fromBinaryValue(byte[] bytes) throws ParseException {
    try {
      return fromByteArray(bytes);
    } catch (SpanContextParseException e) {
      throw new ParseException(e.toString(), 0);
    }
  }

  /**
   * Parses the {@link SpanContext} from a byte array using the binary format.
   *
   * @param bytes a binary encoded buffer from which the {@code SpanContext} will be parsed.
   * @return the parsed {@code SpanContext}.
   * @throws NullPointerException if the {@code input} is {@code null}.
   * @throws SpanContextParseException if the version is not supported or the input is invalid
   * @since 0.7
   */
  public SpanContext fromByteArray(byte[] bytes) throws SpanContextParseException {
    // Implementation must override this method. If it doesn't, the below will StackOverflowError.
    try {
      return fromBinaryValue(bytes);
    } catch (ParseException e) {
      throw new SpanContextParseException("Error while parsing.", e);
    }
  }

  /**
   * Returns the no-op implementation of the {@code BinaryFormat}.
   *
   * @return the no-op implementation of the {@code BinaryFormat}.
   */
  static BinaryFormat getNoopBinaryFormat() {
    return NOOP_BINARY_FORMAT;
  }

  private static final class NoopBinaryFormat extends BinaryFormat {
    @Override
    public byte[] toByteArray(SpanContext spanContext) {
      Utils.checkNotNull(spanContext, "spanContext");
      return new byte[0];
    }

    @Override
    public SpanContext fromByteArray(byte[] bytes) {
      Utils.checkNotNull(bytes, "bytes");
      return SpanContext.INVALID;
    }

    private NoopBinaryFormat() {}
  }
}
