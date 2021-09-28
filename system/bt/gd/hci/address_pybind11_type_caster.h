/******************************************************************************
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#pragma once

#include <cstring>
#include <string>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "hci/address.h"

namespace py = pybind11;

namespace pybind11 {
namespace detail {
template <>
struct type_caster<::bluetooth::hci::Address> {
 public:
  // Set the Python name of Address and declare "value"
  PYBIND11_TYPE_CASTER(::bluetooth::hci::Address, _("Address"));

  // Convert from Python->C++
  bool load(handle src, bool) {
    PyObject* source = src.ptr();
    if (py::isinstance<py::str>(src)) {
      bool conversion_successful = bluetooth::hci::Address::FromString(PyUnicode_AsUTF8(source), value);
      return conversion_successful && !PyErr_Occurred();
    }
    return false;
  }

  // Convert from C++->Python
  static handle cast(bluetooth::hci::Address src, return_value_policy, handle) {
    return PyUnicode_FromString(src.ToString().c_str());
  }
};
}  // namespace detail
}  // namespace pybind11
