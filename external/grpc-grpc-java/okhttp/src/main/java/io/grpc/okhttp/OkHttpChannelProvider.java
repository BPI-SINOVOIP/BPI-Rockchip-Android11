/*
 * Copyright 2015 The gRPC Authors
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

package io.grpc.okhttp;

import io.grpc.Internal;
import io.grpc.InternalServiceProviders;
import io.grpc.ManagedChannelProvider;
import io.grpc.internal.GrpcUtil;

/**
 * Provider for {@link OkHttpChannelBuilder} instances.
 */
@Internal
public final class OkHttpChannelProvider extends ManagedChannelProvider {

  @Override
  public boolean isAvailable() {
    return true;
  }

  @Override
  public int priority() {
    return (GrpcUtil.IS_RESTRICTED_APPENGINE
        || InternalServiceProviders.isAndroid(getClass().getClassLoader())) ? 8 : 3;
  }

  @Override
  public OkHttpChannelBuilder builderForAddress(String name, int port) {
    return OkHttpChannelBuilder.forAddress(name, port);
  }

  @Override
  public OkHttpChannelBuilder builderForTarget(String target) {
    return OkHttpChannelBuilder.forTarget(target);
  }
}
