/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "ETMDecoder.h"

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <llvm/Support/MemoryBuffer.h>
#include <opencsd.h>

using namespace simpleperf;

namespace {

class DecoderLogStr : public ocsdMsgLogStrOutI {
 public:
  void printOutStr(const std::string& out_str) override { LOG(INFO) << out_str; }
};

class DecodeErrorLogger : public ocsdDefaultErrorLogger {
 public:
  DecodeErrorLogger(const std::function<void(const ocsdError&)>& error_callback)
      : error_callback_(error_callback) {
    initErrorLogger(OCSD_ERR_SEV_INFO, false);
    msg_logger_.setLogOpts(ocsdMsgLogger::OUT_STR_CB);
    msg_logger_.setStrOutFn(&log_str_);
    setOutputLogger(&msg_logger_);
  }

  void LogError(const ocsd_hndl_err_log_t handle, const ocsdError* error) override {
    ocsdDefaultErrorLogger::LogError(handle, error);
    if (error != nullptr) {
      error_callback_(*error);
    }
  }

 private:
  std::function<void(const ocsdError&)> error_callback_;
  DecoderLogStr log_str_;
  ocsdMsgLogger msg_logger_;
};

static bool IsRespError(ocsd_datapath_resp_t resp) { return resp >= OCSD_RESP_ERR_CONT; }

// Used instead of DecodeTree in OpenCSD to avoid linking decoders not for ETMV4 instruction tracing
// in OpenCSD.
class ETMV4IDecodeTree {
 public:
  ETMV4IDecodeTree()
      : error_logger_(std::bind(&ETMV4IDecodeTree::ProcessError, this, std::placeholders::_1)) {
    frame_decoder_.Configure(OCSD_DFRMTR_FRAME_MEM_ALIGN);
    frame_decoder_.getErrLogAttachPt()->attach(&error_logger_);
  }

  bool CreateDecoder(const EtmV4Config& config) {
    uint8_t trace_id = config.getTraceID();
    auto packet_decoder = std::make_unique<TrcPktProcEtmV4I>(trace_id);
    packet_decoder->setProtocolConfig(&config);
    packet_decoder->getErrorLogAttachPt()->replace_first(&error_logger_);
    frame_decoder_.getIDStreamAttachPt(trace_id)->attach(packet_decoder.get());
    auto result = packet_decoders_.emplace(trace_id, packet_decoder.release());
    if (!result.second) {
      LOG(ERROR) << "trace id " << trace_id << " has been used";
    }
    return result.second;
  }

  void AttachPacketSink(uint8_t trace_id, IPktDataIn<EtmV4ITrcPacket>& packet_sink) {
    auto& packet_decoder = packet_decoders_[trace_id];
    CHECK(packet_decoder);
    packet_decoder->getPacketOutAttachPt()->replace_first(&packet_sink);
  }

  void AttachPacketMonitor(uint8_t trace_id, IPktRawDataMon<EtmV4ITrcPacket>& packet_monitor) {
    auto& packet_decoder = packet_decoders_[trace_id];
    CHECK(packet_decoder);
    packet_decoder->getRawPacketMonAttachPt()->replace_first(&packet_monitor);
  }

  void AttachRawFramePrinter(RawFramePrinter& frame_printer) {
    frame_decoder_.Configure(frame_decoder_.getConfigFlags() | OCSD_DFRMTR_PACKED_RAW_OUT);
    frame_decoder_.getTrcRawFrameAttachPt()->replace_first(&frame_printer);
  }

  ITrcDataIn& GetDataIn() { return frame_decoder_; }

  void ProcessError(const ocsdError& error) {
    if (error.getErrorCode() == OCSD_ERR_INVALID_PCKT_HDR) {
      // Found an invalid packet header, following packets for this trace id may also be invalid.
      // So reset the decoder to find I_ASYNC packet in the data stream.
      if (auto it = packet_decoders_.find(error.getErrorChanID()); it != packet_decoders_.end()) {
        auto& packet_decoder = it->second;
        CHECK(packet_decoder);
        packet_decoder->TraceDataIn(OCSD_OP_RESET, error.getErrorIndex(), 0, nullptr, nullptr);
      }
    }
  }

  DecodeErrorLogger& ErrorLogger() { return error_logger_; }

