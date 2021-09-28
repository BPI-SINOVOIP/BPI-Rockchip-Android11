//
// Copyright 2018 The Android Open Source Project
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
//

#ifndef __VTS_RESOURCE_VTSFMQDRIVER_H
#define __VTS_RESOURCE_VTSFMQDRIVER_H

#include <mutex>
#include <string>
#include <unordered_map>

#include <android-base/logging.h>
#include <fmq/MessageQueue.h>

using android::hardware::kSynchronizedReadWrite;
using android::hardware::kUnsynchronizedWrite;
using android::hardware::MessageQueue;
using android::hardware::MQDescriptorSync;
using android::hardware::MQDescriptorUnsync;

using namespace std;
using QueueId = int;

static constexpr const int kInvalidQueueId = -1;

namespace android {
namespace vts {

// struct to store queue information.
struct QueueInfo {
  // type of data in the queue.
  string queue_data_type;
  // flavor of the queue (sync or unsync).
  hardware::MQFlavor queue_flavor;
  // pointer to the actual queue object.
  shared_ptr<void> queue_object;
};

// A fast message queue class that manages all fast message queues created
// on the target side. Reader and writer use their id to read from and write
// into the queue.
// Example:
//   VtsFmqDriver manager;
//   // creates one reader and one writer.
//   QueueId writer_id =
//       manager.CreateFmq<uint16_t, kSynchronizedReadWrite>(2048, false);
//   QueueId reader_id =
//       manager.CreateFmq<uint16_t, kSynchronizedReadWrite>(writer_id);
//   // write some data
//   uint16_t write_data[5] {1, 2, 3, 4, 5};
//   manager.WriteFmq<uint16_t, kSynchronizedReadWrite>(
//       writer_id, write_data, 5);
//   // read the same data back
//   uint16_t read_data[5];
//   manager.ReadFmq<uint16_t, kSynchronizedReadWrite>(
//       reader_id, read_data, 5);
class VtsFmqDriver {
 public:
  // Constructor to initialize a Fast Message Queue (FMQ) manager.
  VtsFmqDriver() {}

  // Destructor to clean up the class.
  ~VtsFmqDriver() {
    // Clears objects in the map.
    fmq_map_.clear();
  }

  // Creates a brand new FMQ, i.e. the "first message queue object".
  //
  // @param data_type  type of data in the queue. This information is stored
  //                   in the driver to verify type later.
  // @param queue_size number of elements in the queue.
  // @param blocking   whether to enable blocking within the queue.
  //
  // @return message queue object id associated with the caller on success,
  //         kInvalidQueueId on failure.
  template <typename T, hardware::MQFlavor flavor>
  QueueId CreateFmq(const string& data_type, size_t queue_size, bool blocking);

  // Creates a new FMQ object based on an existing message queue
  // (Using queue_id assigned by fmq_driver.).
  //
  // @param data_type      type of data in the queue. This information is stored
  //                       in the driver to verify type later.
  // @param queue_id       identifies the message queue object.
  // @param reset_pointers whether to reset read and write pointers.
  //
  // @return message queue object id associated with the caller on success,
  //         kInvalidQueueId on failure.
  template <typename T, hardware::MQFlavor flavor>
  QueueId CreateFmq(const string& data_type, QueueId queue_id,
                    bool reset_pointers = true);

  // Creates a new FMQ object based on an existing message queue
  // (Using queue descriptor.).
  // This method will reset read/write pointers in the new queue object.
  //
  // @param data_type        type of data in the queue. This information is
  //                         stored in the driver to verify type later.
  // @param queue_descriptor pointer to descriptor of an existing queue.
  //
  // @return message queue object id associated with the caller on success,
  //         kInvalidQueueId on failure.
  template <typename T, hardware::MQFlavor flavor>
  QueueId CreateFmq(const string& data_type, size_t queue_descriptor);

  // Reads data_size items from FMQ (no blocking at all).
  //
  // @param data_type type of data in the queue. This information is verified
  //                  by the driver before calling the API on FMQ.
  // @param queue_id  identifies the message queue object.
  // @param data      pointer to the start of data to be filled.
  // @param data_size number of items to read.
  //
  // @return true if no error happens when reading from FMQ,
  //         false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool ReadFmq(const string& data_type, QueueId queue_id, T* data,
               size_t data_size);

