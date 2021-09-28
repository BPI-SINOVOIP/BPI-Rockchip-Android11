cmake_minimum_required(VERSION 3.5)

set(AVC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(AVC_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")

if("${AVC_ROOT}" STREQUAL "${AVC_CONFIG_DIR}")
  message(
    FATAL_ERROR
      "Building from within the libavc source tree is not supported.\n"
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

list(APPEND LIBAVCDEC_SRCS
            "${AVC_ROOT}/common/ih264_buf_mgr.c"
            "${AVC_ROOT}/common/ih264_disp_mgr.c"
            "${AVC_ROOT}/common/ih264_inter_pred_filters.c"
            "${AVC_ROOT}/common/ih264_luma_intra_pred_filters.c"
            "${AVC_ROOT}/common/ih264_chroma_intra_pred_filters.c"
            "${AVC_ROOT}/common/ih264_padding.c"
            "${AVC_ROOT}/common/ih264_mem_fns.c"
            "${AVC_ROOT}/common/ih264_deblk_edge_filters.c"
            "${AVC_ROOT}/common/ih264_iquant_itrans_recon.c"
            "${AVC_ROOT}/common/ih264_ihadamard_scaling.c"
            "${AVC_ROOT}/common/ih264_weighted_pred.c"
            "${AVC_ROOT}/common/ithread.c"
            "${AVC_ROOT}/decoder/ih264d_cabac.c"
            "${AVC_ROOT}/decoder/ih264d_parse_mb_header.c"
            "${AVC_ROOT}/decoder/ih264d_parse_cabac.c"
            "${AVC_ROOT}/decoder/ih264d_process_intra_mb.c"
            "${AVC_ROOT}/decoder/ih264d_inter_pred.c"
            "${AVC_ROOT}/decoder/ih264d_parse_bslice.c"
            "${AVC_ROOT}/decoder/ih264d_parse_pslice.c"
            "${AVC_ROOT}/decoder/ih264d_parse_islice.c"
            "${AVC_ROOT}/decoder/ih264d_cabac_init_tables.c"
            "${AVC_ROOT}/decoder/ih264d_bitstrm.c"
            "${AVC_ROOT}/decoder/ih264d_compute_bs.c"
            "${AVC_ROOT}/decoder/ih264d_deblocking.c"
            "${AVC_ROOT}/decoder/ih264d_parse_headers.c"
            "${AVC_ROOT}/decoder/ih264d_mb_utils.c"
            "${AVC_ROOT}/decoder/ih264d_mvpred.c"
            "${AVC_ROOT}/decoder/ih264d_utils.c"
            "${AVC_ROOT}/decoder/ih264d_process_bslice.c"
            "${AVC_ROOT}/decoder/ih264d_process_pslice.c"
            "${AVC_ROOT}/decoder/ih264d_parse_slice.c"
            "${AVC_ROOT}/decoder/ih264d_quant_scaling.c"
            "${AVC_ROOT}/decoder/ih264d_parse_cavlc.c"
            "${AVC_ROOT}/decoder/ih264d_dpb_mgr.c"
            "${AVC_ROOT}/decoder/ih264d_nal.c"
            "${AVC_ROOT}/decoder/ih264d_sei.c"
            "${AVC_ROOT}/decoder/ih264d_tables.c"
            "${AVC_ROOT}/decoder/ih264d_vui.c"
            "${AVC_ROOT}/decoder/ih264d_format_conv.c"
            "${AVC_ROOT}/decoder/ih264d_thread_parse_decode.c"
            "${AVC_ROOT}/decoder/ih264d_api.c"
            "${AVC_ROOT}/decoder/ih264d_thread_compute_bs.c"
            "${AVC_ROOT}/decoder/ih264d_function_selector_generic.c")

list(APPEND LIBAVCDEC_X86_SRCS
            "${AVC_ROOT}/decoder/x86/ih264d_function_selector.c"
            "${AVC_ROOT}/decoder/x86/ih264d_function_selector_sse42.c"
            "${AVC_ROOT}/decoder/x86/ih264d_function_selector_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_inter_pred_filters_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_deblk_luma_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_deblk_chroma_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_padding_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_mem_fns_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_iquant_itrans_recon_dc_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_iquant_itrans_recon_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_luma_intra_pred_filters_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_chroma_intra_pred_filters_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_iquant_itrans_recon_sse42.c"
            "${AVC_ROOT}/common/x86/ih264_weighted_pred_sse42.c"
            "${AVC_ROOT}/common/x86/ih264_ihadamard_scaling_sse42.c")

set(LIBAVCDEC_INCLUDES ${AVC_ROOT}/common ${AVC_ROOT}/decoder)

set(LIBAVCDEC_X86_C_FLAGS
    "-DX86 -DDISABLE_AVX2 -msse4.2 -mno-avx -DDEFAULT_ARCH=D_ARCH_X86_SSE42")
set(LIBAVCDEC_X86_INCLUDES ${AVC_ROOT}/common/x86 ${AVC_ROOT}/decoder/x86)
set(LIBAVCDEC_C_FLAGS "${LIBAVCDEC_X86_C_FLAGS}")

include_directories(${LIBAVCDEC_INCLUDES} ${LIBAVCDEC_X86_INCLUDES})
add_library(libavcdec ${LIBAVCDEC_SRCS} ${LIBAVCDEC_X86_SRCS})
set_target_properties(libavcdec PROPERTIES COMPILE_FLAGS "${LIBAVCDEC_C_FLAGS}")

list(APPEND AVCDEC_SRCS "${AVC_ROOT}/test/decoder/main.c")

add_executable(avcdec ${AVCDEC_SRCS})
set_target_properties(avcdec
                      PROPERTIES COMPILE_FLAGS "-DMD5_DISABLE -DPROFILE_ENABLE")
target_link_libraries(avcdec libavcdec Threads::Threads)

list(APPEND LIBAVCENC_SRCS
            "${AVC_ROOT}/common/ih264_resi_trans_quant.c"
            "${AVC_ROOT}/common/ih264_iquant_itrans_recon.c"
            "${AVC_ROOT}/common/ih264_ihadamard_scaling.c"
            "${AVC_ROOT}/common/ih264_inter_pred_filters.c"
            "${AVC_ROOT}/common/ih264_luma_intra_pred_filters.c"
            "${AVC_ROOT}/common/ih264_chroma_intra_pred_filters.c"
            "${AVC_ROOT}/common/ih264_padding.c"
            "${AVC_ROOT}/common/ih264_mem_fns.c"
            "${AVC_ROOT}/common/ih264_deblk_edge_filters.c"
            "${AVC_ROOT}/common/ih264_deblk_tables.c"
            "${AVC_ROOT}/common/ih264_cavlc_tables.c"
            "${AVC_ROOT}/common/ih264_cabac_tables.c"
            "${AVC_ROOT}/common/ih264_common_tables.c"
            "${AVC_ROOT}/common/ih264_trans_data.c"
            "${AVC_ROOT}/common/ih264_buf_mgr.c"
            "${AVC_ROOT}/common/ih264_dpb_mgr.c"
            "${AVC_ROOT}/common/ih264_list.c"
            "${AVC_ROOT}/common/ithread.c"
            "${AVC_ROOT}/encoder/ih264e_globals.c"
            "${AVC_ROOT}/encoder/ih264e_intra_modes_eval.c"
            "${AVC_ROOT}/encoder/ih264e_half_pel.c"
            "${AVC_ROOT}/encoder/ih264e_mc.c"
            "${AVC_ROOT}/encoder/ih264e_me.c"
            "${AVC_ROOT}/encoder/ih264e_rc_mem_interface.c"
            "${AVC_ROOT}/encoder/ih264e_time_stamp.c"
            "${AVC_ROOT}/encoder/ih264e_modify_frm_rate.c"
            "${AVC_ROOT}/encoder/ih264e_rate_control.c"
            "${AVC_ROOT}/encoder/ih264e_core_coding.c"
            "${AVC_ROOT}/encoder/ih264e_deblk.c"
            "${AVC_ROOT}/encoder/ih264e_api.c"
            "${AVC_ROOT}/encoder/ih264e_process.c"
            "${AVC_ROOT}/encoder/ih264e_encode.c"
            "${AVC_ROOT}/encoder/ih264e_utils.c"
            "${AVC_ROOT}/encoder/ih264e_version.c"
            "${AVC_ROOT}/encoder/ih264e_bitstream.c"
            "${AVC_ROOT}/encoder/ih264e_cavlc.c"
            "${AVC_ROOT}/encoder/ih264e_cabac_init.c"
            "${AVC_ROOT}/encoder/ih264e_cabac.c"
            "${AVC_ROOT}/encoder/ih264e_cabac_encode.c"
            "${AVC_ROOT}/encoder/ih264e_encode_header.c"
            "${AVC_ROOT}/encoder/ih264e_function_selector_generic.c"
            "${AVC_ROOT}/encoder/ih264e_fmt_conv.c"
            "${AVC_ROOT}/encoder/ih264e_sei.c"
            "${AVC_ROOT}/encoder/irc_rate_control_api.c"
            "${AVC_ROOT}/encoder/irc_bit_allocation.c"
            "${AVC_ROOT}/encoder/irc_cbr_buffer_control.c"
            "${AVC_ROOT}/encoder/irc_est_sad.c"
            "${AVC_ROOT}/encoder/irc_fixed_point_error_bits.c"
            "${AVC_ROOT}/encoder/irc_frame_info_collector.c"
            "${AVC_ROOT}/encoder/irc_mb_model_based.c"
            "${AVC_ROOT}/encoder/irc_picture_type.c"
            "${AVC_ROOT}/encoder/irc_rd_model.c"
            "${AVC_ROOT}/encoder/irc_vbr_storage_vbv.c"
            "${AVC_ROOT}/encoder/irc_vbr_str_prms.c"
            "${AVC_ROOT}/encoder/ime.c"
            "${AVC_ROOT}/encoder/ime_distortion_metrics.c")

list(APPEND LIBAVCENC_X86_SRCS
            "${AVC_ROOT}/encoder/x86/ih264e_function_selector.c"
            "${AVC_ROOT}/encoder/x86/ih264e_function_selector_sse42.c"
            "${AVC_ROOT}/encoder/x86/ih264e_function_selector_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_iquant_itrans_recon_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_iquant_itrans_recon_dc_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_ihadamard_scaling_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_inter_pred_filters_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_mem_fns_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_padding_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_luma_intra_pred_filters_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_chroma_intra_pred_filters_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_deblk_chroma_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_deblk_luma_ssse3.c"
            "${AVC_ROOT}/common/x86/ih264_iquant_itrans_recon_sse42.c"
            "${AVC_ROOT}/common/x86/ih264_ihadamard_scaling_sse42.c"
            "${AVC_ROOT}/common/x86/ih264_resi_trans_quant_sse42.c"
            "${AVC_ROOT}/common/x86/ih264_weighted_pred_sse42.c"
            "${AVC_ROOT}/encoder/x86/ih264e_half_pel_ssse3.c"
            "${AVC_ROOT}/encoder/x86/ih264e_intra_modes_eval_ssse3.c"
            "${AVC_ROOT}/encoder/x86/ime_distortion_metrics_sse42.c")

set(LIBAVCENC_INCLUDES ${AVC_ROOT}/common ${AVC_ROOT}/encoder)

set(LIBAVCENC_X86_C_FLAGS "-msse4.2 -mno-avx -UHP_PL -DN_MB_ENABLE")
set(LIBAVCENC_C_FLAGS "${LIBAVCENC_X86_C_FLAGS}")
set(LIBAVCENC_X86_INCLUDES ${AVC_ROOT}/common/x86 ${AVC_ROOT}/encoder/x86)

include_directories(${LIBAVCENC_INCLUDES} ${LIBAVCENC_X86_INCLUDES})
add_library(libavcenc ${LIBAVCENC_SRCS} ${LIBAVCENC_X86_SRCS})
set_target_properties(libavcenc PROPERTIES COMPILE_FLAGS "${LIBAVCENC_C_FLAGS}")

list(APPEND AVCENC_SRCS
            "${AVC_ROOT}/test/encoder/main.c"
            "${AVC_ROOT}/test/encoder/psnr.c"
            "${AVC_ROOT}/test/encoder/input.c"
            "${AVC_ROOT}/test/encoder/output.c"
            "${AVC_ROOT}/test/encoder/recon.c")

add_executable(avcenc ${AVCENC_SRCS})
set_target_properties(avcenc
                      PROPERTIES COMPILE_FLAGS
                                 "-DARM -DMD5_DISABLE -DPROFILE_ENABLE")
target_link_libraries(avcenc
                      libavcenc
                      Threads::Threads
                      m)
