//
// Copyright 2018 Pierre Moreau
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//

#include "invocation.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef HAVE_CLOVER_SPIRV
#include <spirv-tools/libspirv.hpp>
#include <spirv-tools/linker.hpp>
#endif

#include "core/error.hpp"
#include "core/platform.hpp"
#include "invocation.hpp"
#include "llvm/util.hpp"
#include "pipe/p_state.h"
#include "util/algorithm.hpp"
#include "util/functional.hpp"
#include "util/u_math.h"

#include "compiler/spirv/spirv.h"

#define SPIRV_HEADER_WORD_SIZE 5

using namespace clover;

#ifdef HAVE_CLOVER_SPIRV
namespace {

   uint32_t
   make_spirv_version(uint8_t major, uint8_t minor) {
      return (static_cast<uint32_t>(major) << 16u) |
             (static_cast<uint32_t>(minor) << 8u);
   }

   template<typename T>
   T get(const char *source, size_t index) {
      const uint32_t *word_ptr = reinterpret_cast<const uint32_t *>(source);
      return static_cast<T>(word_ptr[index]);
   }

   enum module::argument::type
   convert_storage_class(SpvStorageClass storage_class, std::string &err) {
      switch (storage_class) {
      case SpvStorageClassFunction:
         return module::argument::scalar;
      case SpvStorageClassUniformConstant:
         return module::argument::global;
      case SpvStorageClassWorkgroup:
         return module::argument::local;
      case SpvStorageClassCrossWorkgroup:
         return module::argument::global;
      default:
         err += "Invalid storage type " + std::to_string(storage_class) + "\n";
         throw build_error();
      }
   }

   cl_kernel_arg_address_qualifier
   convert_storage_class_to_cl(SpvStorageClass storage_class) {
      switch (storage_class) {
      case SpvStorageClassUniformConstant:
         return CL_KERNEL_ARG_ADDRESS_CONSTANT;
      case SpvStorageClassWorkgroup:
         return CL_KERNEL_ARG_ADDRESS_LOCAL;
      case SpvStorageClassCrossWorkgroup:
         return CL_KERNEL_ARG_ADDRESS_GLOBAL;
      case SpvStorageClassFunction:
      default:
         return CL_KERNEL_ARG_ADDRESS_PRIVATE;
      }
   }

   enum module::argument::type
   convert_image_type(SpvId id, SpvDim dim, SpvAccessQualifier access,
                      std::string &err) {
      if (dim == SpvDim2D && access == SpvAccessQualifierReadOnly)
         return module::argument::image2d_rd;
      else if (dim == SpvDim2D && access == SpvAccessQualifierWriteOnly)
         return module::argument::image2d_wr;
      else if (dim == SpvDim3D && access == SpvAccessQualifierReadOnly)
         return module::argument::image3d_rd;
      else if (dim == SpvDim3D && access == SpvAccessQualifierWriteOnly)
         return module::argument::image3d_wr;
      else {
         err += "Unknown access qualifier " + std::to_string(access)
             +  " or dimension " + std::to_string(dim) + " for image "
             +  std::to_string(id) + ".\n";
         throw build_error();
      }
   }

   module::section
   make_text_section(const std::vector<char> &code,
                     enum module::section::type section_type) {
      const pipe_binary_program_header header { uint32_t(code.size()) };
      module::section text { 0, section_type, header.num_bytes, {} };

      text.data.insert(text.data.end(), reinterpret_cast<const char *>(&header),
                       reinterpret_cast<const char *>(&header) + sizeof(header));
      text.data.insert(text.data.end(), code.begin(), code.end());

      return text;
   }