 private:
  DecodeErrorLogger error_logger_;
  TraceFormatterFrameDecoder frame_decoder_;
  std::unordered_map<uint8_t, std::unique_ptr<TrcPktProcEtmV4I>> packet_decoders_;
};

// Similar to IPktDataIn<EtmV4ITrcPacket>, but add trace id.
struct PacketCallback {
  // packet callbacks are called in priority order.
  enum Priority {
    MAP_LOCATOR,
    PACKET_TO_ELEMENT,
  };

  PacketCallback(Priority prio) : priority(prio) {}
  virtual ~PacketCallback() {}
  virtual ocsd_datapath_resp_t ProcessPacket(uint8_t trace_id, ocsd_datapath_op_t op,
                                             ocsd_trc_index_t index_sop,
                                             const EtmV4ITrcPacket* pkt) = 0;
  const Priority priority;
};

// Receives packets from a packet decoder in OpenCSD library.
class PacketSink : public IPktDataIn<EtmV4ITrcPacket> {
 public:
  PacketSink(uint8_t trace_id) : trace_id_(trace_id) {}

  void AddCallback(PacketCallback* callback) {
    auto it = std::lower_bound(callbacks_.begin(), callbacks_.end(), callback,
                               [](const PacketCallback* c1, const PacketCallback* c2) {
                                 return c1->priority < c2->priority;
                               });
    callbacks_.insert(it, callback);
  }

  ocsd_datapath_resp_t PacketDataIn(ocsd_datapath_op_t op, ocsd_trc_index_t index_sop,
                                    const EtmV4ITrcPacket* pkt) override {
    for (auto& callback : callbacks_) {
      auto resp = callback->ProcessPacket(trace_id_, op, index_sop, pkt);
      if (IsRespError(resp)) {
        return resp;
      }
    }
    return OCSD_RESP_CONT;
  }

 private:
  uint8_t trace_id_;
  std::vector<PacketCallback*> callbacks_;
};

// For each trace_id, when given an addr, find the thread and map it belongs to.
class MapLocator : public PacketCallback {
 public:
  MapLocator(ThreadTree& thread_tree)
      : PacketCallback(PacketCallback::MAP_LOCATOR), thread_tree_(thread_tree) {}

  ThreadTree& GetThreadTree() { return thread_tree_; }

  ocsd_datapath_resp_t ProcessPacket(uint8_t trace_id, ocsd_datapath_op_t op,
                                     ocsd_trc_index_t index_sop,
                                     const EtmV4ITrcPacket* pkt) override {
    TraceData& data = trace_data_[trace_id];
    if (op == OCSD_OP_DATA) {
      if (pkt != nullptr && pkt->getContext().updated_c) {
        int32_t new_tid = static_cast<int32_t>(pkt->getContext().ctxtID);
        if (data.tid != new_tid) {
          data.tid = new_tid;
          data.thread = nullptr;
          data.userspace_map = nullptr;
        }
      }
    } else if (op == OCSD_OP_RESET) {
      data.tid = -1;
      data.thread = nullptr;
      data.userspace_map = nullptr;
    }
    return OCSD_RESP_CONT;
  }

  const MapEntry* FindMap(uint8_t trace_id, uint64_t addr) {
    TraceData& data = trace_data_[trace_id];
    if (data.userspace_map != nullptr && data.userspace_map->Contains(addr)) {
      return data.userspace_map;
    }
    if (data.tid == -1) {
      return nullptr;
    }
    if (data.thread == nullptr) {
      data.thread = thread_tree_.FindThread(data.tid);
      if (data.thread == nullptr) {
        return nullptr;
      }
    }
    data.userspace_map = data.thread->maps->FindMapByAddr(addr);
    if (data.userspace_map != nullptr) {
      return data.userspace_map;
    }
    // We don't cache kernel map. Because kernel map can start from 0 and overlap all userspace
    // maps.
    return thread_tree_.GetKernelMaps().FindMapByAddr(addr);
  }

 private:
  struct TraceData {
    int32_t tid = -1;  // thread id, -1 if invalid
    const ThreadEntry* thread = nullptr;
    const MapEntry* userspace_map = nullptr;
  };

