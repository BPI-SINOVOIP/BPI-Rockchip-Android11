// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IORAP_SRC_PERFETTO_RX_PRODUCER_H_
#define IORAP_SRC_PERFETTO_RX_PRODUCER_H_

#include "perfetto/perfetto_consumer.h"       // libiorap

#include <perfetto/config/trace_config.pb.h>  // libperfetto
#include <rxcpp/rx.hpp>

#include <iosfwd>
#include <functional>
#include <optional>
#include <vector>

namespace iorap::perfetto {

struct PerfettoDependencies {
  using Component =
      fruit::Component<PerfettoConsumer, ::perfetto::protos::TraceConfig>;
  using Injector =
      fruit::Injector<PerfettoConsumer, ::perfetto::protos::TraceConfig>;
  using NormalizedComponent =
      fruit::NormalizedComponent<PerfettoConsumer, ::perfetto::protos::TraceConfig>;

  // Create a 'live' component that will talk to perfetto via traced.
  static Component CreateComponent(/*TODO: config params*/);

  // Create perfetto.protos.TraceConfig , serialized as a (machine-readable) string.
  //
  // The following ftrace events are enabled:
  // * mm_filemap_add_to_page_cache
  // * mm_filemap_delete_from_page_cache
  //
  // If deferred starting is also enabled, no tracing will begin until
  // ::perfetto::consumer::StartTracing is invoked.
  static ::perfetto::protos::TraceConfig CreateConfig(uint32_t duration_ms,
                                                      bool deferred_start = true,
                                                      uint32_t buffer_size = 4096);
};

namespace detail {
  template <typename T>
  struct concept_message_lite_base {
    static_assert(std::is_base_of_v<::google::protobuf::MessageLite, T>,
                  "T must inherit from MessageLite");
    using type = T;
  };

  template <typename T>
  using concept_message_lite_base_t = typename concept_message_lite_base<T>::type;
}  // namespace detail

/*
 * In Android's version of libprotobuf, move-constructors are not generated.
 * This results in a legitimate (~10sec per TracePacket being compiled) slowdown,
 * so we need to avoid it everywhere.
 *
 * 1) Don't copy the protos, move them instead.
 * 2) Use 'shared_ptr' because rxcpp won't compile with unique_ptr.
 */
template <typename T>
using ProtobufPtr = std::shared_ptr<detail::concept_message_lite_base_t<const T>>;

template <typename T>
using ProtobufMutablePtr = std::shared_ptr<detail::concept_message_lite_base_t<T>>;

// This acts as a lightweight type marker so that we know what data has actually
// encoded under the hood.
template <typename T>
struct BinaryWireProtobuf {
  static_assert(std::is_base_of_v<::google::protobuf::MessageLite, T>,
                "T should be a base class of MessageLite");

  std::vector<std::byte>& data() {
    return data_;
  }

  const std::vector<std::byte>& data() const {
    return data_;
  }

  size_t size() const {
    return data_.size();
  }

  explicit BinaryWireProtobuf(char* data, size_t size)
    : BinaryWireProtobuf(reinterpret_cast<std::byte*>(data), size) {
  }

  explicit BinaryWireProtobuf(std::byte* data, size_t size) {
    data_.resize(size);
    std::copy(data,
              data + size,
              data_.data());
  }

  explicit BinaryWireProtobuf(std::vector<std::byte> data) : data_{std::move(data)} {
  }

  // You wouldn't want to accidentally copy a giant multi-megabyte chunk would you?
  // BinaryWireProtobuf(const BinaryWireProtobuf& other) = delete;  // FIXME: rx likes to copy.
  BinaryWireProtobuf(const BinaryWireProtobuf& other) = default;
  BinaryWireProtobuf(BinaryWireProtobuf&& other) = default;

  // Important: Deserialization could fail, for example data is truncated or
  // some minor disc corruption occurred.
  template <typename U>
  std::optional<ProtobufPtr<U>> MaybeUnserialize() {
    ProtobufMutablePtr<U> unencoded{new U{}};

    if (!unencoded->ParseFromArray(data_.data(), data_.size())) {
      return std::nullopt;
    }

    return {std::move(unencoded)};
  }

  bool WriteFullyToFile(const std::string& path,
                        bool follow_symlinks = false) const;

  static std::optional<BinaryWireProtobuf<T>> ReadFullyFromFile(const std::string& path,
                                                                bool follow_symlinks = false);

  bool operator==(const BinaryWireProtobuf<T>& other) const;
  bool operator!=(const BinaryWireProtobuf<T>& other) const {
    return !(*this == other);
  }

 private:
  static bool CleanUpAfterFailedWrite(const std::string& path);
  bool WriteStringToFd(int fd) const;

  static bool ReadFdToString(int fd, /*out*/std::vector<std::byte>* data);

  std::vector<std::byte> data_;
};

//using PerfettoTraceProto = BinaryWireProtobuf<::perfetto::protos::Trace>;
using PerfettoTraceProto = BinaryWireProtobuf<::google::protobuf::MessageLite>;

enum class PerfettoStreamCommand {
  kStartTracing, // -> () | on_error
  kStopTracing,  // -> on_next(PerfettoTraceProto) | on_error
  kShutdown,     // -> on_completed | on_error
  // XX: should shutdown be converted to use the rx suscriber#unsubscribe instead?
};

std::ostream& operator<<(std::ostream& os, PerfettoStreamCommand c);

struct RxProducerFactory {
  // Passing anything by value leads to a lot of pain and headache.
  // Pass in the injector by reference because nothing else seems to work.
  explicit RxProducerFactory(PerfettoDependencies::Injector& injector);

  // Create a one-shot perfetto observable that will begin
  // asynchronously producing a PerfettoTraceProto after the 'kStartTracing'
  // command is observed.
  //
  // libperfetto is immediately primed (i.e. connected in a deferred state)
  // upon calling this function, to reduce the latency of 'kStartTracing'.
  //
  // To finish the trace, push 'kStopTracing'. To cancel or tear down at any
  // time, push 'kShutdown'.
  //
  // The TraceProto may come out at any time after 'kStartTracing',
  // this is controlled by duration_ms in the TraceConfig.
  //
  // TODO: libperfetto should actually stop tracing when we ask it to,
  // instead of using a hardcoded time.
  //
  // The observable may go into #on_error at any time, if the underlying
  // libperfetto states transition to a failing state.
  // This usually means the OS is not configured correctly.
  rxcpp::observable<PerfettoTraceProto> CreateTraceStream(
      rxcpp::observable<PerfettoStreamCommand> commands);

  // TODO: is this refactor-able into a subscriber factory that takes
  // the commands-observable as a parameter?

  // TODO: infinite perfetto stream.

 private:
  // XX: why doesn't this just let me pass in a regular Component?
  PerfettoDependencies::Injector& injector_;

  friend void CollectPerfettoTraceBufferImmediately(
      RxProducerFactory& producer_factory,
      const std::string& arg_output_proto);
};

// An rx Coordination, which will cause a new thread to spawn for each new Worker.
//
// Idle-class priority is set for the CPU and IO priorities on the new thread.
//
// TODO: move to separate file
rxcpp::observe_on_one_worker ObserveOnNewIoThread();

}  // namespace iorap::perfetto
#endif  // IORAP_SRC_PERFETTO_RX_PRODUCER_H_