   module
   create_module_from_spirv(const std::vector<char> &source,
                            size_t pointer_byte_size,
                            std::string &err) {
      const size_t length = source.size() / sizeof(uint32_t);
      size_t i = SPIRV_HEADER_WORD_SIZE; // Skip header

      std::string kernel_name;
      size_t kernel_nb = 0u;
      std::vector<module::argument> args;
      std::vector<size_t> req_local_size;

      module m;

      std::unordered_map<SpvId, std::vector<size_t> > req_local_sizes;
      std::unordered_map<SpvId, std::string> kernels;
      std::unordered_map<SpvId, module::argument> types;
      std::unordered_map<SpvId, SpvId> pointer_types;
      std::unordered_map<SpvId, unsigned int> constants;
      std::unordered_set<SpvId> packed_structures;
      std::unordered_map<SpvId, std::vector<SpvFunctionParameterAttribute>>
         func_param_attr_map;
      std::unordered_map<SpvId, std::string> names;
      std::unordered_map<SpvId, cl_kernel_arg_type_qualifier> qualifiers;
      std::unordered_map<std::string, std::vector<std::string> > param_type_names;

      while (i < length) {
         const auto inst = &source[i * sizeof(uint32_t)];
         const auto desc_word = get<uint32_t>(inst, 0);
         const auto opcode = static_cast<SpvOp>(desc_word & SpvOpCodeMask);
         const unsigned int num_operands = desc_word >> SpvWordCountShift;

         switch (opcode) {
         case SpvOpName: {
            names.emplace(get<SpvId>(inst, 1),
                          source.data() + (i + 2u) * sizeof(uint32_t));
            break;
         }

         case SpvOpString: {
            // SPIRV-LLVM-Translator stores param type names as OpStrings
            std::string str(source.data() + (i + 2u) * sizeof(uint32_t));
            if (str.find("kernel_arg_type.") != 0)
               break;

            std::string line;
            std::istringstream istream(str.substr(16));

            std::getline(istream, line, '.');

            std::string k = line;
            while (std::getline(istream, line, ','))
               param_type_names[k].push_back(line);
            break;
         }

         case SpvOpEntryPoint:
            if (get<SpvExecutionModel>(inst, 1) == SpvExecutionModelKernel)
               kernels.emplace(get<SpvId>(inst, 2),
                               source.data() + (i + 3u) * sizeof(uint32_t));
            break;

         case SpvOpExecutionMode:
            switch (get<SpvExecutionMode>(inst, 2)) {
            case SpvExecutionModeLocalSize:
               req_local_sizes[get<SpvId>(inst, 1)] = {
                  get<uint32_t>(inst, 3),
                  get<uint32_t>(inst, 4),
                  get<uint32_t>(inst, 5)
               };
               break;
            default:
               break;
            }
            break;

         case SpvOpDecorate: {
            const auto id = get<SpvId>(inst, 1);
            const auto decoration = get<SpvDecoration>(inst, 2);
            switch (decoration) {
            case SpvDecorationCPacked:
               packed_structures.emplace(id);
               break;
            case SpvDecorationFuncParamAttr: {
               const auto attribute =
                  get<SpvFunctionParameterAttribute>(inst, 3u);
               func_param_attr_map[id].push_back(attribute);
               break;
            }
            case SpvDecorationVolatile:
               qualifiers[id] |= CL_KERNEL_ARG_TYPE_VOLATILE;
               break;
            default:
               break;
            }
            break;
         }

         case SpvOpGroupDecorate: {
            const auto group_id = get<SpvId>(inst, 1);
            if (packed_structures.count(group_id)) {
               for (unsigned int i = 2u; i < num_operands; ++i)
                  packed_structures.emplace(get<SpvId>(inst, i));
            }
            const auto func_param_attr_iter =
               func_param_attr_map.find(group_id);
            if (func_param_attr_iter != func_param_attr_map.end()) {
               for (unsigned int i = 2u; i < num_operands; ++i) {
                  auto &attrs = func_param_attr_map[get<SpvId>(inst, i)];
                  attrs.insert(attrs.begin(),
                               func_param_attr_iter->second.begin(),
                               func_param_attr_iter->second.end());
               }
            }
            if (qualifiers.count(group_id)) {
               for (unsigned int i = 2u; i < num_operands; ++i)
                  qualifiers[get<SpvId>(inst, i)] |= qualifiers[group_id];
            }
            break;
         }

         case SpvOpConstant:
            // We only care about constants that represent the size of arrays.
            // If they are passed as argument, they will never be more than
            // 4GB-wide, and even if they did, a clover::module::argument size
            // is represented by an int.
            constants[get<SpvId>(inst, 2)] = get<unsigned int>(inst, 3u);
            break;

         case SpvOpTypeInt:
         case SpvOpTypeFloat: {
            const auto size = get<uint32_t>(inst, 2) / 8u;
            const auto id = get<SpvId>(inst, 1);
            types[id] = { module::argument::scalar, size, size, size,
                          module::argument::zero_ext };
            types[id].info.address_qualifier = CL_KERNEL_ARG_ADDRESS_PRIVATE;
            break;
         }

         case SpvOpTypeArray: {
            const auto id = get<SpvId>(inst, 1);
            const auto type_id = get<SpvId>(inst, 2);
            const auto types_iter = types.find(type_id);
            if (types_iter == types.end())
               break;

            const auto constant_id = get<SpvId>(inst, 3);
            const auto constants_iter = constants.find(constant_id);
            if (constants_iter == constants.end()) {
               err += "Constant " + std::to_string(constant_id) +
                  " is missing\n";
               throw build_error();
            }
            const auto elem_size = types_iter->second.size;
            const auto elem_nbs = constants_iter->second;
            const auto size = elem_size * elem_nbs;
            types[id] = { module::argument::scalar, size, size,
                          types_iter->second.target_align,
                          module::argument::zero_ext };
            break;
         }

         case SpvOpTypeStruct: {
            const auto id = get<SpvId>(inst, 1);
            const bool is_packed = packed_structures.count(id);

            unsigned struct_size = 0u;
            unsigned struct_align = 1u;
            for (unsigned j = 2u; j < num_operands; ++j) {
               const auto type_id = get<SpvId>(inst, j);
               const auto types_iter = types.find(type_id);

               // If a type was not found, that means it is not one of the
               // types allowed as kernel arguments. And since the module has
               // been validated, this means this type is not used for kernel
               // arguments, and therefore can be ignored.
               if (types_iter == types.end())
                  break;

               const auto alignment = is_packed ? 1u
                                                : types_iter->second.target_align;
               const auto padding = (-struct_size) & (alignment - 1u);
               struct_size += padding + types_iter->second.target_size;
               struct_align = std::max(struct_align, alignment);
            }
            struct_size += (-struct_size) & (struct_align - 1u);
            types[id] = { module::argument::scalar, struct_size, struct_size,
                          struct_align, module::argument::zero_ext };
            break;
         }

         case SpvOpTypeVector: {
            const auto id = get<SpvId>(inst, 1);
            const auto type_id = get<SpvId>(inst, 2);
            const auto types_iter = types.find(type_id);

            // If a type was not found, that means it is not one of the
            // types allowed as kernel arguments. And since the module has
            // been validated, this means this type is not used for kernel
            // arguments, and therefore can be ignored.
            if (types_iter == types.end())
               break;

            const auto elem_size = types_iter->second.size;
            const auto elem_nbs = get<uint32_t>(inst, 3);
            const auto size = elem_size * elem_nbs;
            const auto align = elem_size * util_next_power_of_two(elem_nbs);
            types[id] = { module::argument::scalar, size, size, align,
                          module::argument::zero_ext };
            types[id].info.address_qualifier = CL_KERNEL_ARG_ADDRESS_PRIVATE;
            break;
         }

         case SpvOpTypeForwardPointer: // FALLTHROUGH
         case SpvOpTypePointer: {
            const auto id = get<SpvId>(inst, 1);
            const auto storage_class = get<SpvStorageClass>(inst, 2);
            // Input means this is for a builtin variable, which can not be
            // passed as an argument to a kernel.
            if (storage_class == SpvStorageClassInput)
               break;

            if (opcode == SpvOpTypePointer)
               pointer_types[id] = get<SpvId>(inst, 3);

            types[id] = { convert_storage_class(storage_class, err),
                          sizeof(cl_mem),
                          static_cast<module::size_t>(pointer_byte_size),
                          static_cast<module::size_t>(pointer_byte_size),
                          module::argument::zero_ext };
            types[id].info.address_qualifier = convert_storage_class_to_cl(storage_class);
            break;
         }

         case SpvOpTypeSampler:
            types[get<SpvId>(inst, 1)] = { module::argument::sampler,
                                             sizeof(cl_sampler) };
            break;

         case SpvOpTypeImage: {
            const auto id = get<SpvId>(inst, 1);
            const auto dim = get<SpvDim>(inst, 3);
            const auto access = get<SpvAccessQualifier>(inst, 9);
            types[id] = { convert_image_type(id, dim, access, err),
                          sizeof(cl_mem), sizeof(cl_mem), sizeof(cl_mem),
                          module::argument::zero_ext };
            break;
         }

         case SpvOpTypePipe: // FALLTHROUGH
         case SpvOpTypeQueue: {
            err += "TypePipe and TypeQueue are valid SPIR-V 1.0 types, but are "
                   "not available in the currently supported OpenCL C version."
                   "\n";
            throw build_error();
         }

         case SpvOpFunction: {
            auto id = get<SpvId>(inst, 2);
            const auto kernels_iter = kernels.find(id);
            if (kernels_iter != kernels.end())
               kernel_name = kernels_iter->second;

            const auto req_local_size_iter = req_local_sizes.find(id);
            if (req_local_size_iter != req_local_sizes.end())
               req_local_size =  (*req_local_size_iter).second;
            else
               req_local_size = { 0, 0, 0 };

            break;
         }

         case SpvOpFunctionParameter: {
            if (kernel_name.empty())
               break;

            const auto id = get<SpvId>(inst, 2);
            const auto type_id = get<SpvId>(inst, 1);
            auto arg = types.find(type_id)->second;
            const auto &func_param_attr_iter =
               func_param_attr_map.find(get<SpvId>(inst, 2));
            if (func_param_attr_iter != func_param_attr_map.end()) {
               for (auto &i : func_param_attr_iter->second) {
                  switch (i) {
                  case SpvFunctionParameterAttributeSext:
                     arg.ext_type = module::argument::sign_ext;
                     break;
                  case SpvFunctionParameterAttributeZext:
                     arg.ext_type = module::argument::zero_ext;
                     break;
                  case SpvFunctionParameterAttributeByVal: {
                     const SpvId ptr_type_id =
                        pointer_types.find(type_id)->second;
                     arg = types.find(ptr_type_id)->second;
                     break;
                  }
                  case SpvFunctionParameterAttributeNoAlias:
                     arg.info.type_qualifier |= CL_KERNEL_ARG_TYPE_RESTRICT;
                     break;
                  case SpvFunctionParameterAttributeNoWrite:
                     arg.info.type_qualifier |= CL_KERNEL_ARG_TYPE_CONST;
                     break;
                  default:
                     break;
                  }
               }
            }

            auto name_it = names.find(id);
            if (name_it != names.end())
               arg.info.arg_name = (*name_it).second;

            arg.info.type_qualifier |= qualifiers[id];
            arg.info.address_qualifier = types[type_id].info.address_qualifier;
            arg.info.access_qualifier = CL_KERNEL_ARG_ACCESS_NONE;
            args.emplace_back(arg);
            break;
         }

         case SpvOpFunctionEnd:
            if (kernel_name.empty())
               break;

            for (size_t i = 0; i < param_type_names[kernel_name].size(); i++)
               args[i].info.type_name = param_type_names[kernel_name][i];

            m.syms.emplace_back(kernel_name, std::string(),
                                req_local_size, 0, kernel_nb, args);
            ++kernel_nb;
            kernel_name.clear();
            args.clear();
            break;

         default:
            break;
         }

         i += num_operands;
      }

      m.secs.push_back(make_text_section(source,
                                         module::section::text_intermediate));
      return m;
   }

