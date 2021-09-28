/*
 * Copyright 2019 The Android Open Source Project
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

#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <regex>
#include <sstream>
#include <vector>

#include "declarations.h"
#include "struct_parser_generator.h"

#include "language_y.h"

void yylex_init(void**);
void yylex_destroy(void*);
void yyset_debug(int, void*);
void yyset_in(FILE*, void*);

namespace {

void parse_namespace(const std::string& root_namespace, const std::filesystem::path& input_file_relative_path,
                     std::vector<std::string>* token) {
  std::filesystem::path gen_namespace = root_namespace / input_file_relative_path;
  std::string gen_namespace_str = gen_namespace;
  std::regex path_tokenizer("/");
  auto it = std::sregex_token_iterator(gen_namespace_str.cbegin(), gen_namespace_str.cend(), path_tokenizer, -1);
  std::sregex_token_iterator it_end = {};
  for (; it != it_end; ++it) {
    token->push_back(it->str());
  }
}

void generate_namespace_open(const std::vector<std::string>& token, std::ostream& output) {
  for (const auto& ns : token) {
    output << "namespace " << ns << " {" << std::endl;
  }
}

void generate_namespace_close(const std::vector<std::string>& token, std::ostream& output) {
  for (auto it = token.rbegin(); it != token.rend(); ++it) {
    output << "}  //namespace " << *it << std::endl;
  }
}

bool parse_declarations_one_file(const std::filesystem::path& input_file, Declarations* declarations) {
  void* scanner;
  yylex_init(&scanner);

  FILE* in_file = fopen(input_file.string().c_str(), "r");
  if (in_file == nullptr) {
    std::cerr << "can't open " << input_file << ": " << strerror(errno) << std::endl;
    return false;
  }

  yyset_in(in_file, scanner);

  int ret = yy::parser(scanner, declarations).parse();
  if (ret != 0) {
    std::cerr << "yylex parsing failed: returned " << ret << std::endl;
    return false;
  }

  yylex_destroy(scanner);

  fclose(in_file);

  // Set endianess before returning
  for (auto& s : declarations->type_defs_queue_) {
    if (s.second->GetDefinitionType() == TypeDef::Type::STRUCT) {
      auto* struct_def = dynamic_cast<StructDef*>(s.second);
      struct_def->SetEndianness(declarations->is_little_endian);
    }
  }

  for (auto& packet_def : declarations->packet_defs_queue_) {
    packet_def.second.SetEndianness(declarations->is_little_endian);
  }

  return true;
}

bool generate_cpp_headers_one_file(const Declarations& decls, const std::filesystem::path& input_file,
                                   const std::filesystem::path& include_dir, const std::filesystem::path& out_dir,
                                   const std::string& root_namespace) {
  auto gen_relative_path = input_file.lexically_relative(include_dir).parent_path();

  auto input_filename = input_file.filename().string().substr(0, input_file.filename().string().find(".pdl"));
  auto gen_path = out_dir / gen_relative_path;

  std::filesystem::create_directories(gen_path);

  auto gen_file = gen_path / (input_filename + ".h");

  std::ofstream out_file;
  out_file.open(gen_file);
  if (!out_file.is_open()) {
    std::cerr << "can't open " << gen_file << std::endl;
    return false;
  }

  out_file << "\n\n";
  out_file << "#pragma once\n";
  out_file << "\n\n";
  out_file << "#include <stdint.h>\n";
  out_file << "#include <string>\n";
  out_file << "#include <functional>\n";
  out_file << "\n\n";
  out_file << "#include \"os/log.h\"\n";
  out_file << "#include \"packet/base_packet_builder.h\"\n";
  out_file << "#include \"packet/bit_inserter.h\"\n";
  out_file << "#include \"packet/iterator.h\"\n";
  out_file << "#include \"packet/packet_builder.h\"\n";
  out_file << "#include \"packet/packet_struct.h\"\n";
  out_file << "#include \"packet/packet_view.h\"\n";
  out_file << "#include \"packet/parser/checksum_type_checker.h\"\n";
  out_file << "#include \"packet/parser/custom_type_checker.h\"\n";
  out_file << "\n\n";

  for (const auto& c : decls.type_defs_queue_) {
    if (c.second->GetDefinitionType() == TypeDef::Type::CUSTOM ||
        c.second->GetDefinitionType() == TypeDef::Type::CHECKSUM) {
      ((CustomFieldDef*)c.second)->GenInclude(out_file);
    }
  }
  out_file << "\n\n";

  std::vector<std::string> namespace_list;
  parse_namespace(root_namespace, gen_relative_path, &namespace_list);
  generate_namespace_open(namespace_list, out_file);
  out_file << "\n\n";

  for (const auto& c : decls.type_defs_queue_) {
    if (c.second->GetDefinitionType() == TypeDef::Type::CUSTOM ||
        c.second->GetDefinitionType() == TypeDef::Type::CHECKSUM) {
      ((CustomFieldDef*)c.second)->GenUsing(out_file);
    }
  }
  out_file << "\n\n";

  out_file << "using ::bluetooth::packet::BasePacketBuilder;";
  out_file << "using ::bluetooth::packet::BitInserter;";
  out_file << "using ::bluetooth::packet::CustomTypeChecker;";
  out_file << "using ::bluetooth::packet::Iterator;";
  out_file << "using ::bluetooth::packet::kLittleEndian;";
  out_file << "using ::bluetooth::packet::PacketBuilder;";
  out_file << "using ::bluetooth::packet::PacketStruct;";
  out_file << "using ::bluetooth::packet::PacketView;";
  out_file << "using ::bluetooth::packet::parser::ChecksumTypeChecker;";
  out_file << "\n\n";

  for (const auto& e : decls.type_defs_queue_) {
    if (e.second->GetDefinitionType() == TypeDef::Type::ENUM) {
      const auto* enum_def = dynamic_cast<const EnumDef*>(e.second);
      EnumGen gen(*enum_def);
      gen.GenDefinition(out_file);
      out_file << "\n\n";
    }
  }
  for (const auto& e : decls.type_defs_queue_) {
    if (e.second->GetDefinitionType() == TypeDef::Type::ENUM) {
      const auto* enum_def = dynamic_cast<const EnumDef*>(e.second);
      EnumGen gen(*enum_def);
      gen.GenLogging(out_file);
      out_file << "\n\n";
    }
  }
  for (const auto& ch : decls.type_defs_queue_) {
    if (ch.second->GetDefinitionType() == TypeDef::Type::CHECKSUM) {
      const auto* checksum_def = dynamic_cast<const ChecksumDef*>(ch.second);
      checksum_def->GenChecksumCheck(out_file);
    }
  }
  out_file << "\n/* Done ChecksumChecks */\n";

  for (const auto& c : decls.type_defs_queue_) {
    if (c.second->GetDefinitionType() == TypeDef::Type::CUSTOM && c.second->size_ == -1 /* Variable Size */) {
      const auto* custom_field_def = dynamic_cast<const CustomFieldDef*>(c.second);
      custom_field_def->GenCustomFieldCheck(out_file, decls.is_little_endian);
    }
  }
  out_file << "\n";

  for (auto& s : decls.type_defs_queue_) {
    if (s.second->GetDefinitionType() == TypeDef::Type::STRUCT) {
      const auto* struct_def = dynamic_cast<const StructDef*>(s.second);
      struct_def->GenDefinition(out_file);
      out_file << "\n";
    }
  }

  {
    StructParserGenerator spg(decls);
    spg.Generate(out_file);
    out_file << "\n\n";
  }

  for (const auto& packet_def : decls.packet_defs_queue_) {
    packet_def.second.GenParserDefinition(out_file);
    out_file << "\n\n";
  }

  for (const auto& packet_def : decls.packet_defs_queue_) {
    packet_def.second.GenBuilderDefinition(out_file);
    out_file << "\n\n";
  }

  generate_namespace_close(namespace_list, out_file);

  out_file.close();

  return true;
}

