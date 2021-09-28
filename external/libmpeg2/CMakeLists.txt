cmake_minimum_required(VERSION 3.5)

set(MPEG2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(MPEG2_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")

if("${MPEG2_ROOT}" STREQUAL "${MPEG2_CONFIG_DIR}")
  message(
    FATAL_ERROR
      "Building from within the libmpeg2 source tree is not supported.\n"
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

list(APPEND LIBMPEG2DEC_SRCS
            "${MPEG2_ROOT}/common/impeg2_buf_mgr.c"
            "${MPEG2_ROOT}/common/impeg2_disp_mgr.c"
            "${MPEG2_ROOT}/common/impeg2_format_conv.c"
            "${MPEG2_ROOT}/common/impeg2_globals.c"
            "${MPEG2_ROOT}/common/impeg2_idct.c"
            "${MPEG2_ROOT}/common/impeg2_inter_pred.c"
            "${MPEG2_ROOT}/common/impeg2_job_queue.c"
            "${MPEG2_ROOT}/common/impeg2_mem_func.c"
            "${MPEG2_ROOT}/common/ithread.c"
            "${MPEG2_ROOT}/decoder/impeg2d_api_main.c"
            "${MPEG2_ROOT}/decoder/impeg2d_bitstream.c"
            "${MPEG2_ROOT}/decoder/impeg2d_debug.c"
            "${MPEG2_ROOT}/decoder/impeg2d_dec_hdr.c"
            "${MPEG2_ROOT}/decoder/impeg2d_decoder.c"
            "${MPEG2_ROOT}/decoder/impeg2d_d_pic.c"
            "${MPEG2_ROOT}/decoder/impeg2d_function_selector_generic.c"
            "${MPEG2_ROOT}/decoder/impeg2d_globals.c"
            "${MPEG2_ROOT}/decoder/impeg2d_i_pic.c"
            "${MPEG2_ROOT}/decoder/impeg2d_mc.c"
            "${MPEG2_ROOT}/decoder/impeg2d_mv_dec.c"
            "${MPEG2_ROOT}/decoder/impeg2d_pic_proc.c"
            "${MPEG2_ROOT}/decoder/impeg2d_pnb_pic.c"
            "${MPEG2_ROOT}/decoder/impeg2d_vld.c"
            "${MPEG2_ROOT}/decoder/impeg2d_vld_tables.c"
            "${MPEG2_ROOT}/decoder/impeg2d_deinterlace.c"
            "${MPEG2_ROOT}/common/icv_sad.c"
            "${MPEG2_ROOT}/common/icv_variance.c"
            "${MPEG2_ROOT}/common/ideint.c"
            "${MPEG2_ROOT}/common/ideint_cac.c"
            "${MPEG2_ROOT}/common/ideint_debug.c"
            "${MPEG2_ROOT}/common/ideint_function_selector_generic.c"
            "${MPEG2_ROOT}/common/ideint_utils.c")

list(APPEND LIBMPEG2DEC_X86_SRCS
            "${MPEG2_ROOT}/decoder/x86/impeg2d_function_selector.c"
            "${MPEG2_ROOT}/decoder/x86/impeg2d_function_selector_avx2.c"
            "${MPEG2_ROOT}/decoder/x86/impeg2d_function_selector_ssse3.c"
            "${MPEG2_ROOT}/decoder/x86/impeg2d_function_selector_sse42.c"
            "${MPEG2_ROOT}/common/x86/ideint_function_selector.c"
            "${MPEG2_ROOT}/common/x86/ideint_function_selector_ssse3.c"
            "${MPEG2_ROOT}/common/x86/ideint_function_selector_sse42.c"
            "${MPEG2_ROOT}/common/x86/icv_variance_ssse3.c"
            "${MPEG2_ROOT}/common/x86/icv_sad_ssse3.c"
            "${MPEG2_ROOT}/common/x86/ideint_cac_ssse3.c"
            "${MPEG2_ROOT}/common/x86/ideint_spatial_filter_ssse3.c"
            "${MPEG2_ROOT}/common/x86/impeg2_idct_recon_sse42_intr.c"
            "${MPEG2_ROOT}/common/x86/impeg2_inter_pred_sse42_intr.c"
            "${MPEG2_ROOT}/common/x86/impeg2_mem_func_sse42_intr.c")

set(LIBMPEG2DEC_INCLUDES ${MPEG2_ROOT}/common ${MPEG2_ROOT}/decoder)

set(LIBMPEG2DEC_X86_C_FLAGS "-DX86 -DDISABLE_AVX2 -msse4.2 -mno-avx \
  -DDEFAULT_ARCH=D_ARCH_X86_SSE42 -DMULTICORE")
set(LIBMPEG2DEC_X86_INCLUDES ${MPEG2_ROOT}/common/x86 ${MPEG2_ROOT}/decoder/x86)
set(LIBMPEG2DEC_C_FLAGS "${LIBMPEG2DEC_X86_C_FLAGS}")

include_directories(${LIBMPEG2DEC_INCLUDES} ${LIBMPEG2DEC_X86_INCLUDES})
add_library(libmpeg2dec ${LIBMPEG2DEC_SRCS} ${LIBMPEG2DEC_X86_SRCS})
set_target_properties(libmpeg2dec
                      PROPERTIES COMPILE_FLAGS "${LIBMPEG2DEC_C_FLAGS}")

list(APPEND MPEG2DEC_SRCS "${MPEG2_ROOT}/test/decoder/main.c")

add_executable(mpeg2dec ${MPEG2DEC_SRCS})
set_target_properties(mpeg2dec
                      PROPERTIES COMPILE_FLAGS "-DMD5_DISABLE -DPROFILE_ENABLE")
target_link_libraries(mpeg2dec libmpeg2dec Threads::Threads)