   bool
   check_capabilities(const device &dev, const std::vector<char> &source,
                      std::string &r_log) {
      const size_t length = source.size() / sizeof(uint32_t);
      size_t i = SPIRV_HEADER_WORD_SIZE; // Skip header

      while (i < length) {
         const auto desc_word = get<uint32_t>(source.data(), i);
         const auto opcode = static_cast<SpvOp>(desc_word & SpvOpCodeMask);
         const unsigned int num_operands = desc_word >> SpvWordCountShift;

         if (opcode != SpvOpCapability)
            break;

         const auto capability = get<SpvCapability>(source.data(), i + 1u);
         switch (capability) {
         // Mandatory capabilities
         case SpvCapabilityAddresses:
         case SpvCapabilityFloat16Buffer:
         case SpvCapabilityGroups:
         case SpvCapabilityInt64:
         case SpvCapabilityInt16:
         case SpvCapabilityInt8:
         case SpvCapabilityKernel:
         case SpvCapabilityLinkage:
         case SpvCapabilityVector16:
            break;
         // Optional capabilities
         case SpvCapabilityImageBasic:
         case SpvCapabilityLiteralSampler:
         case SpvCapabilitySampled1D:
         case SpvCapabilityImage1D:
         case SpvCapabilitySampledBuffer:
         case SpvCapabilityImageBuffer:
            if (!dev.image_support()) {
               r_log += "Capability 'ImageBasic' is not supported.\n";
               return false;
            }
            break;
         case SpvCapabilityFloat64:
            if (!dev.has_doubles()) {
               r_log += "Capability 'Float64' is not supported.\n";
               return false;
            }
            break;
         // Enabled through extensions
         case SpvCapabilityFloat16:
            if (!dev.has_halves()) {
               r_log += "Capability 'Float16' is not supported.\n";
               return false;
            }
            break;
         case SpvCapabilityInt64Atomics:
            if (!dev.has_int64_atomics()) {
               r_log += "Capability 'Int64Atomics' is not supported.\n";
               return false;
            }
            break;
         default:
            r_log += "Capability '" + std::to_string(capability) +
                     "' is not supported.\n";
            return false;
         }

         i += num_operands;
      }

      return true;
   }