// Get the out_file shard at a symbol_count
std::ofstream& get_out_file(size_t symbol_count, size_t symbol_total, std::vector<std::ofstream>* out_files) {
  auto symbols_per_shard = symbol_total / out_files->size();
  auto file_index = std::min(symbol_count / symbols_per_shard, out_files->size() - 1);
  return out_files->at(file_index);
}

bool generate_pybind11_sources_one_file(const Declarations& decls, const std::filesystem::path& input_file,
                                        const std::filesystem::path& include_dir, const std::filesystem::path& out_dir,
                                        const std::string& root_namespace, size_t num_shards) {
  auto gen_relative_path = input_file.lexically_relative(include_dir).parent_path();

  auto input_filename = input_file.filename().string().substr(0, input_file.filename().string().find(".pdl"));
  auto gen_path = out_dir / gen_relative_path;

  std::filesystem::create_directories(gen_path);

  auto gen_relative_header = gen_relative_path / (input_filename + ".h");

  std::vector<std::string> namespace_list;
  parse_namespace(root_namespace, gen_relative_path, &namespace_list);

  std::vector<std::ofstream> out_file_shards(num_shards);
  for (size_t i = 0; i < out_file_shards.size(); i++) {
    auto filename = gen_path / (input_filename + "_python3_shard_" + std::to_string(i) + ".cc");
    auto& out_file = out_file_shards[i];
    out_file.open(filename);
    if (!out_file.is_open()) {
      std::cerr << "can't open " << filename << std::endl;
      return false;
    }
    out_file << "#include <pybind11/pybind11.h>\n";
    out_file << "#include <pybind11/stl.h>\n";
    out_file << "\n\n";
    out_file << "#include " << gen_relative_header << "\n";
    out_file << "\n\n";
    out_file << "#include \"packet/raw_builder.h\"\n";
    out_file << "\n\n";

    for (const auto& c : decls.type_defs_queue_) {
      if (c.second->GetDefinitionType() == TypeDef::Type::CUSTOM) {
        const auto* custom_def = dynamic_cast<const CustomFieldDef*>(c.second);
        custom_def->GenPyBind11Include(out_file);
      }
    }

    out_file << "\n\n";

    generate_namespace_open(namespace_list, out_file);
    out_file << "\n\n";

    for (const auto& c : decls.type_defs_queue_) {
      if (c.second->GetDefinitionType() == TypeDef::Type::CUSTOM ||
          c.second->GetDefinitionType() == TypeDef::Type::CHECKSUM) {
        const auto* custom_def = dynamic_cast<const CustomFieldDef*>(c.second);
        custom_def->GenUsing(out_file);
      }
    }
    out_file << "\n\n";

    out_file << "using ::bluetooth::packet::BasePacketBuilder;";
    out_file << "using ::bluetooth::packet::BitInserter;";
    out_file << "using ::bluetooth::packet::CustomTypeChecker;";
    out_file << "using ::bluetooth::packet::Iterator;";
    out_file << "using ::bluetooth::packet::kLittleEndian;";
    out_file << "using ::bluetooth::packet::PacketBuilder;";
    out_file << "using ::bluetooth::packet::BaseStruct;";
    out_file << "using ::bluetooth::packet::PacketStruct;";
    out_file << "using ::bluetooth::packet::PacketView;";
    out_file << "using ::bluetooth::packet::RawBuilder;";
    out_file << "using ::bluetooth::packet::parser::ChecksumTypeChecker;";
    out_file << "\n\n";

    out_file << "namespace py = pybind11;\n\n";

    out_file << "void define_" << input_filename << "_submodule_shard_" << std::to_string(i) << "(py::module& m) {\n\n";
  }
  size_t symbol_total = 0;
  // Only count types that will be generated
  for (const auto& e : decls.type_defs_queue_) {
    if (e.second->GetDefinitionType() == TypeDef::Type::ENUM) {
      symbol_total++;
    } else if (e.second->GetDefinitionType() == TypeDef::Type::STRUCT) {
      symbol_total++;
    }
  }
  // View and builder are counted separately
  symbol_total += decls.packet_defs_queue_.size() * 2;
  size_t symbol_count = 0;

  for (const auto& e : decls.type_defs_queue_) {
    if (e.second->GetDefinitionType() == TypeDef::Type::ENUM) {
      const auto* enum_def = dynamic_cast<const EnumDef*>(e.second);
      EnumGen gen(*enum_def);
      auto& out_file = get_out_file(symbol_count, symbol_total, &out_file_shards);
      gen.GenDefinitionPybind11(out_file);
      out_file << "\n\n";
      symbol_count++;
    }
  }

  for (const auto& s : decls.type_defs_queue_) {
    if (s.second->GetDefinitionType() == TypeDef::Type::STRUCT) {
      const auto* struct_def = dynamic_cast<const StructDef*>(s.second);
      auto& out_file = get_out_file(symbol_count, symbol_total, &out_file_shards);
      struct_def->GenDefinitionPybind11(out_file);
      out_file << "\n";
      symbol_count++;
    }
  }

  for (const auto& packet_def : decls.packet_defs_queue_) {
    auto& out_file = get_out_file(symbol_count, symbol_total, &out_file_shards);
    packet_def.second.GenParserDefinitionPybind11(out_file);
    out_file << "\n\n";
    symbol_count++;
  }

  for (const auto& p : decls.packet_defs_queue_) {
    auto& out_file = get_out_file(symbol_count, symbol_total, &out_file_shards);
    p.second.GenBuilderDefinitionPybind11(out_file);
    out_file << "\n\n";
    symbol_count++;
  }

  for (auto& out_file : out_file_shards) {
    out_file << "}\n\n";
    generate_namespace_close(namespace_list, out_file);
  }

  auto gen_file_main = gen_path / (input_filename + "_python3.cc");
  std::ofstream out_file_main;
  out_file_main.open(gen_file_main);
  if (!out_file_main.is_open()) {
    std::cerr << "can't open " << gen_file_main << std::endl;
    return false;
  }
  out_file_main << "#include <pybind11/pybind11.h>\n";
  generate_namespace_open(namespace_list, out_file_main);

  out_file_main << "namespace py = pybind11;\n\n";

  for (size_t i = 0; i < out_file_shards.size(); i++) {
    out_file_main << "void define_" << input_filename << "_submodule_shard_" << std::to_string(i)
                  << "(py::module& m);\n";
  }

  out_file_main << "void define_" << input_filename << "_submodule(py::module& m) {\n\n";
  for (size_t i = 0; i < out_file_shards.size(); i++) {
    out_file_main << "define_" << input_filename << "_submodule_shard_" << std::to_string(i) << "(m);\n";
  }
  out_file_main << "}\n\n";

  generate_namespace_close(namespace_list, out_file_main);

  return true;
}

}  // namespace