  // Reads data_size items from FMQ, block if there is not enough data to
  // read.
  // This method can only be called if blocking=true on creation of the "first
  // message queue object" of the FMQ.
  //
  // @param data_type      type of data in the queue. This information is
  //                       verified by the driver before calling the API on FMQ.
  // @param queue_id       identifies the message queue object.
  // @param data           pointer to the start of data to be filled.
  // @param data_size      number of items to read.
  // @param time_out_nanos wait time when blocking.
  //
  // Returns: true if no error happens when reading from FMQ,
  //          false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool ReadFmqBlocking(const string& data_type, QueueId queue_id, T* data,
                       size_t data_size, int64_t time_out_nanos);

  // Reads data_size items from FMQ, possibly block on other queues.
  //
  // @param data_type          type of data in the queue. This information is
  //                           verified by the driver before calling the API
  //                           on FMQ.
  // @param queue_id           identifies the message queue object.
  // @param data               pointer to the start of data to be filled.
  // @param data_size          number of items to read.
  // @param read_notification  notification bits to set when finish reading.
  // @param write_notification notification bits to wait on when blocking.
  //                           Read will fail if this argument is 0.
  // @param time_out_nanos     wait time when blocking.
  // @param event_flag_word    event flag word shared by multiple queues.
  //
  // @return true if no error happens when reading from FMQ,
  //         false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool ReadFmqBlocking(const string& data_type, QueueId queue_id, T* data,
                       size_t data_size, uint32_t read_notification,
                       uint32_t write_notification, int64_t time_out_nanos,
                       atomic<uint32_t>* event_flag_word);

  // Writes data_size items to FMQ (no blocking at all).
  //
  // @param data_type type of data in the queue. This information is verified
  //                  by the driver before calling the API on FMQ.
  // @param queue_id  identifies the message queue object.
  // @param data      pointer to the start of data to be written.
  // @param data_size number of items to write.
  //
  // @return true if no error happens when writing to FMQ,
  //         false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool WriteFmq(const string& data_type, QueueId queue_id, T* data,
                size_t data_size);

  // Writes data_size items to FMQ, block if there is not enough space in
  // the queue.
  // This method can only be called if blocking=true on creation of the "first
  // message queue object" of the FMQ.
  //
  // @param data_type      type of data in the queue. This information is
  // verified
  //                       by the driver before calling the API on FMQ.
  // @param queue_id       identifies the message queue object.
  // @param data           pointer to the start of data to be written.
  // @param data_size      number of items to write.
  // @param time_out_nanos wait time when blocking.
  //
  // @returns true if no error happens when writing to FMQ,
  //          false otherwise
  template <typename T, hardware::MQFlavor flavor>
  bool WriteFmqBlocking(const string& data_type, QueueId queue_id, T* data,
                        size_t data_size, int64_t time_out_nanos);

  // Writes data_size items to FMQ, possibly block on other queues.
  //
  // @param data_type          type of data in the queue. This information is
  //                           verified by the driver before calling the API
  //                           on FMQ.
  // @param queue_id           identifies the message queue object.
  // @param data               pointer to the start of data to be written.
  // @param data_size          number of items to write.
  // @param read_notification  notification bits to wait on when blocking.
  //                           Write will fail if this argument is 0.
  // @param write_notification notification bits to set when finish writing.
  // @param time_out_nanos     wait time when blocking.
  // @param event_flag_word    event flag word shared by multiple queues.
  //
  // @return true if no error happens when writing to FMQ,
  //         false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool WriteFmqBlocking(const string& data_type, QueueId queue_id, T* data,
                        size_t data_size, uint32_t read_notification,
                        uint32_t write_notification, int64_t time_out_nanos,
                        atomic<uint32_t>* event_flag_word);

  // Gets space available to write in the queue.
  //
  // @param data_type type of data in the queue. This information is
  //                  verified by the driver before calling the API on FMQ.
  // @param queue_id  identifies the message queue object.
  // @param result    pointer to the result. Use pointer to store result because
  //                  the return value signals if the queue is found correctly.
  //
  // @return true if queue is found and type matches, and puts actual result in
  //              result pointer,
  //         false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool AvailableToWrite(const string& data_type, QueueId queue_id,
                        size_t* result);

  // Gets number of items available to read in the queue.
  //
  // @param data_type type of data in the queue. This information is
  //                  verified by the driver before calling the API on FMQ.
  // @param queue_id  identifies the message queue object.
  // @param result    pointer to the result. Use pointer to store result because
  //                  the return value signals if the queue is found correctly.
  //
  // @return true if queue is found and type matches, and puts actual result in
  //              result pointer,
  //         false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool AvailableToRead(const string& data_type, QueueId queue_id,
                       size_t* result);

  // Gets size of item in the queue.
  //
  // @param data_type type of data in the queue. This information is
  //                  verified by the driver before calling the API on FMQ.
  // @param queue_id  identifies the message queue object.
  // @param result    pointer to the result. Use pointer to store result because
  //                  the return value signals if the queue is found correctly.
  //
  // @return true if queue is found and type matches, and puts actual result in
  //              result pointer,
  //         false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool GetQuantumSize(const string& data_type, QueueId queue_id,
                      size_t* result);

  // Gets number of items that fit in the queue.
  //
  // @param data_type type of data in the queue. This information is
  //                  verified by the driver before calling the API on FMQ.
  // @param queue_id  identifies the message queue object.
  // @param result    pointer to the result. Use pointer to store result because
  //                  the return value signals if the queue is found correctly.
  //
  // @return true if queue is found and type matches, and puts actual result in
  //              result pointer,
  //         false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool GetQuantumCount(const string& data_type, QueueId queue_id,
                       size_t* result);

  // Checks if the queue associated with queue_id is valid.
  //
  // @param data_type type of data in the queue. This information is
  //                  verified by the driver before calling the API on FMQ.
  // @param queue_id  identifies the message queue object.
  //
  // @return true if the queue object is valid, false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool IsValid(const string& data_type, QueueId queue_id);

  // Gets event flag word of the queue, which allows multiple queues
  // to communicate (i.e. blocking).
  // The returned event flag word can be passed into readBlocking() and
  // writeBlocking() to achieve blocking among multiple queues.
  //
  // @param data_type type of data in the queue. This information is
  //                  verified by the driver before calling the API on FMQ.
  // @param queue_id  identifies the message queue object.
  // @param result    pointer to the result. Use pointer to store result because
  //                  the return value signals if the queue is found correctly.
  //
  // @return true if queue is found and type matches, and puts actual result in
  //              result pointer,
  //         false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool GetEventFlagWord(const string& data_type, QueueId queue_id,
                        atomic<uint32_t>** result);

  // Gets the address of queue descriptor in memory. This function is called by
  // driver_manager to preprocess arguments that are FMQs.
  //
  // @param data_type type of data in the queue. This information is
  //                  verified by the driver before calling the API on FMQ.
  // @param queue_id  identifies the message queue object.
  // @param result    pointer to store result.
  //
  // @return true if queue is found, and type matches, and puts actual result in
  //              result pointer,
  //         false otherwise.
  template <typename T, hardware::MQFlavor flavor>
  bool GetQueueDescAddress(const string& data_type, QueueId queue_id,
                           size_t* result);

 private:
  // Finds the queue in the map based on the input queue ID.
  //
  // @param data_type type of data in the queue. This function verifies this
  //                  information.
  // @param queue_id  identifies the message queue object.
  //
  // @return the pointer to message queue object,
  //         nullptr if queue ID is invalid or queue type is misspecified.
  template <typename T, hardware::MQFlavor flavor>
  MessageQueue<T, flavor>* FindQueue(const string& data_type, QueueId queue_id);

  // Inserts a FMQ object into the map, along with its type of data and queue
  // flavor. This function ensures only one thread is inserting queue into the
  // map at once.
  //
  // @param data_type    type of data in the queue. This information is stored
  //                     in the driver.
  // @param queue_object a shared pointer to MessageQueue.
  //
  // @return id associated with the queue.
  template <typename T, hardware::MQFlavor flavor>
  QueueId InsertQueue(const string& data_type,
                      shared_ptr<MessageQueue<T, flavor>> queue_object);

  // a hashtable to keep track of all ongoing FMQ's.
  // The key of the hashtable is the queue ID.
  // The value of the hashtable is a smart pointer to message queue object
  // information struct associated with the queue ID.
  unordered_map<QueueId, unique_ptr<QueueInfo>> fmq_map_;

  // a mutex to ensure mutual exclusion of operations on fmq_map_
  mutex map_mutex_;
};

