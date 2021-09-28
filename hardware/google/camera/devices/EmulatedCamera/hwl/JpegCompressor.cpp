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

//#define LOG_NDEBUG 0
#define LOG_TAG "JpegCompressor"

#include "JpegCompressor.h"

#include <cutils/properties.h>
#include <hardware/camera3.h>
#include <libyuv.h>
#include <utils/Log.h>
#include <utils/Trace.h>

namespace android {

using google_camera_hal::ErrorCode;
using google_camera_hal::MessageType;
using google_camera_hal::NotifyMessage;

JpegCompressor::JpegCompressor() {
  ATRACE_CALL();
  char value[PROPERTY_VALUE_MAX];
  if (property_get("ro.product.vendor.manufacturer", value, "unknown") <= 0) {
    ALOGW("%s: No Exif make data!", __FUNCTION__);
  }
  exif_make_ = std::string(value);

  if (property_get("ro.product.vendor.model", value, "unknown") <= 0) {
    ALOGW("%s: No Exif model data!", __FUNCTION__);
  }
  exif_model_ = std::string(value);

  jpeg_processing_thread_ = std::thread([this] { this->ThreadLoop(); });
}

JpegCompressor::~JpegCompressor() {
  ATRACE_CALL();

  // Abort the ongoing compression and flush any pending jobs
  jpeg_done_ = true;
  condition_.notify_one();
  jpeg_processing_thread_.join();
  while (!pending_yuv_jobs_.empty()) {
    auto job = std::move(pending_yuv_jobs_.front());
    job->output->stream_buffer.status = BufferStatus::kError;
    pending_yuv_jobs_.pop();
  }
}

status_t JpegCompressor::QueueYUV420(std::unique_ptr<JpegYUV420Job> job) {
  ATRACE_CALL();

  if ((job->input.get() == nullptr) || (job->output.get() == nullptr) ||
      (job->output->format != HAL_PIXEL_FORMAT_BLOB) ||
      (job->output->dataSpace != HAL_DATASPACE_V0_JFIF)) {
    ALOGE("%s: Unable to find buffers for JPEG source/destination",
          __FUNCTION__);
    return BAD_VALUE;
  }

  std::unique_lock<std::mutex> lock(mutex_);
  pending_yuv_jobs_.push(std::move(job));
  condition_.notify_one();

  return OK;
}

void JpegCompressor::ThreadLoop() {
  ATRACE_CALL();

  while (!jpeg_done_) {
    std::unique_ptr<JpegYUV420Job> current_yuv_job = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!pending_yuv_jobs_.empty()) {
        current_yuv_job = std::move(pending_yuv_jobs_.front());
        pending_yuv_jobs_.pop();
      }
    }

    if (current_yuv_job.get() != nullptr) {
      CompressYUV420(std::move(current_yuv_job));
    }

    std::unique_lock<std::mutex> lock(mutex_);
    auto ret = condition_.wait_for(lock, std::chrono::milliseconds(10));
    if (ret == std::cv_status::timeout) {
      ALOGV("%s: Jpeg thread timeout", __FUNCTION__);
    }
  }
}