   bool
   check_extensions(const device &dev, const std::vector<char> &source,
                    std::string &r_log) {
      const size_t length = source.size() / sizeof(uint32_t);
      size_t i = SPIRV_HEADER_WORD_SIZE; // Skip header
      const auto spirv_extensions = spirv::supported_extensions();

      while (i < length) {
         const auto desc_word = get<uint32_t>(source.data(), i);
         const auto opcode = static_cast<SpvOp>(desc_word & SpvOpCodeMask);
         const unsigned int num_operands = desc_word >> SpvWordCountShift;

         if (opcode == SpvOpCapability) {
            i += num_operands;
            continue;
         }
         if (opcode != SpvOpExtension)
            break;

         const std::string extension = source.data() + (i + 1u) * sizeof(uint32_t);
         if (spirv_extensions.count(extension) == 0) {
            r_log += "Extension '" + extension + "' is not supported.\n";
            return false;
         }

         i += num_operands;
      }

      return true;
   }

   bool
   check_memory_model(const device &dev, const std::vector<char> &source,
                      std::string &r_log) {
      const size_t length = source.size() / sizeof(uint32_t);
      size_t i = SPIRV_HEADER_WORD_SIZE; // Skip header

      while (i < length) {
         const auto desc_word = get<uint32_t>(source.data(), i);
         const auto opcode = static_cast<SpvOp>(desc_word & SpvOpCodeMask);
         const unsigned int num_operands = desc_word >> SpvWordCountShift;

         switch (opcode) {
         case SpvOpMemoryModel:
            switch (get<SpvAddressingModel>(source.data(), i + 1u)) {
            case SpvAddressingModelPhysical32:
               return dev.address_bits() == 32;
            case SpvAddressingModelPhysical64:
               return dev.address_bits() == 64;
            default:
               unreachable("Only Physical32 and Physical64 are valid for OpenCL, and the binary was already validated");
               return false;
            }
            break;
         default:
            break;
         }

         i += num_operands;
      }

      return false;
   }