// Implementations follow, because all the methods are template methods.
template <typename T, hardware::MQFlavor flavor>
QueueId VtsFmqDriver::CreateFmq(const string& data_type, size_t queue_size,
                                bool blocking) {
  shared_ptr<MessageQueue<T, flavor>> new_queue(
      new (std::nothrow) MessageQueue<T, flavor>(queue_size, blocking));
  return InsertQueue<T, flavor>(data_type, new_queue);
}

template <typename T, hardware::MQFlavor flavor>
QueueId VtsFmqDriver::CreateFmq(const string& data_type, QueueId queue_id,
                                bool reset_pointers) {
  MessageQueue<T, flavor>* queue_object =
      FindQueue<T, flavor>(data_type, queue_id);
  const hardware::MQDescriptor<T, flavor>* descriptor = queue_object->getDesc();
  if (descriptor == nullptr) {
    LOG(ERROR) << "FMQ Driver: cannot find descriptor for the specified "
               << "Fast Message Queue with ID " << queue_id << ".";
    return kInvalidQueueId;
  }

  shared_ptr<MessageQueue<T, flavor>> new_queue(
      new (std::nothrow) MessageQueue<T, flavor>(*descriptor, reset_pointers));
  return InsertQueue<T, flavor>(data_type, new_queue);
}