// TODO(b/141583809): stop leaks
extern "C" const char* __asan_default_options() {
  return "detect_leaks=0";
}

int main(int argc, const char** argv) {
  std::filesystem::path out_dir;
  std::filesystem::path include_dir;
  std::string root_namespace = "bluetooth";
  // Number of shards per output pybind11 cc file
  size_t num_shards = 1;
  std::queue<std::filesystem::path> input_files;
  const std::string arg_out = "--out=";
  const std::string arg_include = "--include=";
  const std::string arg_namespace = "--root_namespace=";
  const std::string arg_num_shards = "--num_shards=";

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg.find(arg_out) == 0) {
      out_dir = std::filesystem::current_path() / std::filesystem::path(arg.substr(arg_out.size()));
    } else if (arg.find(arg_include) == 0) {
      include_dir = std::filesystem::current_path() / std::filesystem::path(arg.substr(arg_include.size()));
    } else if (arg.find(arg_namespace) == 0) {
      root_namespace = arg.substr(arg_namespace.size());
    } else if (arg.find(arg_num_shards) == 0) {
      num_shards = std::stoul(arg.substr(arg_num_shards.size()));
    } else {
      input_files.emplace(std::filesystem::current_path() / std::filesystem::path(arg));
    }
  }
  if (out_dir == std::filesystem::path() || include_dir == std::filesystem::path() || num_shards == 0) {
    std::cerr << "Usage: bt-packetgen --out=OUT --include=INCLUDE --root_namespace=NAMESPACE --num_shards=NUM_SHARDS "
              << "input_files..." << std::endl;
    return 1;
  }

  while (!input_files.empty()) {
    Declarations declarations;
    if (!parse_declarations_one_file(input_files.front(), &declarations)) {
      std::cerr << "Cannot parse " << input_files.front() << " correctly" << std::endl;
      return 2;
    }
    if (!generate_cpp_headers_one_file(declarations, input_files.front(), include_dir, out_dir, root_namespace)) {
      std::cerr << "Didn't generate cpp headers for " << input_files.front() << std::endl;
      return 3;
    }
    if (!generate_pybind11_sources_one_file(declarations, input_files.front(), include_dir, out_dir, root_namespace,
                                            num_shards)) {
      std::cerr << "Didn't generate pybind11 sources for " << input_files.front() << std::endl;
      return 4;
    }
    input_files.pop();
  }

  return 0;
}