   // Copies the input binary and convert it to the endianness of the host CPU.
   std::vector<char>
   spirv_to_cpu(const std::vector<char> &binary)
   {
      const uint32_t first_word = get<uint32_t>(binary.data(), 0u);
      if (first_word == SpvMagicNumber)
         return binary;

      std::vector<char> cpu_endianness_binary(binary.size());
      for (size_t i = 0; i < (binary.size() / 4u); ++i) {
         const uint32_t word = get<uint32_t>(binary.data(), i);
         reinterpret_cast<uint32_t *>(cpu_endianness_binary.data())[i] =
            util_bswap32(word);
      }

      return cpu_endianness_binary;
   }

#ifdef HAVE_CLOVER_SPIRV
   std::string
   format_validator_msg(spv_message_level_t level, const char * /* source */,
                        const spv_position_t &position, const char *message) {
      std::string level_str;
      switch (level) {
      case SPV_MSG_FATAL:
         level_str = "Fatal";
         break;
      case SPV_MSG_INTERNAL_ERROR:
         level_str = "Internal error";
         break;
      case SPV_MSG_ERROR:
         level_str = "Error";
         break;
      case SPV_MSG_WARNING:
         level_str = "Warning";
         break;
      case SPV_MSG_INFO:
         level_str = "Info";
         break;
      case SPV_MSG_DEBUG:
         level_str = "Debug";
         break;
      }
      return "[" + level_str + "] At word No." +
             std::to_string(position.index) + ": \"" + message + "\"\n";
   }

