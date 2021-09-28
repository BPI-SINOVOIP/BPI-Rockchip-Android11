// Copyright (C) 2018 The Android Open Source Project
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

#ifndef HEADER_CHECKER_H_
#define HEADER_CHECKER_H_

#include "repr/ir_representation.h"

#include <set>
#include <string>


namespace header_checker {
namespace dumper {


class HeaderCheckerOptions {
 public:
  std::string source_file_;
  std::string dump_name_;
  std::set<std::string> exported_headers_;
  repr::TextFormatIR text_format_;
  bool dump_function_declarations_;
  bool suppress_errors_;

 public:
  HeaderCheckerOptions(std::string source_file, std::string dump_name,
                       std::set<std::string> exported_headers,
                       repr::TextFormatIR text_format,
                       bool dump_function_declarations, bool suppress_errors)
      : source_file_(std::move(source_file)), dump_name_(std::move(dump_name)),
        exported_headers_(std::move(exported_headers)),
        text_format_(text_format),
        dump_function_declarations_(dump_function_declarations),
        suppress_errors_(suppress_errors) {}
};


}  // namespace dumper
}  // namespace header_checker


#endif  // HEADER_CHECKER_H_
