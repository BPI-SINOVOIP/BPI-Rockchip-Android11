/*
 * Copyright 2018 The gRPC Authors
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

package io.grpc.grpclb;

import io.grpc.Attributes;
import io.grpc.EquivalentAddressGroup;
import io.grpc.LoadBalancer.Helper;
import io.grpc.LoadBalancer.Subchannel;
import java.util.concurrent.ScheduledExecutorService;
import javax.annotation.concurrent.NotThreadSafe;

/**
 * Manages life-cycle of Subchannels for {@link GrpclbState}.
 *
 * <p>All methods are run from the ChannelExecutor that the helper uses.
 */
@NotThreadSafe
interface SubchannelPool {
  /**
   * Pass essential utilities.
   */
  void init(Helper helper, ScheduledExecutorService timerService);

  /**
   * Takes a {@link Subchannel} from the pool for the given {@code eag} if there is one available.
   * Otherwise, creates and returns a new {@code Subchannel} with the given {@code eag} and {@code
   * defaultAttributes}.
   */
  Subchannel takeOrCreateSubchannel(EquivalentAddressGroup eag, Attributes defaultAttributes);

  /**
   * Puts a {@link Subchannel} back to the pool.  From this point the Subchannel is owned by the
   * pool.
   */
  void returnSubchannel(Subchannel subchannel);

  /**
   * Shuts down all subchannels in the pool immediately.
   */
  void clear();
}