   spv_target_env
   convert_opencl_str_to_target_env(const std::string &opencl_version) {
      // Pick 1.2 for 3.0 for now
      if (opencl_version == "3.0") {
         return SPV_ENV_OPENCL_1_2;
      } else if (opencl_version == "2.2") {
         return SPV_ENV_OPENCL_2_2;
      } else if (opencl_version == "2.1") {
         return SPV_ENV_OPENCL_2_1;
      } else if (opencl_version == "2.0") {
         return SPV_ENV_OPENCL_2_0;
      } else if (opencl_version == "1.2" ||
                 opencl_version == "1.1" ||
                 opencl_version == "1.0") {
         // SPIR-V is only defined for OpenCL >= 1.2, however some drivers
         // might use it with OpenCL 1.0 and 1.1.
         return SPV_ENV_OPENCL_1_2;
      } else {
         throw build_error("Invalid OpenCL version");
      }
   }
#endif

}

module
clover::spirv::compile_program(const std::vector<char> &binary,
                               const device &dev, std::string &r_log,
                               bool validate) {
   std::vector<char> source = spirv_to_cpu(binary);

   if (validate && !is_valid_spirv(source, dev.device_version(), r_log))
      throw build_error();

   if (!check_capabilities(dev, source, r_log))
      throw build_error();
   if (!check_extensions(dev, source, r_log))
      throw build_error();
   if (!check_memory_model(dev, source, r_log))
      throw build_error();

   return create_module_from_spirv(source,
                                   dev.address_bits() == 32 ? 4u : 8u, r_log);
}

module
clover::spirv::link_program(const std::vector<module> &modules,
                            const device &dev, const std::string &opts,
                            std::string &r_log) {
   std::vector<std::string> options = tokenize(opts);

   bool create_library = false;

   std::string ignored_options;
   for (const std::string &option : options) {
      if (option == "-create-library") {
         create_library = true;
      } else {
         ignored_options += "'" + option + "' ";
      }
   }
   if (!ignored_options.empty()) {
      r_log += "Ignoring the following link options: " + ignored_options
            + "\n";
   }

   spvtools::LinkerOptions linker_options;
   linker_options.SetCreateLibrary(create_library);

   module m;

   const auto section_type = create_library ? module::section::text_library :
                                              module::section::text_executable;

   std::vector<const uint32_t *> sections;
   sections.reserve(modules.size());
   std::vector<size_t> lengths;
   lengths.reserve(modules.size());

   auto const validator_consumer = [&r_log](spv_message_level_t level,
                                            const char *source,
                                            const spv_position_t &position,
                                            const char *message) {
      r_log += format_validator_msg(level, source, position, message);
   };

   for (const auto &mod : modules) {
      const auto &msec = find([](const module::section &sec) {
                  return sec.type == module::section::text_intermediate ||
                         sec.type == module::section::text_library;
               }, mod.secs);

      const auto c_il = ((struct pipe_binary_program_header*)msec.data.data())->blob;
      const auto length = msec.size;

      sections.push_back(reinterpret_cast<const uint32_t *>(c_il));
      lengths.push_back(length / sizeof(uint32_t));
   }

   std::vector<uint32_t> linked_binary;

   const std::string opencl_version = dev.device_version();
   const spv_target_env target_env =
      convert_opencl_str_to_target_env(opencl_version);

   const spvtools::MessageConsumer consumer = validator_consumer;
   spvtools::Context context(target_env);
   context.SetMessageConsumer(std::move(consumer));

   if (Link(context, sections.data(), lengths.data(), sections.size(),
            &linked_binary, linker_options) != SPV_SUCCESS)
      throw error(CL_LINK_PROGRAM_FAILURE);

   std::vector<char> final_binary{
         reinterpret_cast<char *>(linked_binary.data()),
         reinterpret_cast<char *>(linked_binary.data() +
               linked_binary.size()) };
   if (!is_valid_spirv(final_binary, opencl_version, r_log))
      throw error(CL_LINK_PROGRAM_FAILURE);

   if (has_flag(llvm::debug::spirv))
      llvm::debug::log(".spvasm", spirv::print_module(final_binary, dev.device_version()));

   for (const auto &mod : modules)
      m.syms.insert(m.syms.end(), mod.syms.begin(), mod.syms.end());

   m.secs.emplace_back(make_text_section(final_binary, section_type));

   return m;
}

