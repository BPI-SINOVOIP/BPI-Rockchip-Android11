/*
 * Copyright 2020 The Android Open Source Project
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

#ifndef SURROUND_VIEW_SERVICE_IMPL_MTLREADER_H_
#define SURROUND_VIEW_SERVICE_IMPL_MTLREADER_H_

#include <map>
#include <string>

#include "core_lib.h"

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

// Mtl defined params.
struct MtlConfigParams {
    // Ns exponent
    // Specifies the specular exponent for the current material. This defines
    // the focus of the specular highlight.
    // Ns values normally range from 0 to 1000.
    float ns = -1;

    // optical_density
    // Specifies the optical density for the surface. This is also known as
    // index of refraction.
    //  "optical_density" is the value for the optical density. The values can
    // range from 0.001 to 10. A value of 1.0 means that light does not bend
    // as it passes through an object. Increasing the optical_density
    // increases the amount of bending. Glass has an index of refraction of
    // about 1.5.  Values of less than 1.0 produce bizarre results and are not
    // recommended.
    float ni = -1;

    // d defines the non-transparency of the material to be alpha.
    // The default is 1.0 (not transparent at all).
    // The quantities d and Tr are the opposites of each other.
    float d = -1;

    // The Tr statement specifies the transparency of the material to be alpha.
    // The default is 0.0 (not transparent at all).
    // The quantities d and Tr are the opposites of each other,
    float tr = -1;

    // The Tf statement specifies the transmission filter using RGB values.
    // "r g b" are the values for the red, green, and blue components of the
    // atmosphere.  The g and b arguments are optional. If only r is
    // specified, then g, and b are assumed to be equal to r. The r g b values
    // are normally in the range of 0.0 to 1.0. Values outside this range
    // increase or decrease the relectivity accordingly.
    float tf[3] = {-1, -1, -1};

    // illum_#
    // The "illum" statement specifies the illumination model to use in the
    // material.  Illumination models are mathematical equations that represent
    // various material lighting and shading effects.
    //
    // "illum_#"can be a number from 0 to 10. The illumination models are
    // summarized below;
    //
    //  Illumination    Properties that are turned on in the
    //  model           Property Editor
    //
    //  0 Color on and Ambient off
    //  1 Color on and Ambient on
    //  2 Highlight on
    //  3 Reflection on and Ray trace on
    //  4 Transparency: Glass on
    //    Reflection: Ray trace on
    //  5 Reflection: Fresnel on and Ray trace on
    //  6 Transparency: Refraction on
    //    Reflection: Fresnel off and Ray trace on
    //  7 Transparency: Refraction on
    //    Reflection: Fresnel on and Ray trace on
    //  8 Reflection on and Ray trace off
    //  9 Transparency: Glass on
    //    Reflection: Ray trace off
    // 10 Casts shadows onto invisible surfaces
    int illum = -1;

    // The Ka statement specifies the ambient reflectivity using RGB values.
    // "r g b" are the values for the red, green, and blue components of the
    // color.  The g and b arguments are optional. If only r is specified,
    // then g, and b are assumed to be equal to r. The r g b values are
    // normally in the range of 0.0 to 1.0. Values outside this range increase
    // or decrease the relectivity accordingly.
    float ka[3] = {-1, -1, -1};

    // The Kd statement specifies the diffuse reflectivity using RGB values.
    //  "r g b" are the values for the red, green, and blue components of the
    // atmosphere.  The g and b arguments are optional.  If only r is
    // specified, then g, and b are assumed to be equal to r. The r g b values
    // are normally in the range of 0.0 to 1.0. Values outside this range
    // increase or decrease the relectivity accordingly.
    float kd[3] = {-1, -1, -1};

    // The Ks statement specifies the specular reflectivity using RGB values.
    //  "r g b" are the values for the red, green, and blue components of the
    // atmosphere. The g and b arguments are optional. If only r is
    // specified, then g, and b are assumed to be equal to r. The r g b values
    // are normally in the range of 0.0 to 1.0. Values outside this range
    // increase or decrease the relectivity accordingly.
    float ks[3] = {-1, -1, -1};

    // Emissive coeficient. It goes together with ambient, diffuse and specular
    // and represents the amount of light emitted by the material.
    float ke[3] = {-1, -1, -1};

    // Specifies that a color texture file or color procedural texture file is
    // linked to the specular reflectivity of the material. During rendering,
    // the map_Ks value is multiplied by the Ks value.
    std::string mapKs;

    // Specifies that a color texture file or a color procedural texture file
    // is applied to the ambient reflectivity of the material. During
    // rendering, the "map_Ka" value is multiplied by the "Ka" value.
    std::string mapKa;

    // Specifies that a color texture file or color procedural texture file is
    // linked to the diffuse reflectivity of the material. During rendering,
    // the map_Kd value is multiplied by the Kd value.
    std::string mapKd;

    // Same as bump
    std::string mapBump;

    // Specifies that a bump texture file or a bump procedural texture file is
    // linked to the material.
    std::string bump;

    MtlConfigParams& operator=(const MtlConfigParams& rhs) {
        ns = rhs.ns;
        ni = rhs.ni;
        d = rhs.d;
        tr = rhs.tr;
        std::memcpy(tf, rhs.tf, 3 * sizeof(float));
        illum = rhs.illum;
        std::memcpy(ka, rhs.ka, 3 * sizeof(float));
        std::memcpy(kd, rhs.kd, 3 * sizeof(float));
        std::memcpy(ks, rhs.ks, 3 * sizeof(float));
        std::memcpy(ke, rhs.ke, 3 * sizeof(float));
        mapKs = rhs.mapKs;
        mapKa = rhs.mapKa;
        mapKd = rhs.mapKd;
        mapBump = rhs.mapBump;
        bump = rhs.bump;

        return *this;
    }
};

// Reads mtl file associated with obj file.
// |filename| is the full path and name of the obj file.
bool ReadMtlFromFile(const std::string& mtlFilename,
                     std::map<std::string, MtlConfigParams>* params);

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // SURROUND_VIEW_SERVICE_IMPL_MTLREADER_H_
