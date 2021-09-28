cmake_minimum_required(VERSION 3.5)

set(HEVC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(HEVC_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")

if("${HEVC_ROOT}" STREQUAL "${HEVC_CONFIG_DIR}")
  message(
    FATAL_ERROR
      "Building from within the libhevc source tree is not supported.\n"
      "Hint: Run these commands\n"
      "$ rm -rf CMakeCache.txt CMakeFiles\n"
      "$ mkdir -p ./build\n"
      "$ cd ./build\n"
      "And re-run CMake from the build directory.")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
set(CMAKE_STATIC_LIBRARY_PREFIX "")

if(SANITIZE)
  string(TOLOWER ${SANITIZE} SANITIZE)

  set(CMAKE_SANITIZER_C_FLAGS "-fno-omit-frame-pointer -fsanitize=${SANITIZE}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_SANITIZER_C_FLAGS}")
endif()

list(APPEND LIBHEVCDEC_SRCS
            "${HEVC_ROOT}/common/ithread.c"
            "${HEVC_ROOT}/decoder/ihevcd_api.c"
            "${HEVC_ROOT}/common/ihevc_quant_tables.c"
            "${HEVC_ROOT}/common/ihevc_inter_pred_filters.c"
            "${HEVC_ROOT}/common/ihevc_weighted_pred.c"
            "${HEVC_ROOT}/common/ihevc_padding.c"
            "${HEVC_ROOT}/common/ihevc_deblk_edge_filter.c"
            "${HEVC_ROOT}/common/ihevc_deblk_tables.c"
            "${HEVC_ROOT}/common/ihevc_cabac_tables.c"
            "${HEVC_ROOT}/common/ihevc_common_tables.c"
            "${HEVC_ROOT}/common/ihevc_intra_pred_filters.c"
            "${HEVC_ROOT}/common/ihevc_chroma_intra_pred_filters.c"
            "${HEVC_ROOT}/common/ihevc_mem_fns.c"
            "${HEVC_ROOT}/common/ihevc_sao.c"
            "${HEVC_ROOT}/common/ihevc_trans_tables.c"
            "${HEVC_ROOT}/common/ihevc_recon.c"
            "${HEVC_ROOT}/common/ihevc_itrans.c"
            "${HEVC_ROOT}/common/ihevc_itrans_recon.c"
            "${HEVC_ROOT}/common/ihevc_iquant_recon.c"
            "${HEVC_ROOT}/common/ihevc_iquant_itrans_recon.c"
            "${HEVC_ROOT}/common/ihevc_itrans_recon_32x32.c"
            "${HEVC_ROOT}/common/ihevc_itrans_recon_16x16.c"
            "${HEVC_ROOT}/common/ihevc_itrans_recon_8x8.c"
            "${HEVC_ROOT}/common/ihevc_chroma_itrans_recon.c"
            "${HEVC_ROOT}/common/ihevc_chroma_iquant_recon.c"
            "${HEVC_ROOT}/common/ihevc_chroma_iquant_itrans_recon.c"
            "${HEVC_ROOT}/common/ihevc_chroma_recon.c"
            "${HEVC_ROOT}/common/ihevc_chroma_itrans_recon_16x16.c"
            "${HEVC_ROOT}/common/ihevc_chroma_itrans_recon_8x8.c"
            "${HEVC_ROOT}/common/ihevc_buf_mgr.c"
            "${HEVC_ROOT}/common/ihevc_disp_mgr.c"
            "${HEVC_ROOT}/common/ihevc_dpb_mgr.c"
            "${HEVC_ROOT}/common/ithread.c"
            "${HEVC_ROOT}/decoder/ihevcd_version.c"
            "${HEVC_ROOT}/decoder/ihevcd_api.c"
            "${HEVC_ROOT}/decoder/ihevcd_decode.c"
            "${HEVC_ROOT}/decoder/ihevcd_nal.c"
            "${HEVC_ROOT}/decoder/ihevcd_bitstream.c"
            "${HEVC_ROOT}/decoder/ihevcd_parse_headers.c"
            "${HEVC_ROOT}/decoder/ihevcd_parse_slice_header.c"
            "${HEVC_ROOT}/decoder/ihevcd_parse_slice.c"
            "${HEVC_ROOT}/decoder/ihevcd_parse_residual.c"
            "${HEVC_ROOT}/decoder/ihevcd_cabac.c"
            "${HEVC_ROOT}/decoder/ihevcd_intra_pred_mode_prediction.c"
            "${HEVC_ROOT}/decoder/ihevcd_process_slice.c"
            "${HEVC_ROOT}/decoder/ihevcd_utils.c"
            "${HEVC_ROOT}/decoder/ihevcd_job_queue.c"
            "${HEVC_ROOT}/decoder/ihevcd_ref_list.c"
            "${HEVC_ROOT}/decoder/ihevcd_get_mv.c"
            "${HEVC_ROOT}/decoder/ihevcd_mv_pred.c"
            "${HEVC_ROOT}/decoder/ihevcd_mv_merge.c"
            "${HEVC_ROOT}/decoder/ihevcd_iquant_itrans_recon_ctb.c"
            "${HEVC_ROOT}/decoder/ihevcd_itrans_recon_dc.c"
            "${HEVC_ROOT}/decoder/ihevcd_common_tables.c"
            "${HEVC_ROOT}/decoder/ihevcd_boundary_strength.c"
            "${HEVC_ROOT}/decoder/ihevcd_deblk.c"
            "${HEVC_ROOT}/decoder/ihevcd_inter_pred.c"
            "${HEVC_ROOT}/decoder/ihevcd_sao.c"
            "${HEVC_ROOT}/decoder/ihevcd_ilf_padding.c"
            "${HEVC_ROOT}/decoder/ihevcd_fmt_conv.c")

list(
  APPEND LIBHEVCDEC_X86_SRCS
         "${HEVC_ROOT}/decoder/x86/ihevcd_function_selector.c"
         "${HEVC_ROOT}/decoder/x86/ihevcd_function_selector_generic.c"
         "${HEVC_ROOT}/decoder/x86/ihevcd_function_selector_ssse3.c"
         "${HEVC_ROOT}/decoder/x86/ihevcd_function_selector_sse42.c"
         "${HEVC_ROOT}/common/x86/ihevc_inter_pred_filters_ssse3_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_weighted_pred_ssse3_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_intra_pred_filters_ssse3_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_chroma_intra_pred_filters_ssse3_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_itrans_recon_ssse3_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_itrans_recon_16x16_ssse3_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_itrans_recon_32x32_ssse3_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_sao_ssse3_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_deblk_ssse3_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_padding_ssse3_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_mem_fns_ssse3_intr.c"
         "${HEVC_ROOT}/decoder/x86/ihevcd_fmt_conv_ssse3_intr.c"
         "${HEVC_ROOT}/decoder/x86/ihevcd_it_rec_dc_ssse3_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_inter_pred_filters_sse42_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_weighted_pred_sse42_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_intra_pred_filters_sse42_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_chroma_intra_pred_filters_sse42_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_itrans_recon_sse42_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_16x16_itrans_recon_sse42_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_32x32_itrans_recon_sse42_intr.c"
         "${HEVC_ROOT}/decoder/x86/ihevcd_it_rec_dc_sse42_intr.c"
         "${HEVC_ROOT}/common/x86/ihevc_tables_x86_intr.c")

set(LIBHEVCDEC_INCLUDES ${HEVC_ROOT}/common ${HEVC_ROOT}/decoder)

set(LIBHEVCDEC_X86_C_FLAGS "-DX86 -DDISABLE_AVX2 -msse4.2 -mno-avx \
  -DDEFAULT_ARCH=D_ARCH_X86_SSE42 -DMULTICORE")
set(LIBHEVCDEC_X86_INCLUDES ${HEVC_ROOT}/common/x86 ${HEVC_ROOT}/decoder/x86)
set(LIBHEVCDEC_C_FLAGS "${LIBHEVCDEC_X86_C_FLAGS}")

include_directories(${LIBHEVCDEC_INCLUDES} ${LIBHEVCDEC_X86_INCLUDES})
add_library(libhevcdec ${LIBHEVCDEC_SRCS} ${LIBHEVCDEC_X86_SRCS})
set_target_properties(libhevcdec
                      PROPERTIES COMPILE_FLAGS "${LIBHEVCDEC_C_FLAGS}")

list(APPEND HEVCDEC_SRCS "${HEVC_ROOT}/test/decoder/main.c")

add_executable(hevcdec ${HEVCDEC_SRCS})
set_target_properties(hevcdec PROPERTIES COMPILE_FLAGS "-DMD5_DISABLE \
  -DPROFILE_ENABLE")
target_link_libraries(hevcdec libhevcdec Threads::Threads)
