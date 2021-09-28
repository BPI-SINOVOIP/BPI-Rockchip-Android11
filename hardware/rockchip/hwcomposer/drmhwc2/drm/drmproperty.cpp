/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "drmproperty.h"
#include "drmdevice.h"

#include <errno.h>
#include <stdint.h>
#include <string>

#include <xf86drmMode.h>

namespace android {

DrmProperty::DrmPropertyEnum::DrmPropertyEnum(drm_mode_property_enum *e)
    : value_(e->value), name_(e->name) {
}

DrmProperty::DrmPropertyEnum::~DrmPropertyEnum() {
}

DrmProperty::DrmProperty(drmModePropertyPtr p, uint64_t value)
    : id_(0), type_(DRM_PROPERTY_TYPE_INVALID), flags_(0), name_("") {
  Init(p, value);
}

void DrmProperty::Init(drmModePropertyPtr p, uint64_t value) {
  id_ = p->prop_id;
  flags_ = p->flags;
  name_ = p->name;
  value_ = value;

  for (int i = 0; i < p->count_values; ++i)
    values_.push_back(p->values[i]);

  for (int i = 0; i < p->count_enums; ++i)
    enums_.push_back(DrmPropertyEnum(&p->enums[i]));

  for (int i = 0; i < p->count_blobs; ++i)
    blob_ids_.push_back(p->blob_ids[i]);

  if (flags_ & DRM_MODE_PROP_RANGE)
    type_ = DRM_PROPERTY_TYPE_INT;
  else if (flags_ & DRM_MODE_PROP_ENUM)
    type_ = DRM_PROPERTY_TYPE_ENUM;
  else if (flags_ & DRM_MODE_PROP_OBJECT)
    type_ = DRM_PROPERTY_TYPE_OBJECT;
  else if (flags_ & DRM_MODE_PROP_BLOB)
    type_ = DRM_PROPERTY_TYPE_BLOB;
  else if (flags_ & DRM_MODE_PROP_BITMASK)
    type_ = DRM_PROPERTY_TYPE_BITMASK;

  feature_name_ = NULL;
}

uint32_t DrmProperty::id() const {
  return id_;
}

std::string DrmProperty::name() const {
  return name_;
}

void DrmProperty::set_feature(const char* pcFeature) const{
  feature_name_ = pcFeature;
}

std::tuple<int, uint64_t> DrmProperty::value() const {
  if (type_ == DRM_PROPERTY_TYPE_BLOB)
    return std::make_tuple(0, value_);

  if (values_.size() == 0)
    return std::make_tuple(-ENOENT, 0);

  switch (type_) {
    case DRM_PROPERTY_TYPE_INT:
      return std::make_tuple(0, value_);

    case DRM_PROPERTY_TYPE_ENUM:
      if (value_ >= enums_.size())
        return std::make_tuple(-ENOENT, 0);

      return std::make_tuple(0, enums_[value_].value_);

    case DRM_PROPERTY_TYPE_OBJECT:
      return std::make_tuple(0, value_);
    case DRM_PROPERTY_TYPE_BITMASK:
        if(feature_name_ == NULL){
            ALOGE("You don't set feature name for %s",name_.c_str());
            return std::make_tuple(-EINVAL, 0);
        }
        if(strlen(feature_name_) > 0){
            for (auto &drm_enum : enums_){
                if(!strncmp(drm_enum.name_.c_str(),(const char*)feature_name_,strlen(feature_name_))){
                    return std::make_tuple(0, (value_ & (1LL << drm_enum.value_)));
                }
            }
        }else{
            return std::make_tuple(0, 0xff);
        }
        return std::make_tuple(-EINVAL, 0);
    default:
      return std::make_tuple(-EINVAL, 0);
  }
}

std::tuple<int, bool> DrmProperty::bitmask(const char* name) const {
  if (type_ != DRM_PROPERTY_TYPE_BITMASK)
    return std::make_tuple(0, false);

  if (values_.size() == 0)
    return std::make_tuple(-ENOENT, false);

  if(!name)
    return std::make_tuple(-ENOENT, false);

  if(strlen(name) > 0){
      for (auto &drm_enum : enums_){
          if(!strncmp(drm_enum.name_.c_str(),(const char*)name,strlen(name))){
              return std::make_tuple(1LL << drm_enum.value_, true);
          }
      }
  }else{
      return std::make_tuple(0, false);
  }
  return std::make_tuple(0, false);
}

// drm_enum.value_ is the offset of 1LL
std::tuple<int, bool> DrmProperty::value_bitmask(const char* name) const {
  if (type_ != DRM_PROPERTY_TYPE_BITMASK)
    return std::make_tuple(0, false);

  if (values_.size() == 0)
    return std::make_tuple(-ENOENT, false);

  if(!name)
    return std::make_tuple(-ENOENT, false);

  if(strlen(name) > 0){
      for (auto &drm_enum : enums_){
          if(!strncmp(drm_enum.name_.c_str(),(const char*)name,strlen(name))){
              return std::make_tuple(1LL << drm_enum.value_, (value_ & (1LL << drm_enum.value_)) > 0);
          }
      }
  }else{
      return std::make_tuple(0, false);
  }
  return std::make_tuple(0, false);
}

bool DrmProperty::is_immutable() const {
  return id_ && (flags_ & DRM_MODE_PROP_IMMUTABLE);
}

bool DrmProperty::is_range() const {
  return id_ && (flags_ & DRM_MODE_PROP_RANGE);
}

std::tuple<int, uint64_t> DrmProperty::range_min() const {
  if (!is_range())
    return std::make_tuple(-EINVAL, 0);
  if (values_.size() < 1)
    return std::make_tuple(-ENOENT, 0);

  return std::make_tuple(0, values_[0]);
}

std::tuple<int, uint64_t> DrmProperty::range_max() const {
  if (!is_range())
    return std::make_tuple(-EINVAL, 0);
  if (values_.size() < 2)
    return std::make_tuple(-ENOENT, 0);

  return std::make_tuple(0, values_[1]);
}

std::tuple<uint64_t, int> DrmProperty::GetEnumValueWithName(
    std::string name) const {
  for (auto it : enums_) {
    if (it.name_.compare(name) == 0) {
      return std::make_tuple(it.value_, 0);
    }
  }

  return std::make_tuple(UINT64_MAX, -EINVAL);
}
}  // namespace android