  ThreadTree& thread_tree_;
  TraceData trace_data_[256];
};

// Map (trace_id, ip address) to (binary_path, binary_offset), and read binary files.
class MemAccess : public ITargetMemAccess {
 public:
  MemAccess(MapLocator& map_locator) : map_locator_(map_locator) {}

  ocsd_err_t ReadTargetMemory(const ocsd_vaddr_t address, uint8_t trace_id, ocsd_mem_space_acc_t,
                              uint32_t* num_bytes, uint8_t* p_buffer) override {
    TraceData& data = trace_data_[trace_id];
    const MapEntry* map = map_locator_.FindMap(trace_id, address);
    // fast path
    if (map != nullptr && map == data.buffer_map && address >= data.buffer_start &&
        address + *num_bytes <= data.buffer_end) {
      if (data.buffer == nullptr) {
        *num_bytes = 0;
      } else {
        memcpy(p_buffer, data.buffer + (address - data.buffer_start), *num_bytes);
      }
      return OCSD_OK;
    }

    // slow path
    size_t copy_size = 0;
    if (map != nullptr) {
      llvm::MemoryBuffer* memory = GetMemoryBuffer(map->dso);
      if (memory != nullptr) {
        uint64_t offset = address - map->start_addr + map->pgoff;
        size_t file_size = memory->getBufferSize();
        copy_size = file_size > offset ? std::min<size_t>(file_size - offset, *num_bytes) : 0;
        if (copy_size > 0) {
          memcpy(p_buffer, memory->getBufferStart() + offset, copy_size);
        }
      }
      // Update the last buffer cache.
      data.buffer_map = map;
      data.buffer = memory == nullptr ? nullptr : (memory->getBufferStart() + map->pgoff);
      data.buffer_start = map->start_addr;
      data.buffer_end = map->get_end_addr();
    }
    *num_bytes = copy_size;
    return OCSD_OK;
  }

 private:
  llvm::MemoryBuffer* GetMemoryBuffer(Dso* dso) {
    auto it = elf_map_.find(dso);
    if (it == elf_map_.end()) {
      ElfStatus status;
      auto res = elf_map_.emplace(dso, ElfFile::Open(dso->GetDebugFilePath(), &status));
      it = res.first;
    }
    return it->second ? it->second->GetMemoryBuffer() : nullptr;
  }

  struct TraceData {
    const MapEntry* buffer_map = nullptr;
    const char* buffer = nullptr;
    uint64_t buffer_start = 0;
    uint64_t buffer_end = 0;
  };

  MapLocator& map_locator_;
  std::unordered_map<Dso*, std::unique_ptr<ElfFile>> elf_map_;
  TraceData trace_data_[256];
};

class InstructionDecoder : public TrcIDecode {
 public:
  ocsd_err_t DecodeInstruction(ocsd_instr_info* instr_info) {
    this->instr_info = instr_info;
    return TrcIDecode::DecodeInstruction(instr_info);
  }

  ocsd_instr_info* instr_info;
};

// Similar to ITrcGenElemIn, but add next instruction info, which is needed to get branch to addr
// for an InstructionRange element.
struct ElementCallback {
 public:
  virtual ~ElementCallback(){};
  virtual ocsd_datapath_resp_t ProcessElement(ocsd_trc_index_t index_sop, uint8_t trace_id,
                                              const OcsdTraceElement& elem,
                                              const ocsd_instr_info* next_instr) = 0;
};

// Decode packets into elements.
class PacketToElement : public PacketCallback, public ITrcGenElemIn {
 public:
  PacketToElement(MapLocator& map_locator, const std::unordered_map<uint8_t, EtmV4Config>& configs,
                  DecodeErrorLogger& error_logger)
      : PacketCallback(PacketCallback::PACKET_TO_ELEMENT), mem_access_(map_locator) {
    for (auto& p : configs) {
      uint8_t trace_id = p.first;
      const EtmV4Config& config = p.second;
      element_decoders_.emplace(trace_id, trace_id);
      auto& decoder = element_decoders_[trace_id];
      decoder.setProtocolConfig(&config);
      decoder.getErrorLogAttachPt()->replace_first(&error_logger);
      decoder.getInstrDecodeAttachPt()->replace_first(&instruction_decoder_);
      decoder.getMemoryAccessAttachPt()->replace_first(&mem_access_);
      decoder.getTraceElemOutAttachPt()->replace_first(this);
    }
  }

