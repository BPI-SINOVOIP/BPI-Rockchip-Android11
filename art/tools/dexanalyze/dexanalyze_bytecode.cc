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

#include "dexanalyze_bytecode.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

#include "dex/class_accessor-inl.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_instruction-inl.h"

namespace art {
namespace dexanalyze {

// Given a map of <key, usage count>, sort by most used and assign index <key, index in most used>
enum class Order {
  kMostUsed,
  kNormal,
};

template <typename T, typename U>
static inline SafeMap<T, U> SortByOrder(const SafeMap<T, U>& usage, Order order) {
  std::vector<std::pair<U, T>> most_used;
  for (const auto& pair : usage) {
    most_used.emplace_back(pair.second, pair.first);
  }
  if (order == Order::kMostUsed) {
    std::sort(most_used.rbegin(), most_used.rend());
  }
  U current_index = 0u;
  SafeMap<T, U> ret;
  for (auto&& pair : most_used) {
    CHECK(ret.emplace(pair.second, current_index++).second);
  }
  return ret;
}

template <typename A, typename B>
std::ostream& operator <<(std::ostream& os, const std::pair<A, B>& pair) {
  return os << "{" << pair.first << ", " << pair.second << "}";
}

template <typename T, typename... Args, template <typename...> class ArrayType>
SafeMap<size_t, T> MakeUsageMap(const ArrayType<T, Args...>& array) {
  SafeMap<size_t, T> ret;
  for (size_t i = 0; i < array.size(); ++i) {
    if (array[i] > 0) {
      ret.Put(i, array[i]);
    }
  }
  return ret;
}

template <typename T, typename U, typename... Args, template <typename...> class Map>
void PrintMostUsed(std::ostream& os,
                   const Map<T, U, Args...>& usage,
                   size_t max_count,
                   std::function<void(std::ostream& os, T)> printer =
                       [](std::ostream& os, T v) {
    os << v;
  }) {
  std::vector<std::pair<U, T>> sorted;
  uint64_t total = 0u;
  for (const auto& pair : usage) {
    sorted.emplace_back(pair.second, pair.first);
    total += pair.second;
  }
  std::sort(sorted.rbegin(), sorted.rend());
  uint64_t other = 0u;
  for (auto&& pair : sorted) {
    if (max_count > 0) {
      os << Percent(pair.first, total) << " : ";
      printer(os, pair.second);
      os << "\n";
      --max_count;
    } else {
      other += pair.first;
    }
  }
  if (other != 0u) {
    os << "other: " << Percent(other, total) << "\n";
  }
}

static inline std::ostream& operator<<(std::ostream& os, const std::vector<uint8_t>& bytes) {
  os << std::hex;
  for (const uint8_t& c : bytes) {
    os << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(c)
       << (&c != &bytes.back() ? " " : "");
  }
  os << std::dec;
  return os;
}

void NewRegisterInstructions::ProcessDexFiles(
    const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
  std::set<std::vector<uint8_t>> deduped;
  for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
    std::map<size_t, TypeLinkage> types;
    std::set<const void*> visited;
    for (ClassAccessor accessor : dex_file->GetClasses()) {
      for (const ClassAccessor::Method& method : accessor.GetMethods()) {
        ProcessCodeItem(*dex_file,
                        method.GetInstructionsAndData(),
                        accessor.GetClassIdx(),
                        /*count_types=*/ true,
                        types);
      }
    }
    // Reorder to get an index for each map instead of a count.
    for (auto&& pair : types) {
      pair.second.types_ = SortByOrder(pair.second.types_, Order::kMostUsed);
      pair.second.fields_ = SortByOrder(pair.second.fields_, Order::kMostUsed);
      pair.second.methods_ = SortByOrder(pair.second.methods_, Order::kMostUsed);
      pair.second.strings_ = SortByOrder(pair.second.strings_, Order::kMostUsed);
    }
    // Visit classes and convert code items.
    for (ClassAccessor accessor : dex_file->GetClasses()) {
      for (const ClassAccessor::Method& method : accessor.GetMethods()) {
        if (method.GetCodeItem() == nullptr || !visited.insert(method.GetCodeItem()).second) {
          continue;
        }
        if (verbose_level_ >= VerboseLevel::kEverything) {
          std::cout << std::endl
                    << "Processing " << dex_file->PrettyMethod(method.GetIndex(), true);
        }
        CodeItemDataAccessor data = method.GetInstructionsAndData();
        ProcessCodeItem(*dex_file,
                        data,
                        accessor.GetClassIdx(),
                        /*count_types=*/ false,
                        types);
        std::vector<uint8_t> buffer = std::move(buffer_);
        buffer_.clear();
        const size_t buffer_size = buffer.size();
        dex_code_bytes_ += data.InsnsSizeInBytes();
        output_size_ += buffer_size;
        // Add extra data at the end to have fair dedupe.
        EncodeUnsignedLeb128(&buffer, data.RegistersSize());
        EncodeUnsignedLeb128(&buffer, data.InsSize());
        EncodeUnsignedLeb128(&buffer, data.OutsSize());
        EncodeUnsignedLeb128(&buffer, data.TriesSize());
        EncodeUnsignedLeb128(&buffer, data.InsnsSizeInCodeUnits());
        if (deduped.insert(buffer).second) {
          deduped_size_ += buffer_size;
        }
      }
    }
  }
}

void NewRegisterInstructions::Dump(std::ostream& os, uint64_t total_size) const {
  os << "Enabled experiments " << experiments_ << std::endl;
  os << "Total Dex code bytes: " << Percent(dex_code_bytes_, total_size) << "\n";
  os << "Total output code bytes: " << Percent(output_size_, total_size) << "\n";
  os << "Total deduped code bytes: " << Percent(deduped_size_, total_size) << "\n";
  std::vector<std::pair<size_t, std::vector<uint8_t>>> pairs;
  for (auto&& pair : instruction_freq_) {
    if (pair.second > 0 && !pair.first.empty()) {
      // Savings exclude one byte per occurrence and one occurrence from having the macro
      // dictionary.
      pairs.emplace_back((pair.second - 1) * (pair.first.size() - 1), pair.first);
    }
  }
  std::sort(pairs.rbegin(), pairs.rend());
  static constexpr size_t kMaxMacros = 128;
  static constexpr size_t kMaxPrintedMacros = 32;
  uint64_t top_instructions_savings = 0u;
  for (size_t i = 0; i < kMaxMacros && i < pairs.size(); ++i) {
    top_instructions_savings += pairs[i].first;
  }
  if (verbose_level_ >= VerboseLevel::kNormal) {
    os << "Move result register distribution" << "\n";
    PrintMostUsed(os, MakeUsageMap(move_result_reg_), 16);
    os << "First arg register usage\n";
    std::function<void(std::ostream& os, size_t)> printer = [&](std::ostream& os, size_t idx) {
      os << Instruction::Name(static_cast<Instruction::Code>(idx));
    };
    PrintMostUsed(os, MakeUsageMap(first_arg_reg_count_), 16, printer);
    os << "Most used field linkage pairs\n";
    PrintMostUsed(os, field_linkage_counts_, 32);
    os << "Current extended " << extended_field_ << "\n";
    os << "Most used method linkage pairs\n";
    PrintMostUsed(os, method_linkage_counts_, 32);
    os << "Current extended " << extended_method_ << "\n";
    os << "Top " << kMaxMacros << " instruction bytecode sizes and hex dump" << "\n";
    for (size_t i = 0; i < kMaxMacros && i < pairs.size(); ++i) {
      auto bytes = pairs[i].second;
      // Remove opcode bytes.
      bytes.erase(bytes.begin());
      if (i < kMaxPrintedMacros) {
        os << Percent(pairs[i].first, total_size) << " "
           << Instruction::Name(static_cast<Instruction::Code>(pairs[i].second[0]))
           << "(" << bytes << ")\n";
      }
    }
  }
  os << "Top instructions 1b macro savings "
     << Percent(top_instructions_savings, total_size) << "\n";
}

void NewRegisterInstructions::ProcessCodeItem(const DexFile& dex_file,
                                              const CodeItemDataAccessor& code_item,
                                              dex::TypeIndex current_class_type,
                                              bool count_types,
                                              std::map<size_t, TypeLinkage>& types) {
  TypeLinkage& current_type = types[current_class_type.index_];
  bool skip_next = false;
  for (auto inst = code_item.begin(); inst != code_item.end(); ++inst) {
    if (verbose_level_ >= VerboseLevel::kEverything) {
      std::cout << std::endl;
      std::cout << inst->DumpString(nullptr);
      if (skip_next) {
        std::cout << " (SKIPPED)";
      }
    }
    if (skip_next) {
      skip_next = false;
      continue;
    }
    bool is_iget = false;
    const Instruction::Code opcode = inst->Opcode();
    Instruction::Code new_opcode = opcode;
    ++opcode_count_[opcode];
    switch (opcode) {
      case Instruction::IGET:
      case Instruction::IGET_WIDE:
      case Instruction::IGET_OBJECT:
      case Instruction::IGET_BOOLEAN:
      case Instruction::IGET_BYTE:
      case Instruction::IGET_CHAR:
      case Instruction::IGET_SHORT:
        is_iget = true;
        FALLTHROUGH_INTENDED;
      case Instruction::IPUT:
      case Instruction::IPUT_WIDE:
      case Instruction::IPUT_OBJECT:
      case Instruction::IPUT_BOOLEAN:
      case Instruction::IPUT_BYTE:
      case Instruction::IPUT_CHAR:
      case Instruction::IPUT_SHORT: {
        const uint32_t dex_field_idx = inst->VRegC_22c();
        if (Enabled(kExperimentSingleGetSet)) {
          // Test deduplication improvements from replacing all iget/set with the same opcode.
          new_opcode = is_iget ? Instruction::IGET : Instruction::IPUT;
        }
        CHECK_LT(dex_field_idx, dex_file.NumFieldIds());
        dex::TypeIndex holder_type = dex_file.GetFieldId(dex_field_idx).class_idx_;
        uint32_t receiver = inst->VRegB_22c();
        uint32_t first_arg_reg = code_item.RegistersSize() - code_item.InsSize();
        uint32_t out_reg = inst->VRegA_22c();
        if (Enabled(kExperimentInstanceFieldSelf) &&
            first_arg_reg == receiver &&
            holder_type == current_class_type) {
          if (count_types) {
            ++current_type.fields_.FindOrAdd(dex_field_idx)->second;
          } else {
            uint32_t field_idx = types[holder_type.index_].fields_.Get(dex_field_idx);
            ExtendPrefix(&out_reg, &field_idx);
            CHECK(InstNibbles(new_opcode, {out_reg, field_idx}));
            continue;
          }
        } else if (Enabled(kExperimentInstanceField)) {
          if (count_types) {
            ++current_type.types_.FindOrAdd(holder_type.index_)->second;
            ++types[holder_type.index_].fields_.FindOrAdd(dex_field_idx)->second;
          } else {
            uint32_t type_idx = current_type.types_.Get(holder_type.index_);
            uint32_t field_idx = types[holder_type.index_].fields_.Get(dex_field_idx);
            ExtendPrefix(&type_idx, &field_idx);
            CHECK(InstNibbles(new_opcode, {out_reg, receiver, type_idx, field_idx}));
            continue;
          }
        }
        break;
      }
      case Instruction::CONST_STRING:
      case Instruction::CONST_STRING_JUMBO: {
        const bool is_jumbo = opcode == Instruction::CONST_STRING_JUMBO;
        const uint16_t str_idx = is_jumbo ? inst->VRegB_31c() : inst->VRegB_21c();
        uint32_t out_reg = is_jumbo ? inst->VRegA_31c() : inst->VRegA_21c();
        if (Enabled(kExperimentString)) {
          new_opcode = Instruction::CONST_STRING;
          if (count_types) {
            ++current_type.strings_.FindOrAdd(str_idx)->second;
          } else {
            uint32_t idx = current_type.strings_.Get(str_idx);
            ExtendPrefix(&out_reg, &idx);
            CHECK(InstNibbles(opcode, {out_reg, idx}));
            continue;
          }
        }
        break;
      }
      case Instruction::SGET:
      case Instruction::SGET_WIDE:
      case Instruction::SGET_OBJECT:
      case Instruction::SGET_BOOLEAN:
      case Instruction::SGET_BYTE:
      case Instruction::SGET_CHAR:
      case Instruction::SGET_SHORT:
      case Instruction::SPUT:
      case Instruction::SPUT_WIDE:
      case Instruction::SPUT_OBJECT:
      case Instruction::SPUT_BOOLEAN:
      case Instruction::SPUT_BYTE:
      case Instruction::SPUT_CHAR:
      case Instruction::SPUT_SHORT: {
        uint32_t out_reg = inst->VRegA_21c();
        const uint32_t dex_field_idx = inst->VRegB_21c();
        CHECK_LT(dex_field_idx, dex_file.NumFieldIds());
        dex::TypeIndex holder_type = dex_file.GetFieldId(dex_field_idx).class_idx_;
        if (Enabled(kExperimentStaticField)) {
          if (holder_type == current_class_type) {
            if (count_types) {
              ++types[holder_type.index_].fields_.FindOrAdd(dex_field_idx)->second;
            } else {
              uint32_t field_idx = types[holder_type.index_].fields_.Get(dex_field_idx);
              ExtendPrefix(&out_reg, &field_idx);
              if (InstNibbles(new_opcode, {out_reg, field_idx})) {
                continue;
              }
            }
          } else {
            if (count_types) {
              ++types[current_class_type.index_].types_.FindOrAdd(holder_type.index_)->second;
              ++types[holder_type.index_].fields_.FindOrAdd(dex_field_idx)->second;
            } else {
              uint32_t type_idx = current_type.types_.Get(holder_type.index_);
              uint32_t field_idx = types[holder_type.index_].fields_.Get(dex_field_idx);
              ++field_linkage_counts_[std::make_pair(type_idx, field_idx)];
              extended_field_ += ExtendPrefix(&type_idx, &field_idx) ? 1u : 0u;
              if (InstNibbles(new_opcode, {out_reg >> 4, out_reg & 0xF, type_idx, field_idx})) {
                continue;
              }
            }
          }
        }
        break;
      }
      // Invoke cases.
      case Instruction::INVOKE_VIRTUAL:
      case Instruction::INVOKE_DIRECT:
      case Instruction::INVOKE_STATIC:
      case Instruction::INVOKE_INTERFACE:
      case Instruction::INVOKE_SUPER: {
        const uint32_t method_idx = DexMethodIndex(inst.Inst());
        const dex::MethodId& method = dex_file.GetMethodId(method_idx);
        const dex::TypeIndex receiver_type = method.class_idx_;
        if (Enabled(kExperimentInvoke)) {
          if (count_types) {
            ++current_type.types_.FindOrAdd(receiver_type.index_)->second;
            ++types[receiver_type.index_].methods_.FindOrAdd(method_idx)->second;
          } else {
            uint32_t args[6] = {};
            uint32_t arg_count = inst->GetVarArgs(args);
            const uint32_t first_arg_reg = code_item.RegistersSize() - code_item.InsSize();

            bool next_move_result = false;
            uint32_t dest_reg = 0;
            auto next = std::next(inst);
            if (next != code_item.end()) {
              next_move_result =
                  next->Opcode() == Instruction::MOVE_RESULT ||
                  next->Opcode() == Instruction::MOVE_RESULT_WIDE ||
                  next->Opcode() == Instruction::MOVE_RESULT_OBJECT;
              if (next_move_result) {
                dest_reg = next->VRegA_11x();
                ++move_result_reg_[dest_reg];
              }
            }

            uint32_t type_idx = current_type.types_.Get(receiver_type.index_);
            uint32_t local_idx = types[receiver_type.index_].methods_.Get(method_idx);
            ++method_linkage_counts_[std::make_pair(type_idx, local_idx)];

            // If true, we always put the return value in r0.
            static constexpr bool kMoveToDestReg = true;

            std::vector<uint32_t> new_args;
            if (kMoveToDestReg && arg_count % 2 == 1) {
              // Use the extra nibble to sneak in part of the type index.
              new_args.push_back(local_idx >> 4);
              local_idx &= ~0xF0;
            }
            extended_method_ += ExtendPrefix(&type_idx, &local_idx) ? 1u : 0u;
            new_args.push_back(type_idx);
            new_args.push_back(local_idx);
            if (!kMoveToDestReg) {
              ExtendPrefix(&dest_reg, &local_idx);
              new_args.push_back(dest_reg);
            }
            for (size_t i = 0; i < arg_count; ++i) {
              if (args[i] == first_arg_reg) {
                ++first_arg_reg_count_[opcode];
                break;
              }
            }
            new_args.insert(new_args.end(), args, args + arg_count);
            if (InstNibbles(opcode, new_args)) {
              skip_next = next_move_result;
              if (kMoveToDestReg && dest_reg != 0u) {
                CHECK(InstNibbles(Instruction::MOVE, {dest_reg >> 4, dest_reg & 0xF}));
              }
              continue;
            }
          }
        }
        break;
      }
      case Instruction::IF_EQZ:
      case Instruction::IF_NEZ: {
        uint32_t reg = inst->VRegA_21t();
        int16_t offset = inst->VRegB_21t();
        if (!count_types &&
            Enabled(kExperimentSmallIf) &&
            InstNibbles(opcode, {reg, static_cast<uint16_t>(offset)})) {
          continue;
        }
        break;
      }
      case Instruction::INSTANCE_OF: {
        uint32_t type_idx = inst->VRegC_22c();
        uint32_t in_reg = inst->VRegB_22c();
        uint32_t out_reg = inst->VRegA_22c();
        if (count_types) {
          ++current_type.types_.FindOrAdd(type_idx)->second;
        } else {
          uint32_t local_type = current_type.types_.Get(type_idx);
          ExtendPrefix(&in_reg, &local_type);
          CHECK(InstNibbles(new_opcode, {in_reg, out_reg, local_type}));
          continue;
        }
        break;
      }
      case Instruction::NEW_ARRAY: {
        uint32_t len_reg = inst->VRegB_22c();
        uint32_t type_idx = inst->VRegC_22c();
        uint32_t out_reg = inst->VRegA_22c();
        if (count_types) {
          ++current_type.types_.FindOrAdd(type_idx)->second;
        } else {
          uint32_t local_type = current_type.types_.Get(type_idx);
          ExtendPrefix(&out_reg, &local_type);
          CHECK(InstNibbles(new_opcode, {len_reg, out_reg, local_type}));
          continue;
        }
        break;
      }
      case Instruction::CONST_CLASS:
      case Instruction::CHECK_CAST:
      case Instruction::NEW_INSTANCE: {
        uint32_t type_idx = inst->VRegB_21c();
        uint32_t out_reg = inst->VRegA_21c();
        if (Enabled(kExperimentLocalType)) {
          if (count_types) {
            ++current_type.types_.FindOrAdd(type_idx)->second;
          } else {
            bool next_is_init = false;
            if (opcode == Instruction::NEW_INSTANCE) {
              auto next = std::next(inst);
              if (next != code_item.end() && next->Opcode() == Instruction::INVOKE_DIRECT) {
                uint32_t args[6] = {};
                uint32_t arg_count = next->GetVarArgs(args);
                uint32_t method_idx = DexMethodIndex(next.Inst());
                if (arg_count == 1u &&
                    args[0] == out_reg &&
                    dex_file.GetMethodName(dex_file.GetMethodId(method_idx)) ==
                        std::string("<init>")) {
                  next_is_init = true;
                }
              }
            }
            uint32_t local_type = current_type.types_.Get(type_idx);
            ExtendPrefix(&out_reg, &local_type);
            CHECK(InstNibbles(opcode, {out_reg, local_type}));
            skip_next = next_is_init;
            continue;
          }
        }
        break;
      }
      case Instruction::RETURN:
      case Instruction::RETURN_OBJECT:
      case Instruction::RETURN_WIDE:
      case Instruction::RETURN_VOID: {
        if (!count_types && Enabled(kExperimentReturn)) {
          if (opcode == Instruction::RETURN_VOID || inst->VRegA_11x() == 0) {
            if (InstNibbles(opcode, {})) {
              continue;
            }
          }
        }
        break;
      }
      default:
        break;
    }
    if (!count_types) {
      Add(new_opcode, inst.Inst());
    }
  }
  if (verbose_level_ >= VerboseLevel::kEverything) {
    std::cout << std::endl
              << "Bytecode size " << code_item.InsnsSizeInBytes() << " -> " << buffer_.size();
    std::cout << std::endl;
  }
}

void NewRegisterInstructions::Add(Instruction::Code opcode, const Instruction& inst) {
  const uint8_t* start = reinterpret_cast<const uint8_t*>(&inst);
  const size_t buffer_start = buffer_.size();
  buffer_.push_back(opcode);
  buffer_.insert(buffer_.end(), start + 1, start + 2 * inst.SizeInCodeUnits());
  // Register the instruction blob.
  ++instruction_freq_[std::vector<uint8_t>(buffer_.begin() + buffer_start, buffer_.end())];
}

bool NewRegisterInstructions::ExtendPrefix(uint32_t* value1, uint32_t* value2) {
  if (*value1 < 16 && *value2 < 16) {
    return false;
  }
  if ((*value1 >> 4) == 1 && *value2 < 16) {
    InstNibbles(0xE5, {});
    *value1 ^= 1u << 4;
    return true;
  } else if ((*value2 >> 4) == 1 && *value1 < 16) {
    InstNibbles(0xE6, {});
    *value2 ^= 1u << 4;
    return true;
  }
  if (*value1 < 256 && *value2 < 256) {
    // Extend each value by 4 bits.
    CHECK(InstNibbles(0xE3, {*value1 >> 4, *value2 >> 4}));
  } else {
    // Extend each value by 12 bits.
    CHECK(InstNibbles(0xE4, {
        (*value1 >> 12) & 0xF,
        (*value1 >> 8) & 0xF,
        (*value1 >> 4) & 0xF,
        (*value2 >> 12) & 0xF,
        (*value2 >> 8) & 0xF,
        (*value2 >> 4) & 0xF}));
  }
  *value1 &= 0xF;
  *value2 &= 0XF;
  return true;
}

bool NewRegisterInstructions::InstNibbles(uint8_t opcode, const std::vector<uint32_t>& args) {
  if (verbose_level_ >= VerboseLevel::kEverything) {
    std::cout << " ==> " << Instruction::Name(static_cast<Instruction::Code>(opcode)) << " ";
    for (int v : args) {
      std::cout << v << ", ";
    }
  }
  for (int v : args) {
    if (v >= 16) {
      if (verbose_level_ >= VerboseLevel::kEverything) {
        std::cout << "(OUT_OF_RANGE)";
      }
      return false;
    }
  }
  const size_t buffer_start = buffer_.size();
  buffer_.push_back(opcode);
  for (size_t i = 0; i < args.size(); i += 2) {
    buffer_.push_back(args[i] << 4);
    if (i + 1 < args.size()) {
      buffer_.back() |= args[i + 1];
    }
  }
  while (buffer_.size() % alignment_ != 0) {
    buffer_.push_back(0);
  }
  // Register the instruction blob.
  ++instruction_freq_[std::vector<uint8_t>(buffer_.begin() + buffer_start, buffer_.end())];
  return true;
}

}  // namespace dexanalyze
}  // namespace art
