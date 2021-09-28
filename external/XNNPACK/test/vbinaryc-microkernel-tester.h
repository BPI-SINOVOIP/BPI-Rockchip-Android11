// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <random>
#include <vector>

#include <xnnpack.h>
#include <xnnpack/params-init.h>
#include <xnnpack/params.h>


class VBinOpCMicrokernelTester {
 public:
  enum class OpType {
    AddC,
    DivC,
    RDivC,
    MaxC,
    MinC,
    MulC,
    SubC,
    RSubC,
  };

  enum class Variant {
    Native,
    Scalar,
  };

  inline VBinOpCMicrokernelTester& batch_size(size_t batch_size) {
    assert(batch_size != 0);
    this->batch_size_ = batch_size;
    return *this;
  }

  inline size_t batch_size() const {
    return this->batch_size_;
  }

  inline VBinOpCMicrokernelTester& inplace(bool inplace) {
    this->inplace_ = inplace;
    return *this;
  }

  inline bool inplace() const {
    return this->inplace_;
  }

  inline VBinOpCMicrokernelTester& qmin(uint8_t qmin) {
    this->qmin_ = qmin;
    return *this;
  }

  inline uint8_t qmin() const {
    return this->qmin_;
  }

  inline VBinOpCMicrokernelTester& qmax(uint8_t qmax) {
    this->qmax_ = qmax;
    return *this;
  }

  inline uint8_t qmax() const {
    return this->qmax_;
  }

  inline VBinOpCMicrokernelTester& iterations(size_t iterations) {
    this->iterations_ = iterations;
    return *this;
  }

  inline size_t iterations() const {
    return this->iterations_;
  }

  void Test(xnn_f32_vbinary_ukernel_function vbinaryc, OpType op_type, Variant variant = Variant::Native) const {
    std::random_device random_device;
    auto rng = std::mt19937(random_device());
    auto f32rng = std::bind(std::uniform_real_distribution<float>(0.0f, 1.0f), rng);

    std::vector<float> a(batch_size() + XNN_EXTRA_BYTES / sizeof(float));
    const float b = f32rng();
    std::vector<float> y(batch_size() + (inplace() ? XNN_EXTRA_BYTES / sizeof(float) : 0));
    std::vector<float> y_ref(batch_size());
    for (size_t iteration = 0; iteration < iterations(); iteration++) {
      std::generate(a.begin(), a.end(), std::ref(f32rng));
      if (inplace()) {
        std::generate(y.begin(), y.end(), std::ref(f32rng));
      } else {
        std::fill(y.begin(), y.end(), nanf(""));
      }
      const float* a_data = inplace() ? y.data() : a.data();

      // Compute reference results.
      for (size_t i = 0; i < batch_size(); i++) {
        switch (op_type) {
          case OpType::AddC:
            y_ref[i] = a_data[i] + b;
            break;
          case OpType::DivC:
            y_ref[i] = a_data[i] / b;
            break;
          case OpType::RDivC:
            y_ref[i] = b / a_data[i];
            break;
          case OpType::MaxC:
            y_ref[i] = std::max<float>(a_data[i], b);
            break;
          case OpType::MinC:
            y_ref[i] = std::min<float>(a_data[i], b);
            break;
          case OpType::MulC:
            y_ref[i] = a_data[i] * b;
            break;
          case OpType::SubC:
            y_ref[i] = a_data[i] - b;
            break;
          case OpType::RSubC:
            y_ref[i] = b - a_data[i];
            break;
        }
      }
      const float accumulated_min = *std::min_element(y_ref.cbegin(), y_ref.cend());
      const float accumulated_max = *std::max_element(y_ref.cbegin(), y_ref.cend());
      const float accumulated_range = accumulated_max - accumulated_min;
      const float y_max = accumulated_range > 0.0f ?
        (accumulated_max - accumulated_range / 255.0f * float(255 - qmax())) :
        +std::numeric_limits<float>::infinity();
      const float y_min = accumulated_range > 0.0f ?
        (accumulated_min + accumulated_range / 255.0f * float(qmin())) :
        -std::numeric_limits<float>::infinity();
      for (size_t i = 0; i < batch_size(); i++) {
        y_ref[i] = std::max<float>(std::min<float>(y_ref[i], y_max), y_min);
      }

      // Prepare output parameters.
      xnn_f32_output_params output_params = { };
      switch (variant) {
        case Variant::Native:
          output_params = xnn_init_f32_output_params(y_min, y_max);
          break;
        case Variant::Scalar:
          output_params = xnn_init_scalar_f32_output_params(y_min, y_max);
          break;
      }

      // Call optimized micro-kernel.
      vbinaryc(batch_size() * sizeof(float), a_data, &b, y.data(), &output_params);

      // Verify results.
      for (size_t i = 0; i < batch_size(); i++) {
        ASSERT_NEAR(y[i], y_ref[i], std::abs(y_ref[i]) * 1.0e-6f)
          << "at " << i << " / " << batch_size();
      }
    }
  }

 private:
  size_t batch_size_{1};
  bool inplace_{false};
  uint8_t qmin_{0};
  uint8_t qmax_{255};
  size_t iterations_{15};
};