  void AddCallback(ElementCallback* callback) { callbacks_.push_back(callback); }

  ocsd_datapath_resp_t ProcessPacket(uint8_t trace_id, ocsd_datapath_op_t op,
                                     ocsd_trc_index_t index_sop,
                                     const EtmV4ITrcPacket* pkt) override {
    return element_decoders_[trace_id].PacketDataIn(op, index_sop, pkt);
  }

  ocsd_datapath_resp_t TraceElemIn(const ocsd_trc_index_t index_sop, uint8_t trc_chan_id,
                                   const OcsdTraceElement& elem) override {
    for (auto& callback : callbacks_) {
      auto resp =
          callback->ProcessElement(index_sop, trc_chan_id, elem, instruction_decoder_.instr_info);
      if (IsRespError(resp)) {
        return resp;
      }
    }
    return OCSD_RESP_CONT;
  }

 private:
  // map from trace id of an etm device to its element decoder
  std::unordered_map<uint8_t, TrcPktDecodeEtmV4I> element_decoders_;
  MemAccess mem_access_;
  InstructionDecoder instruction_decoder_;
  std::vector<ElementCallback*> callbacks_;
};

// Dump etm data generated at different stages.
class DataDumper : public ElementCallback {
 public:
  DataDumper(ETMV4IDecodeTree& decode_tree) : decode_tree_(decode_tree) {}

  void DumpRawData() {
    decode_tree_.AttachRawFramePrinter(frame_printer_);
    frame_printer_.setMessageLogger(&stdout_logger_);
  }

  void DumpPackets(const std::unordered_map<uint8_t, EtmV4Config>& configs) {
    for (auto& p : configs) {
      uint8_t trace_id = p.first;
      auto result = packet_printers_.emplace(trace_id, trace_id);
      CHECK(result.second);
      auto& packet_printer = result.first->second;
      decode_tree_.AttachPacketMonitor(trace_id, packet_printer);
      packet_printer.setMessageLogger(&stdout_logger_);
    }
  }

  void DumpElements() { element_printer_.setMessageLogger(&stdout_logger_); }

  ocsd_datapath_resp_t ProcessElement(ocsd_trc_index_t index_sop, uint8_t trc_chan_id,
                                      const OcsdTraceElement& elem, const ocsd_instr_info*) {
    return element_printer_.TraceElemIn(index_sop, trc_chan_id, elem);
  }

 private:
  ETMV4IDecodeTree& decode_tree_;
  RawFramePrinter frame_printer_;
  std::unordered_map<uint8_t, PacketPrinter<EtmV4ITrcPacket>> packet_printers_;
  TrcGenericElementPrinter element_printer_;
  ocsdMsgLogger stdout_logger_;
};

// It decodes each ETMV4IPacket into TraceElements, and generates ETMInstrRanges from TraceElements.
// Decoding each packet is slow, but ensures correctness.
class InstrRangeParser : public ElementCallback {
 private:
  struct TraceData {
    ETMInstrRange instr_range;
    bool wait_for_branch_to_addr_fix = false;
  };

 public:
  InstrRangeParser(MapLocator& map_locator, const ETMDecoder::CallbackFn& callback)
      : map_locator_(map_locator), callback_(callback) {}