template <typename T, hardware::MQFlavor flavor>
QueueId VtsFmqDriver::CreateFmq(const string& data_type,
                                size_t queue_desc_addr) {
  // Cast the address back to a descriptor object.
  hardware::MQDescriptor<T, flavor>* descriptor_addr =
      reinterpret_cast<hardware::MQDescriptor<T, flavor>*>(queue_desc_addr);
  shared_ptr<MessageQueue<T, flavor>> new_queue(
      new (std::nothrow) MessageQueue<T, flavor>(*descriptor_addr));
  // Need to manually delete this pointer because HAL driver allocates it.
  delete descriptor_addr;

  return InsertQueue<T, flavor>(data_type, new_queue);
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::ReadFmq(const string& data_type, QueueId queue_id, T* data,
                           size_t data_size) {
  MessageQueue<T, flavor>* queue_object =
      FindQueue<T, flavor>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  return queue_object->read(data, data_size);
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::ReadFmqBlocking(const string& data_type, QueueId queue_id,
                                   T* data, size_t data_size,
                                   int64_t time_out_nanos) {
  if (flavor == kUnsynchronizedWrite) {
    LOG(ERROR) << "FMQ Driver: blocking read is not allowed in "
               << "unsynchronized queue.";
    return false;
  }

  MessageQueue<T, kSynchronizedReadWrite>* queue_object =
      FindQueue<T, kSynchronizedReadWrite>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  return queue_object->readBlocking(data, data_size, time_out_nanos);
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::ReadFmqBlocking(const string& data_type, QueueId queue_id,
                                   T* data, size_t data_size,
                                   uint32_t read_notification,
                                   uint32_t write_notification,
                                   int64_t time_out_nanos,
                                   atomic<uint32_t>* event_flag_word) {
  if (flavor == kUnsynchronizedWrite) {
    LOG(ERROR) << "FMQ Driver: blocking read is not allowed in "
               << "unsynchronized queue.";
    return false;
  }

  hardware::EventFlag* ef_group = nullptr;
  status_t status;
  // create an event flag out of the event flag word
  status = hardware::EventFlag::createEventFlag(event_flag_word, &ef_group);
  if (status != NO_ERROR) {  // check status
    LOG(ERROR) << "FMQ Driver: cannot create event flag with the specified "
               << "event flag word.";
    return false;
  }

  MessageQueue<T, kSynchronizedReadWrite>* queue_object =
      FindQueue<T, kSynchronizedReadWrite>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  return queue_object->readBlocking(data, data_size, read_notification,
                                    write_notification, time_out_nanos,
                                    ef_group);
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::WriteFmq(const string& data_type, QueueId queue_id, T* data,
                            size_t data_size) {
  MessageQueue<T, flavor>* queue_object =
      FindQueue<T, flavor>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  return queue_object->write(data, data_size);
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::WriteFmqBlocking(const string& data_type, QueueId queue_id,
                                    T* data, size_t data_size,
                                    int64_t time_out_nanos) {
  if (flavor == kUnsynchronizedWrite) {
    LOG(ERROR) << "FMQ Driver: blocking write is not allowed in "
               << "unsynchronized queue.";
    return false;
  }

  MessageQueue<T, kSynchronizedReadWrite>* queue_object =
      FindQueue<T, kSynchronizedReadWrite>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  return queue_object->writeBlocking(data, data_size, time_out_nanos);
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::WriteFmqBlocking(const string& data_type, QueueId queue_id,
                                    T* data, size_t data_size,
                                    uint32_t read_notification,
                                    uint32_t write_notification,
                                    int64_t time_out_nanos,
                                    atomic<uint32_t>* event_flag_word) {
  if (flavor == kUnsynchronizedWrite) {
    LOG(ERROR) << "FMQ Driver: blocking write is not allowed in "
               << "unsynchronized queue.";
    return false;
  }

  hardware::EventFlag* ef_group = nullptr;
  status_t status;
  // create an event flag out of the event flag word
  status = hardware::EventFlag::createEventFlag(event_flag_word, &ef_group);
  if (status != NO_ERROR) {  // check status
    LOG(ERROR) << "FMQ Driver: cannot create event flag with the specified "
               << "event flag word.";
    return false;
  }

  MessageQueue<T, kSynchronizedReadWrite>* queue_object =
      FindQueue<T, kSynchronizedReadWrite>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  return queue_object->writeBlocking(data, data_size, read_notification,
                                     write_notification, time_out_nanos,
                                     ef_group);
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::AvailableToWrite(const string& data_type, QueueId queue_id,
                                    size_t* result) {
  MessageQueue<T, flavor>* queue_object =
      FindQueue<T, flavor>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  *result = queue_object->availableToWrite();
  return true;
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::AvailableToRead(const string& data_type, QueueId queue_id,
                                   size_t* result) {
  MessageQueue<T, flavor>* queue_object =
      FindQueue<T, flavor>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  *result = queue_object->availableToRead();
  return true;
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::GetQuantumSize(const string& data_type, QueueId queue_id,
                                  size_t* result) {
  MessageQueue<T, flavor>* queue_object =
      FindQueue<T, flavor>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  *result = queue_object->getQuantumSize();
  return true;
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::GetQuantumCount(const string& data_type, QueueId queue_id,
                                   size_t* result) {
  MessageQueue<T, flavor>* queue_object =
      FindQueue<T, flavor>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  *result = queue_object->getQuantumCount();
  return true;
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::IsValid(const string& data_type, QueueId queue_id) {
  MessageQueue<T, flavor>* queue_object =
      FindQueue<T, flavor>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  return queue_object->isValid();
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::GetEventFlagWord(const string& data_type, QueueId queue_id,
                                    atomic<uint32_t>** result) {
  MessageQueue<T, flavor>* queue_object =
      FindQueue<T, flavor>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  *result = queue_object->getEventFlagWord();
  return true;
}

template <typename T, hardware::MQFlavor flavor>
bool VtsFmqDriver::GetQueueDescAddress(const string& data_type,
                                       QueueId queue_id, size_t* result) {
  MessageQueue<T, flavor>* queue_object =
      FindQueue<T, flavor>(data_type, queue_id);
  if (queue_object == nullptr) return false;
  *result = reinterpret_cast<size_t>(queue_object->getDesc());
  return true;
}

template <typename T, hardware::MQFlavor flavor>
MessageQueue<T, flavor>* VtsFmqDriver::FindQueue(const string& data_type,
                                                 QueueId queue_id) {
  map_mutex_.lock();  // Ensure mutual exclusion.
  auto iterator = fmq_map_.find(queue_id);
  if (iterator == fmq_map_.end()) {  // queue not found
    LOG(ERROR) << "FMQ Driver: cannot find Fast Message Queue with ID "
               << queue_id;
    return nullptr;
  }

  QueueInfo* queue_info = (iterator->second).get();
  if (queue_info->queue_data_type != data_type) {  // queue data type incorrect
    LOG(ERROR) << "FMQ Driver: caller specified data type " << data_type
               << " doesn't match with the data type "
               << queue_info->queue_data_type << " stored in driver.";
    return nullptr;
  }

  if (queue_info->queue_flavor != flavor) {  // queue flavor incorrect
    LOG(ERROR) << "FMQ Driver: caller specified flavor " << flavor
               << "doesn't match with the stored queue flavor "
               << queue_info->queue_flavor << ".";
    return nullptr;
  }

  // type check passes, extract queue from the struct
  shared_ptr<MessageQueue<T, flavor>> queue_object =
      static_pointer_cast<MessageQueue<T, flavor>>(queue_info->queue_object);
  MessageQueue<T, flavor>* result = queue_object.get();
  map_mutex_.unlock();
  return result;
}

template <typename T, hardware::MQFlavor flavor>
QueueId VtsFmqDriver::InsertQueue(
    const string& data_type, shared_ptr<MessageQueue<T, flavor>> queue_object) {
  if (queue_object == nullptr) {
    LOG(ERROR) << "FMQ Driver Error: Failed to create a FMQ "
               << "using FMQ constructor.";
    return kInvalidQueueId;
  }
  // Create a struct to store queue object, type of data, and
  // queue flavor.
  unique_ptr<QueueInfo> new_queue_info(new QueueInfo{
      string(data_type), flavor, static_pointer_cast<void>(queue_object)});
  map_mutex_.lock();
  size_t new_queue_id = fmq_map_.size();
  fmq_map_.emplace(new_queue_id, move(new_queue_info));
  map_mutex_.unlock();
  return new_queue_id;
}

}  // namespace vts
}  // namespace android
#endif  //__VTS_RESOURCE_VTSFMQDRIVER_H