bool
clover::spirv::is_valid_spirv(const std::vector<char> &binary,
                              const std::string &opencl_version,
                              std::string &r_log) {
   auto const validator_consumer =
      [&r_log](spv_message_level_t level, const char *source,
               const spv_position_t &position, const char *message) {
      r_log += format_validator_msg(level, source, position, message);
   };

   const spv_target_env target_env =
      convert_opencl_str_to_target_env(opencl_version);
   spvtools::SpirvTools spvTool(target_env);
   spvTool.SetMessageConsumer(validator_consumer);

   return spvTool.Validate(reinterpret_cast<const uint32_t *>(binary.data()),
                           binary.size() / 4u);
}

std::string
clover::spirv::print_module(const std::vector<char> &binary,
                            const std::string &opencl_version) {
   const spv_target_env target_env =
      convert_opencl_str_to_target_env(opencl_version);
   spvtools::SpirvTools spvTool(target_env);
   spv_context spvContext = spvContextCreate(target_env);
   if (!spvContext)
      return "Failed to create an spv_context for disassembling the module.";

   spv_text disassembly;
   spvBinaryToText(spvContext,
                   reinterpret_cast<const uint32_t *>(binary.data()),
                   binary.size() / 4u, SPV_BINARY_TO_TEXT_OPTION_NONE,
                   &disassembly, nullptr);
   spvContextDestroy(spvContext);

   const std::string disassemblyStr = disassembly->str;
   spvTextDestroy(disassembly);

   return disassemblyStr;
}

std::unordered_set<std::string>
clover::spirv::supported_extensions() {
   return {
      /* this is only a hint so all devices support that */
      "SPV_KHR_no_integer_wrap_decoration"
   };
}

std::vector<uint32_t>
clover::spirv::supported_versions() {
   return { make_spirv_version(1u, 0u) };
}

#else
bool
clover::spirv::is_valid_spirv(const std::vector<char> &/*binary*/,
                              const std::string &/*opencl_version*/,
                              std::string &/*r_log*/) {
   return false;
}

module
clover::spirv::compile_program(const std::vector<char> &binary,
                               const device &dev, std::string &r_log,
                               bool validate) {
   r_log += "SPIR-V support in clover is not enabled.\n";
   throw build_error();
}

module
clover::spirv::link_program(const std::vector<module> &/*modules*/,
                            const device &/*dev*/, const std::string &/*opts*/,
                            std::string &r_log) {
   r_log += "SPIR-V support in clover is not enabled.\n";
   throw error(CL_LINKER_NOT_AVAILABLE);
}

std::string
clover::spirv::print_module(const std::vector<char> &binary,
                            const std::string &opencl_version) {
   return std::string();
}

std::unordered_set<std::string>
clover::spirv::supported_extensions() {
   return {};
}

std::vector<uint32_t>
clover::spirv::supported_versions() {
   return {};
}
#endif
