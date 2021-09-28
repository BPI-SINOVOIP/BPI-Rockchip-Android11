/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_COMPILER_DEBUG_ELF_DEBUG_LINE_WRITER_H_
#define ART_COMPILER_DEBUG_ELF_DEBUG_LINE_WRITER_H_

#include <unordered_set>
#include <vector>

#include "debug/elf_compilation_unit.h"
#include "debug/src_map_elem.h"
#include "dex/dex_file-inl.h"
#include "dwarf/debug_line_opcode_writer.h"
#include "dwarf/headers.h"
#include "elf/elf_builder.h"
#include "oat_file.h"
#include "stack_map.h"

namespace art {
namespace debug {

typedef std::vector<DexFile::PositionInfo> PositionInfos;

template<typename ElfTypes>
class ElfDebugLineWriter {
  using Elf_Addr = typename ElfTypes::Addr;

 public:
  explicit ElfDebugLineWriter(ElfBuilder<ElfTypes>* builder) : builder_(builder) {
  }

  void Start() {
    builder_->GetDebugLine()->Start();
  }

  // Write line table for given set of methods.
  // Returns the number of bytes written.
  size_t WriteCompilationUnit(ElfCompilationUnit& compilation_unit) {
    const InstructionSet isa = builder_->GetIsa();
    const bool is64bit = Is64BitInstructionSet(isa);
    const Elf_Addr base_address = compilation_unit.is_code_address_text_relative
        ? builder_->GetText()->GetAddress()
        : 0;

    compilation_unit.debug_line_offset = builder_->GetDebugLine()->GetPosition();

    std::vector<dwarf::FileEntry> files;
    std::unordered_map<std::string, size_t> files_map;
    std::vector<std::string> directories;
    std::unordered_map<std::string, size_t> directories_map;
    int code_factor_bits_ = 0;
    int dwarf_isa = -1;
    switch (isa) {
      case InstructionSet::kArm:  // arm actually means thumb2.
      case InstructionSet::kThumb2:
        code_factor_bits_ = 1;  // 16-bit instuctions
        dwarf_isa = 1;  // DW_ISA_ARM_thumb.
        break;
      case InstructionSet::kArm64:
        code_factor_bits_ = 2;  // 32-bit instructions
        break;
      case InstructionSet::kNone:
      case InstructionSet::kX86:
      case InstructionSet::kX86_64:
        break;
    }
    std::unordered_set<uint64_t> seen_addresses(compilation_unit.methods.size());
    dwarf::DebugLineOpCodeWriter<> opcodes(is64bit, code_factor_bits_);
    for (const MethodDebugInfo* mi : compilation_unit.methods) {
      // Ignore function if we have already generated line table for the same address.
      // It would confuse the debugger and the DWARF specification forbids it.
      // We allow the line table for method to be replicated in different compilation unit.
      // This ensures that each compilation unit contains line table for all its methods.
      if (!seen_addresses.insert(mi->code_address).second) {
        continue;
      }

      uint32_t prologue_end = std::numeric_limits<uint32_t>::max();
      std::vector<SrcMapElem> pc2dex_map;
      if (mi->code_info != nullptr) {
        // Use stack maps to create mapping table from pc to dex.
        const CodeInfo code_info(mi->code_info);
        pc2dex_map.reserve(code_info.GetNumberOfStackMaps());
        for (StackMap stack_map : code_info.GetStackMaps()) {
          const uint32_t pc = stack_map.GetNativePcOffset(isa);
          const int32_t dex = stack_map.GetDexPc();
          pc2dex_map.push_back({pc, dex});
          if (stack_map.HasDexRegisterMap()) {
            // Guess that the first map with local variables is the end of prologue.
            prologue_end = std::min(prologue_end, pc);
          }
        }
        std::sort(pc2dex_map.begin(), pc2dex_map.end());
      }

      if (pc2dex_map.empty()) {
        continue;
      }

      // Compensate for compiler's off-by-one-instruction error.
      //
      // The compiler generates stackmap with PC *after* the branch instruction
      // (because this is the PC which is easier to obtain when unwinding).
      //
      // However, the debugger is more clever and it will ask us for line-number
      // mapping at the location of the branch instruction (since the following
      // instruction could belong to other line, this is the correct thing to do).
      //
      // So we really want to just decrement the PC by one instruction so that the
      // branch instruction is covered as well. However, we do not know the size
      // of the previous instruction, and we can not subtract just a fixed amount
      // (the debugger would trust us that the PC is valid; it might try to set
      // breakpoint there at some point, and setting breakpoint in mid-instruction
      // would make the process crash in spectacular way).
      //
      // Therefore, we say that the PC which the compiler gave us for the stackmap
      // is the end of its associated address range, and we use the PC from the
      // previous stack map as the start of the range. This ensures that the PC is
      // valid and that the branch instruction is covered.
      //
      // This ensures we have correct line number mapping at call sites (which is
      // important for backtraces), but there is nothing we can do for non-call
      // sites (so stepping through optimized code in debugger is not possible).
      //
      // We do not adjust the stackmaps if the code was compiled as debuggable.
      // In that case, the stackmaps should accurately cover all instructions.
      if (!mi->is_native_debuggable) {
        for (size_t i = pc2dex_map.size() - 1; i > 0; --i) {
          pc2dex_map[i].from_ = pc2dex_map[i - 1].from_;
        }
        pc2dex_map[0].from_ = 0;
      }

      Elf_Addr method_address = base_address + mi->code_address;

      PositionInfos dex2line_map;
      const DexFile* dex = mi->dex_file;
      DCHECK(dex != nullptr);
      CodeItemDebugInfoAccessor accessor(*dex, mi->code_item, mi->dex_method_index);
      if (!accessor.DecodeDebugPositionInfo(
          [&](const DexFile::PositionInfo& entry) {
            dex2line_map.push_back(entry);
            return false;
          })) {
        continue;
      }

      if (dex2line_map.empty()) {
        continue;
      }

      opcodes.SetAddress(method_address);
      if (dwarf_isa != -1) {
        opcodes.SetISA(dwarf_isa);
      }

      // Get and deduplicate directory and filename.
      int file_index = 0;  // 0 - primary source file of the compilation.
      auto& dex_class_def = dex->GetClassDef(mi->class_def_index);
      const char* source_file = dex->GetSourceFile(dex_class_def);
      if (source_file != nullptr) {
        std::string file_name(source_file);
        size_t file_name_slash = file_name.find_last_of('/');
        std::string class_name(dex->GetClassDescriptor(dex_class_def));
        size_t class_name_slash = class_name.find_last_of('/');
        std::string full_path(file_name);

        // Guess directory from package name.
        int directory_index = 0;  // 0 - current directory of the compilation.
        if (file_name_slash == std::string::npos &&  // Just filename.
            class_name.front() == 'L' &&  // Type descriptor for a class.
            class_name_slash != std::string::npos) {  // Has package name.
          std::string package_name = class_name.substr(1, class_name_slash - 1);
          auto it = directories_map.find(package_name);
          if (it == directories_map.end()) {
            directory_index = 1 + directories.size();
            directories_map.emplace(package_name, directory_index);
            directories.push_back(package_name);
          } else {
            directory_index = it->second;
          }
          full_path = package_name + "/" + file_name;
        }

        // Add file entry.
        auto it2 = files_map.find(full_path);
        if (it2 == files_map.end()) {
          file_index = 1 + files.size();
          files_map.emplace(full_path, file_index);
          files.push_back(dwarf::FileEntry {
            file_name,
            directory_index,
            0,  // Modification time - NA.
            0,  // File size - NA.
          });
        } else {
          file_index = it2->second;
        }
      }
      opcodes.SetFile(file_index);

      // Generate mapping opcodes from PC to Java lines.
      if (file_index != 0) {
        // If the method was not compiled as native-debuggable, we still generate all available
        // lines, but we try to prevent the debugger from stepping and setting breakpoints since
        // the information is too inaccurate for that (breakpoints would be set after the calls).
        const bool default_is_stmt = mi->is_native_debuggable;
        bool first = true;
        for (SrcMapElem pc2dex : pc2dex_map) {
          uint32_t pc = pc2dex.from_;
          int dex_pc = pc2dex.to_;
          // Find mapping with address with is greater than our dex pc; then go back one step.
          auto dex2line = std::upper_bound(
              dex2line_map.begin(),
              dex2line_map.end(),
              dex_pc,
              [](uint32_t address, const DexFile::PositionInfo& entry) {
                  return address < entry.address_;
              });
          // Look for first valid mapping after the prologue.
          if (dex2line != dex2line_map.begin() && pc >= prologue_end) {
            int line = (--dex2line)->line_;
            if (first) {
              first = false;
              if (pc > 0) {
                // Assume that any preceding code is prologue.
                int first_line = dex2line_map.front().line_;
                // Prologue is not a sensible place for a breakpoint.
                opcodes.SetIsStmt(false);
                opcodes.AddRow(method_address, first_line);
                opcodes.SetPrologueEnd();
              }
              opcodes.SetIsStmt(default_is_stmt);
              opcodes.AddRow(method_address + pc, line);
            } else if (line != opcodes.CurrentLine()) {
              opcodes.SetIsStmt(default_is_stmt);
              opcodes.AddRow(method_address + pc, line);
            }
          }
        }
      } else {
        // line 0 - instruction cannot be attributed to any source line.
        opcodes.AddRow(method_address, 0);
      }

      opcodes.AdvancePC(method_address + mi->code_size);
      opcodes.EndSequence();
    }
    std::vector<uint8_t> buffer;
    buffer.reserve(opcodes.data()->size() + KB);
    WriteDebugLineTable(directories, files, opcodes, &buffer);
    builder_->GetDebugLine()->WriteFully(buffer.data(), buffer.size());
    return buffer.size();
  }

  void End() {
    builder_->GetDebugLine()->End();
  }

 private:
  ElfBuilder<ElfTypes>* builder_;
};

}  // namespace debug
}  // namespace art

#endif  // ART_COMPILER_DEBUG_ELF_DEBUG_LINE_WRITER_H_