void JpegCompressor::CompressYUV420(std::unique_ptr<JpegYUV420Job> job) {
  const uint8_t* app1_buffer = nullptr;
  size_t app1_buffer_size = 0;
  std::vector<uint8_t> thumbnail_jpeg_buffer;
  size_t encoded_thumbnail_size = 0;
  if ((job->exif_utils.get() != nullptr) &&
      (job->result_metadata.get() != nullptr)) {
    if (job->exif_utils->Initialize()) {
      camera_metadata_ro_entry_t entry;
      size_t thumbnail_width = 0;
      size_t thumbnail_height = 0;
      std::vector<uint8_t> thumb_yuv420_frame;
      YCbCrPlanes thumb_planes;
      auto ret = job->result_metadata->Get(ANDROID_JPEG_THUMBNAIL_SIZE, &entry);
      if ((ret == OK) && (entry.count == 2)) {
        thumbnail_width = entry.data.i32[0];
        thumbnail_height = entry.data.i32[1];
        if ((thumbnail_width > 0) && (thumbnail_height > 0)) {
          thumb_yuv420_frame.resize((thumbnail_width * thumbnail_height * 3) /
                                    2);
          thumb_planes = {
              .img_y = thumb_yuv420_frame.data(),
              .img_cb = thumb_yuv420_frame.data() +
                        thumbnail_width * thumbnail_height,
              .img_cr = thumb_yuv420_frame.data() +
                        (thumbnail_width * thumbnail_height * 5) / 4,
              .y_stride = static_cast<uint32_t>(thumbnail_width),
              .cbcr_stride = static_cast<uint32_t>(thumbnail_width) / 2};
          // TODO: Crop thumbnail according to documentation
          auto stat = I420Scale(
              job->input->yuv_planes.img_y, job->input->yuv_planes.y_stride,
              job->input->yuv_planes.img_cb, job->input->yuv_planes.cbcr_stride,
              job->input->yuv_planes.img_cr, job->input->yuv_planes.cbcr_stride,
              job->input->width, job->input->height, thumb_planes.img_y,
              thumb_planes.y_stride, thumb_planes.img_cb,
              thumb_planes.cbcr_stride, thumb_planes.img_cr,
              thumb_planes.cbcr_stride, thumbnail_width, thumbnail_height,
              libyuv::kFilterNone);
          if (stat != 0) {
            ALOGE("%s: Failed during thumbnail scaling: %d", __FUNCTION__, stat);
            thumb_yuv420_frame.clear();
          }
        }
      }

      if (job->exif_utils->SetFromMetadata(
              *job->result_metadata, job->input->width, job->input->height)) {
        if (!thumb_yuv420_frame.empty()) {
          thumbnail_jpeg_buffer.resize(64 * 1024);  // APP1 is limited by 64k
          encoded_thumbnail_size = CompressYUV420Frame(
              {.output_buffer = thumbnail_jpeg_buffer.data(),
               .output_buffer_size = thumbnail_jpeg_buffer.size(),
               .yuv_planes = thumb_planes,
               .width = thumbnail_width,
               .height = thumbnail_height,
               .app1_buffer = nullptr,
               .app1_buffer_size = 0});
          if (encoded_thumbnail_size > 0) {
            job->output->stream_buffer.status = BufferStatus::kOk;
          } else {
            ALOGE("%s: Failed encoding thumbail!", __FUNCTION__);
            thumbnail_jpeg_buffer.clear();
          }
        }

        job->exif_utils->SetMake(exif_make_);
        job->exif_utils->SetModel(exif_model_);
        if (job->exif_utils->GenerateApp1(thumbnail_jpeg_buffer.empty()
                                              ? nullptr
                                              : thumbnail_jpeg_buffer.data(),
                                          encoded_thumbnail_size)) {
          app1_buffer = job->exif_utils->GetApp1Buffer();
          app1_buffer_size = job->exif_utils->GetApp1Length();
        } else {
          ALOGE("%s: Unable to generate App1 buffer", __FUNCTION__);
        }
      } else {
        ALOGE("%s: Unable to generate EXIF section!", __FUNCTION__);
      }
    } else {
      ALOGE("%s: Unable to initialize Exif generator!", __FUNCTION__);
    }
  }

  auto encoded_size = CompressYUV420Frame(
      {.output_buffer = job->output->plane.img.img,
       .output_buffer_size = job->output->plane.img.buffer_size,
       .yuv_planes = job->input->yuv_planes,
       .width = job->input->width,
       .height = job->input->height,
       .app1_buffer = app1_buffer,
       .app1_buffer_size = app1_buffer_size});
  if (encoded_size > 0) {
    job->output->stream_buffer.status = BufferStatus::kOk;
  } else {
    job->output->stream_buffer.status = BufferStatus::kError;
    return;
  }

  auto jpeg_header_offset =
      job->output->plane.img.buffer_size - sizeof(struct camera3_jpeg_blob);
  if (jpeg_header_offset > encoded_size) {
    struct camera3_jpeg_blob* blob =
        reinterpret_cast<struct camera3_jpeg_blob*>(job->output->plane.img.img +
                                                    jpeg_header_offset);
    blob->jpeg_blob_id = CAMERA3_JPEG_BLOB_ID;
    blob->jpeg_size = encoded_size;
  } else {
    ALOGW("%s: No space for jpeg header at offset: %u and jpeg size: %u",
          __FUNCTION__, static_cast<unsigned>(jpeg_header_offset),
          static_cast<unsigned>(encoded_size));
  }
}

