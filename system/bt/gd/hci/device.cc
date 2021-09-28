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
#include "hci/device.h"

using namespace bluetooth::hci;

std::string Device::generate_uid() {
  // TODO(optedoblivion): Figure out a good way to do this for what we want
  // to do
  // TODO(optedoblivion): Need to make a way to override something for LE pub addr case
  // Not sure if something like this is needed, but here is the idea (I think it came from mylesgw)
  // CLASSIC: have all 0s in front for classic then have private address
  // LE: have first public address in front then all 0s
  // LE: have first public address in front then private address
  //
  return address_.ToString();
}