  ocsd_datapath_resp_t ProcessElement(const ocsd_trc_index_t, uint8_t trace_id,
                                      const OcsdTraceElement& elem,
                                      const ocsd_instr_info* next_instr) override {
    if (elem.getType() == OCSD_GEN_TRC_ELEM_INSTR_RANGE) {
      TraceData& data = trace_data_[trace_id];
      const MapEntry* map = map_locator_.FindMap(trace_id, elem.st_addr);
      if (map == nullptr) {
        FlushData(data);
        return OCSD_RESP_CONT;
      }
      uint64_t start_addr = map->GetVaddrInFile(elem.st_addr);
      auto& instr_range = data.instr_range;

      if (data.wait_for_branch_to_addr_fix) {
        // OpenCSD may cache a list of InstrRange elements, making it inaccurate to get branch to
        // address from next_instr->branch_addr. So fix it by using the start address of the next
        // InstrRange element.
        instr_range.branch_to_addr = start_addr;
      }
      FlushData(data);
      instr_range.dso = map->dso;
      instr_range.start_addr = start_addr;
      instr_range.end_addr = map->GetVaddrInFile(elem.en_addr - elem.last_instr_sz);
      bool end_with_branch =
          elem.last_i_type == OCSD_INSTR_BR || elem.last_i_type == OCSD_INSTR_BR_INDIRECT;
      bool branch_taken = end_with_branch && elem.last_instr_exec;
      if (elem.last_i_type == OCSD_INSTR_BR && branch_taken) {
        // It is based on the assumption that we only do immediate branch inside a binary,
        // which may not be true for all cases. TODO: http://b/151665001.
        instr_range.branch_to_addr = map->GetVaddrInFile(next_instr->branch_addr);
        data.wait_for_branch_to_addr_fix = true;
      } else {
        instr_range.branch_to_addr = 0;
      }
      instr_range.branch_taken_count = branch_taken ? 1 : 0;
      instr_range.branch_not_taken_count = branch_taken ? 0 : 1;

    } else if (elem.getType() == OCSD_GEN_TRC_ELEM_TRACE_ON) {
      // According to the ETM Specification, the Trace On element indicates a discontinuity in the
      // instruction trace stream. So it cuts the connection between instr ranges.
      FlushData(trace_data_[trace_id]);
    }
    return OCSD_RESP_CONT;
  }

  void FinishData() {
    for (auto& pair : trace_data_) {
      FlushData(pair.second);
    }
  }

 private:
  void FlushData(TraceData& data) {
    if (data.instr_range.dso != nullptr) {
      callback_(data.instr_range);
      data.instr_range.dso = nullptr;
    }
    data.wait_for_branch_to_addr_fix = false;
  }

  MapLocator& map_locator_;
  std::unordered_map<uint8_t, TraceData> trace_data_;
  ETMDecoder::CallbackFn callback_;
};

// Etm data decoding in OpenCSD library has two steps:
// 1. From byte stream to etm packets. Each packet shows an event happened. For example,
// an Address packet shows the cpu is running the instruction at that address, an Atom
// packet shows whether the cpu decides to branch or not.
// 2. From etm packets to trace elements. To generates elements, the decoder needs both etm
// packets and executed binaries. For example, an InstructionRange element needs the decoder
// to find the next branch instruction starting from an address.
//
// ETMDecoderImpl uses OpenCSD library to decode etm data. It has the following properties:
// 1. Supports flexible decoding strategy. It allows installing packet callbacks and element
// callbacks, and decodes to either packets or elements based on requirements.
// 2. Supports dumping data at different stages.
class ETMDecoderImpl : public ETMDecoder {
 public:
  ETMDecoderImpl(ThreadTree& thread_tree) : thread_tree_(thread_tree) {}

  void CreateDecodeTree(const AuxTraceInfoRecord& auxtrace_info) {
    for (int i = 0; i < auxtrace_info.data->nr_cpu; i++) {
      auto& etm4 = auxtrace_info.data->etm4_info[i];
      ocsd_etmv4_cfg cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.reg_idr0 = etm4.trcidr0;
      cfg.reg_idr1 = etm4.trcidr1;
      cfg.reg_idr2 = etm4.trcidr2;
      cfg.reg_idr8 = etm4.trcidr8;
      cfg.reg_configr = etm4.trcconfigr;
      cfg.reg_traceidr = etm4.trctraceidr;
      cfg.arch_ver = ARCH_V8;
      cfg.core_prof = profile_CortexA;
      uint8_t trace_id = cfg.reg_traceidr & 0x7f;
      configs_.emplace(trace_id, &cfg);
      decode_tree_.CreateDecoder(configs_[trace_id]);
      auto result = packet_sinks_.emplace(trace_id, trace_id);
      CHECK(result.second);
      decode_tree_.AttachPacketSink(trace_id, result.first->second);
    }
  }

  void EnableDump(const ETMDumpOption& option) override {
    dumper_.reset(new DataDumper(decode_tree_));
    if (option.dump_raw_data) {
      dumper_->DumpRawData();
    }
    if (option.dump_packets) {
      dumper_->DumpPackets(configs_);
    }
    if (option.dump_elements) {
      dumper_->DumpElements();
      InstallElementCallback(dumper_.get());
    }
  }

