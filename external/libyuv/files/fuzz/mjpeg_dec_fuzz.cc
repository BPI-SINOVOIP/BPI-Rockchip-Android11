// -----------------------------------------------------------------------------
// Fuzz Target for libyuv's mjpeg decoder.
//
// This fuzz target focuses on the decoding from JPEG to YUV format.
// -----------------------------------------------------------------------------
#include "libyuv/basic_types.h"
#include "libyuv/mjpeg_decoder.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>


// -----------------------------------------------------------------------------
// Checks whether 3 values are equal.
//
inline bool IsEqual(int a, int b, int  c) {
  return (a == b && a == c);
}

// -----------------------------------------------------------------------------
// libFuzzer's callback that is invoked upon startup.
//
extern "C" int LLVMFuzzerInitialize(int *unused_argc, char ***unused_argv) {
  (void) unused_argc;  // Avoid "-Wunused-parameter" warnings.
  (void) unused_argv;
  // Printing this message is benefial as we can infer which fuzzer runs
  // just by looking at the logs which are stored in the cloud.
  printf("[*] Fuzz Target for libyuv mjpeg decoder started.\n");

  return 0;
}

// -----------------------------------------------------------------------------
// Decodes a JPEG image into a YUV format.
//
extern "C" bool Decode(libyuv::MJpegDecoder &decoder) {
  // YUV colors are represented with one "luminance" component called Y
  // and two "chrominance" components, called U and V.
  // Planar formats use separate matrices for each of the 3 color components.
  //
  // If we don't have 3 components abort.
  //
  // NOTE: It may be possible to have 4 planes for CMYK and alpha, but it's
  // very rare and not supported.
  int num_planes = decoder.GetNumComponents();

  if (num_planes != 3) {
    return false;
  }

  /* NOTE: Without a jpeg corpus, we can't reach this point */

  int width = decoder.GetWidth();
  int height = decoder.GetHeight();
  int y_width = decoder.GetComponentWidth(0);
  int y_height = decoder.GetComponentHeight(0);
  int u_width = decoder.GetComponentWidth(1);
  int u_height = decoder.GetComponentHeight(1);
  int v_width = decoder.GetComponentWidth(2);
  int v_height = decoder.GetComponentHeight(2);
  uint8_t *y;
  uint8_t *u;
  uint8_t *v;

  // Make sure that width and heigh stay at decent levels (< 16K * 16K).
  // (Y is the largest buffer).
  if (width > (1 << 14) || height > (1 << 14)) {
    // Ok, if this happens it's a DoS, but let's ignore it for now.
    return false;
  }

  // Allocate stides according to the sampling type.
  if (IsEqual(y_width, u_width, v_width) &&
      IsEqual(y_height, u_height, v_height)) {
    // Sampling type: YUV444.
    y = new uint8_t[width * height];
    u = new uint8_t[width * height];
    v = new uint8_t[width * height];

  } else if (IsEqual((y_width + 1) / 2, u_width, v_width) &&
      IsEqual(y_height, u_height, v_height)) {
    // Sampling type: YUV422.
    y = new uint8_t[width * height];
    u = new uint8_t[((width + 1) / 2) * height];
    v = new uint8_t[((width + 1) / 2) * height];

  } else if (IsEqual((y_width + 1) / 2, u_width, v_width) &&
      IsEqual((y_height + 1) / 2, u_height, v_height)) {
    // Sampling type: YUV420.
    y = new uint8_t[width * height];
    u = new uint8_t[((width + 1) / 2) * ((height + 1) / 2)];
    v = new uint8_t[((width + 1) / 2) * ((height + 1) / 2)];

  } else {
    // Invalid sampling type.
    return false;
  }

  uint8_t* planes[] = {y, u, v};

  // Do the actual decoding. (Ignore return values).
  decoder.DecodeToBuffers(planes, width, height);

  delete[] y;
  delete[] u;
  delete[] v;

  return true;  // Success!
}

// -----------------------------------------------------------------------------
// libFuzzer's callback that performs the actual fuzzing.
//
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Make sure that we have a minimum length (32 or something small).
  if (size < 32) {
      return 0;
  }

  // Create the decoder object.
  libyuv::MJpegDecoder decoder;

  // Load frame, read its headers and determine uncompress image format.
  if (decoder.LoadFrame(data, size) == LIBYUV_FALSE) {
    // Header parsing error. Discrad frame.
    return 0;
  }

  // Do the actual decoding.
  Decode(decoder);

  // Unload the frame.
  decoder.UnloadFrame();

  return 0;
}
