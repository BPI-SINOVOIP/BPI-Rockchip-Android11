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
#include "drmresources.h"

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
    : id_(0), type_(DRM_PROPERTY_TYPE_INVALID), flags_(0), name_(""), p_(p) {
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

int DrmProperty::value(uint64_t *value) const {
  if (type_ == DRM_PROPERTY_TYPE_BLOB) {
    *value = value_;
    return 0;
  }

  if (values_.size() == 0)
    return -ENOENT;

  switch (type_) {
    case DRM_PROPERTY_TYPE_INT:
      *value = value_;
      return 0;

    case DRM_PROPERTY_TYPE_ENUM:
      if (value_ >= enums_.size())
        return -ENOENT;

      *value = enums_[value_].value_;
      return 0;

    case DRM_PROPERTY_TYPE_OBJECT:
      *value = value_;
      return 0;

    case DRM_PROPERTY_TYPE_BITMASK:
        if(feature_name_ == NULL)
        {
            ALOGE("You don't set feature name for %s",name_.c_str());
            return -EINVAL;
        }
        //if(!strcmp(feature_name_,"scale"))
        if(strlen(feature_name_) > 0)
        {
            for (auto &drm_enum : enums_)
            {
                if(!strncmp(drm_enum.name_.c_str(),(const char*)feature_name_,strlen(feature_name_)))
                {
                    *value = (value_ & (1LL << drm_enum.value_));
                    break;
                }
            }
        }
        else
        {
            *value = value_;
        }

        return 0;

    default:
      return -EINVAL;
  }
}
}