  void RegisterCallback(const CallbackFn& callback) {
    InstallMapLocator();
    instr_range_parser_.reset(new InstrRangeParser(*map_locator_, callback));
    InstallElementCallback(instr_range_parser_.get());
  }

  bool ProcessData(const uint8_t* data, size_t size) override {
    // Reset decoders before processing each data block. Because:
    // 1. Data blocks are not continuous. So decoders shouldn't keep previous states when
    //    processing a new block.
    // 2. The beginning part of a data block may be truncated if kernel buffer is temporarily full.
    //    So we may see garbage data, which can cause decoding errors if we don't reset decoders.
    auto resp =
        decode_tree_.GetDataIn().TraceDataIn(OCSD_OP_RESET, data_index_, 0, nullptr, nullptr);
    if (IsRespError(resp)) {
      LOG(ERROR) << "failed to reset decoder, resp " << resp;
      return false;
    }
    size_t left_size = size;
    while (left_size > 0) {
      uint32_t processed;
      auto resp = decode_tree_.GetDataIn().TraceDataIn(OCSD_OP_DATA, data_index_, left_size, data,
                                                       &processed);
      if (IsRespError(resp)) {
        // A decoding error shouldn't ruin all data. Reset decoders to recover from it.
        LOG(INFO) << "reset etm decoders for seeing a decode failure, resp " << resp;
        decode_tree_.GetDataIn().TraceDataIn(OCSD_OP_RESET, data_index_ + processed, 0, nullptr,
                                             nullptr);
      }
      data += processed;
      left_size -= processed;
      data_index_ += processed;
    }
    return true;
  }

  bool FinishData() override {
    if (instr_range_parser_) {
      instr_range_parser_->FinishData();
    }
    return true;
  }

 private:
  void InstallMapLocator() {
    if (!map_locator_) {
      map_locator_.reset(new MapLocator(thread_tree_));
      InstallPacketCallback(map_locator_.get());
    }
  }

  void InstallPacketCallback(PacketCallback* callback) {
    for (auto& p : packet_sinks_) {
      p.second.AddCallback(callback);
    }
  }

  void InstallElementCallback(ElementCallback* callback) {
    if (!packet_to_element_) {
      InstallMapLocator();
      packet_to_element_.reset(
          new PacketToElement(*map_locator_, configs_, decode_tree_.ErrorLogger()));
      InstallPacketCallback(packet_to_element_.get());
    }
    packet_to_element_->AddCallback(callback);
  }

  // map ip address to binary path and binary offset
  ThreadTree& thread_tree_;
  // handle to build OpenCSD decoder
  ETMV4IDecodeTree decode_tree_;
  // map from the trace id of an etm device to its config
  std::unordered_map<uint8_t, EtmV4Config> configs_;
  // map from the trace id of an etm device to its PacketSink
  std::unordered_map<uint8_t, PacketSink> packet_sinks_;
  std::unique_ptr<PacketToElement> packet_to_element_;
  std::unique_ptr<DataDumper> dumper_;
  // an index keeping processed etm data size
  size_t data_index_ = 0;
  std::unique_ptr<InstrRangeParser> instr_range_parser_;
  std::unique_ptr<MapLocator> map_locator_;
};

}  // namespace

namespace simpleperf {

bool ParseEtmDumpOption(const std::string& s, ETMDumpOption* option) {
  for (auto& value : android::base::Split(s, ",")) {
    if (value == "raw") {
      option->dump_raw_data = true;
    } else if (value == "packet") {
      option->dump_packets = true;
    } else if (value == "element") {
      option->dump_elements = true;
    } else {
      LOG(ERROR) << "unknown etm dump option: " << value;
      return false;
    }
  }
  return true;
}

std::unique_ptr<ETMDecoder> ETMDecoder::Create(const AuxTraceInfoRecord& auxtrace_info,
                                               ThreadTree& thread_tree) {
  auto decoder = std::make_unique<ETMDecoderImpl>(thread_tree);
  decoder->CreateDecodeTree(auxtrace_info);
  return std::unique_ptr<ETMDecoder>(decoder.release());
}

}  // namespace simpleperf