size_t JpegCompressor::CompressYUV420Frame(YUV420Frame frame) {
  ATRACE_CALL();

  struct CustomJpegDestMgr : public jpeg_destination_mgr {
    JOCTET* buffer;
    size_t buffer_size;
    size_t encoded_size;
    bool success;
  } dmgr;

  // Set up error management
  jpeg_error_info_ = NULL;
  jpeg_error_mgr jerr;

  auto cinfo = std::make_unique<jpeg_compress_struct>();
  cinfo->err = jpeg_std_error(&jerr);
  cinfo->err->error_exit = [](j_common_ptr cinfo) {
    (*cinfo->err->output_message)(cinfo);
    if (cinfo->client_data) {
      auto& dmgr = *static_cast<CustomJpegDestMgr*>(cinfo->client_data);
      dmgr.success = false;
    }
  };

  jpeg_create_compress(cinfo.get());
  if (CheckError("Error initializing compression")) {
    return 0;
  }

  dmgr.buffer = static_cast<JOCTET*>(frame.output_buffer);
  dmgr.buffer_size = frame.output_buffer_size;
  dmgr.encoded_size = 0;
  dmgr.success = true;
  cinfo->client_data = static_cast<void*>(&dmgr);
  dmgr.init_destination = [](j_compress_ptr cinfo) {
    auto& dmgr = static_cast<CustomJpegDestMgr&>(*cinfo->dest);
    dmgr.next_output_byte = dmgr.buffer;
    dmgr.free_in_buffer = dmgr.buffer_size;
    ALOGV("%s:%d jpeg start: %p [%zu]", __FUNCTION__, __LINE__, dmgr.buffer,
          dmgr.buffer_size);
  };

  dmgr.empty_output_buffer = [](j_compress_ptr cinfo __unused) {
    ALOGE("%s:%d Out of buffer", __FUNCTION__, __LINE__);
    return 0;
  };

  dmgr.term_destination = [](j_compress_ptr cinfo) {
    auto& dmgr = static_cast<CustomJpegDestMgr&>(*cinfo->dest);
    dmgr.encoded_size = dmgr.buffer_size - dmgr.free_in_buffer;
    ALOGV("%s:%d Done with jpeg: %zu", __FUNCTION__, __LINE__,
          dmgr.encoded_size);
  };

  cinfo->dest = reinterpret_cast<struct jpeg_destination_mgr*>(&dmgr);

  // Set up compression parameters
  cinfo->image_width = frame.width;
  cinfo->image_height = frame.height;
  cinfo->input_components = 3;
  cinfo->in_color_space = JCS_YCbCr;

  jpeg_set_defaults(cinfo.get());
  if (CheckError("Error configuring defaults")) {
    return 0;
  }

  jpeg_set_colorspace(cinfo.get(), JCS_YCbCr);
  if (CheckError("Error configuring color space")) {
    return 0;
  }

  cinfo->raw_data_in = 1;
  // YUV420 planar with chroma subsampling
  cinfo->comp_info[0].h_samp_factor = 2;
  cinfo->comp_info[0].v_samp_factor = 2;
  cinfo->comp_info[1].h_samp_factor = 1;
  cinfo->comp_info[1].v_samp_factor = 1;
  cinfo->comp_info[2].h_samp_factor = 1;
  cinfo->comp_info[2].v_samp_factor = 1;

  int max_vsamp_factor = std::max({cinfo->comp_info[0].v_samp_factor,
                                   cinfo->comp_info[1].v_samp_factor,
                                   cinfo->comp_info[2].v_samp_factor});
  int c_vsub_sampling =
      cinfo->comp_info[0].v_samp_factor / cinfo->comp_info[1].v_samp_factor;

  // Start compression
  jpeg_start_compress(cinfo.get(), TRUE);
  if (CheckError("Error starting compression")) {
    return 0;
  }

  if ((frame.app1_buffer != nullptr) && (frame.app1_buffer_size > 0)) {
    jpeg_write_marker(cinfo.get(), JPEG_APP0 + 1,
                      static_cast<const JOCTET*>(frame.app1_buffer),
                      frame.app1_buffer_size);
  }

  // Compute our macroblock height, so we can pad our input to be vertically
  // macroblock aligned.

  size_t mcu_v = DCTSIZE * max_vsamp_factor;
  size_t padded_height = mcu_v * ((cinfo->image_height + mcu_v - 1) / mcu_v);

  std::vector<JSAMPROW> y_lines(padded_height);
  std::vector<JSAMPROW> cb_lines(padded_height / c_vsub_sampling);
  std::vector<JSAMPROW> cr_lines(padded_height / c_vsub_sampling);

  uint8_t* py = static_cast<uint8_t*>(frame.yuv_planes.img_y);
  uint8_t* pcr = static_cast<uint8_t*>(frame.yuv_planes.img_cr);
  uint8_t* pcb = static_cast<uint8_t*>(frame.yuv_planes.img_cb);

  for (uint32_t i = 0; i < padded_height; i++) {
    /* Once we are in the padding territory we still point to the last line
     * effectively replicating it several times ~ CLAMP_TO_EDGE */
    int li = std::min(i, cinfo->image_height - 1);
    y_lines[i] = static_cast<JSAMPROW>(py + li * frame.yuv_planes.y_stride);
    if (i < padded_height / c_vsub_sampling) {
      li = std::min(i, (cinfo->image_height - 1) / c_vsub_sampling);
      cr_lines[i] =
          static_cast<JSAMPROW>(pcr + li * frame.yuv_planes.cbcr_stride);
      cb_lines[i] =
          static_cast<JSAMPROW>(pcb + li * frame.yuv_planes.cbcr_stride);
    }
  }

  const uint32_t batch_size = DCTSIZE * max_vsamp_factor;
  while (cinfo->next_scanline < cinfo->image_height) {
    JSAMPARRAY planes[3]{&y_lines[cinfo->next_scanline],
                         &cb_lines[cinfo->next_scanline / c_vsub_sampling],
                         &cr_lines[cinfo->next_scanline / c_vsub_sampling]};

    jpeg_write_raw_data(cinfo.get(), planes, batch_size);
    if (CheckError("Error while compressing")) {
      return 0;
    }

    if (jpeg_done_) {
      ALOGV("%s: Cancel called, exiting early", __FUNCTION__);
      jpeg_finish_compress(cinfo.get());
      return 0;
    }
  }

  jpeg_finish_compress(cinfo.get());
  if (CheckError("Error while finishing compression")) {
    return 0;
  }

  return dmgr.encoded_size;
}

bool JpegCompressor::CheckError(const char* msg) {
  if (jpeg_error_info_) {
    char err_buffer[JMSG_LENGTH_MAX];
    jpeg_error_info_->err->format_message(jpeg_error_info_, err_buffer);
    ALOGE("%s: %s: %s", __FUNCTION__, msg, err_buffer);
    jpeg_error_info_ = NULL;
    return true;
  }

  return false;
}

}  // namespace android
