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

#ifndef WIRELESS_ANDROID_AUTOMOTIVE_CAML_SURROUND_VIEW_CORE_LIB_H_
#define WIRELESS_ANDROID_AUTOMOTIVE_CAML_SURROUND_VIEW_CORE_LIB_H_

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace android_auto {
namespace surround_view {

// bounding box (bb)
// It is used to describe the car model bounding box in 3D.
// It assumes z = 0 and only x, y are used in the struct.
// Of course, it is compatible to the 2d version bounding box and may be used
// for other bounding box purpose (e.g., 2d bounding box in image).
struct BoundingBox {
    // (x,y) is bounding box's top left corner coordinate.
    float x;
    float y;

    // (width, height) is the size of the bounding box.
    float width;
    float height;

    BoundingBox() : x(0.0f), y(0.0f), width(0.0f), height(0.0f) {}

    BoundingBox(float x_, float y_, float width_, float height_) :
          x(x_), y(y_), width(width_), height(height_) {}

    BoundingBox(const BoundingBox& bb_) :
          x(bb_.x), y(bb_.y), width(bb_.width), height(bb_.height) {}

    // Checks if data is valid.
    bool IsValid() const { return width >= 0 && height >= 0; }

    bool operator==(const BoundingBox& rhs) const {
        return x == rhs.x && y == rhs.y && width == rhs.width && height == rhs.height;
    }

    BoundingBox& operator=(const BoundingBox& rhs) {
        x = rhs.x;
        y = rhs.y;
        width = rhs.width;
        height = rhs.height;
        return *this;
    }
};

template <typename T>
struct Coordinate2dBase {
    // x coordinate.
    T x;

    // y coordinate.
    T y;

    Coordinate2dBase() : x(0), y(0) {}

    Coordinate2dBase(T x_, T y_) : x(x_), y(y_) {}

    bool operator==(const Coordinate2dBase& rhs) const { return x == rhs.x && y == rhs.y; }

    Coordinate2dBase& operator=(const Coordinate2dBase& rhs) {
        x = rhs.x;
        y = rhs.y;
        return *this;
    }
};

// integer type size.
typedef Coordinate2dBase<int> Coordinate2dInteger;

// float type size.
typedef Coordinate2dBase<float> Coordinate2dFloat;

struct Coordinate3dFloat {
    // x coordinate.
    float x;

    // y coordinate.
    float y;

    // z coordinate.
    float z;

    Coordinate3dFloat() : x(0), y(0), z(0) {}

    Coordinate3dFloat(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    bool operator==(const Coordinate3dFloat& rhs) const { return x == rhs.x && y == rhs.y; }

    Coordinate3dFloat& operator=(const Coordinate3dFloat& rhs) {
        x = rhs.x;
        y = rhs.y;
        return *this;
    }
};

//  pixel weight used for illumination assessment
struct PixelWeight {
    // x and y are the coordinates (absolute value) in image space.
    // pixel coordinate x in horizontal direction.
    float x;

    // pixel coordinate y in vertical direction.
    float y;

    // pixel weight, range in [0, 1].
    float weight;

    PixelWeight() : x(-1), y(-1), weight(0) {}

    PixelWeight(int x_, int y_, int weight_) : x(x_), y(y_), weight(weight_) {}

    bool operator==(const PixelWeight& rhs) const {
        return x == rhs.x && y == rhs.y && weight == rhs.weight;
    }

    PixelWeight& operator=(const PixelWeight& rhs) {
        x = rhs.x;
        y = rhs.y;
        weight = rhs.weight;
        return *this;
    }
};

// base size 2d type template.
template <typename T>
struct Size2dBase {
    // width of size.
    T width;

    // height of size.
    T height;

    Size2dBase() : width(0), height(0) {}

    Size2dBase(T width_, T height_) : width(width_), height(height_) {}

    bool IsValid() const { return width > 0 && height > 0; }

    bool operator==(const Size2dBase& rhs) const {
        return width == rhs.width && height == rhs.height;
    }

    Size2dBase& operator=(const Size2dBase& rhs) {
        width = rhs.width;
        height = rhs.height;
        return *this;
    }
};

// integer type size.
typedef Size2dBase<int> Size2dInteger;

// float type size.
typedef Size2dBase<float> Size2dFloat;

//  surround view 2d parameters
struct SurroundView2dParams {
    // surround view 2d image resolution (width, height).
    Size2dInteger resolution;

    // the physical size of surround view 2d area in surround view coordinate.
    // (surround view coordinate is defined as X rightward, Y forward and
    // the origin lies on the center of the (symmetric) bowl (ground).
    // When bowl is not used, surround view coordinate origin lies on the
    // center of car model bounding box.)
    // The unit should be consistent with camera extrinsics (translation).
    Size2dFloat physical_size;

    // the center of surround view 2d area in surround view coordinate
    // (consistent with extrinsics coordinate).
    Coordinate2dFloat physical_center;

    // Enumeration for list of 2d blending types.
    enum BlendingType { MULTIBAND = 0, ALPHA };

    // Blending type for high quality preset.
    BlendingType high_quality_blending;

    // Blending type for low quality preset.
    BlendingType low_quality_blending;

    // whether gpu acceleration is enabled or not
    bool gpu_acceleration_enabled;

    SurroundView2dParams() :
          resolution{0, 0},
          physical_size{0.0f, 0.0f},
          physical_center{0.0f, 0.0f},
          high_quality_blending(BlendingType::MULTIBAND),
          low_quality_blending(BlendingType::ALPHA),
          gpu_acceleration_enabled(false) {}

    SurroundView2dParams(Size2dInteger resolution_, Size2dFloat physical_size_,
                         Coordinate2dFloat physical_center_,
                         bool gpu_acceleration_enabled_ = false) :
          resolution(resolution_),
          physical_size(physical_size_),
          physical_center(physical_center_),
          high_quality_blending(BlendingType::MULTIBAND),
          low_quality_blending(BlendingType::ALPHA),
          gpu_acceleration_enabled(gpu_acceleration_enabled_) {}

    // Checks if data is valid.
    bool IsValid() const { return resolution.IsValid() && physical_size.IsValid(); }

    bool operator==(const SurroundView2dParams& rhs) const {
        return resolution == rhs.resolution && physical_size == rhs.physical_size &&
                physical_center == rhs.physical_center &&
                high_quality_blending == rhs.high_quality_blending &&
                low_quality_blending == rhs.low_quality_blending &&
                gpu_acceleration_enabled == rhs.gpu_acceleration_enabled;
    }

    SurroundView2dParams& operator=(const SurroundView2dParams& rhs) {
        resolution = rhs.resolution;
        physical_size = rhs.physical_size;
        physical_center = rhs.physical_center;
        high_quality_blending = rhs.high_quality_blending;
        low_quality_blending = rhs.low_quality_blending;
        gpu_acceleration_enabled = rhs.gpu_acceleration_enabled;
        return *this;
    }
};

//  surround view 3d parameters
struct SurroundView3dParams {
    // Bowl center is the origin of the surround view coordinate. If surround view
    // coordinate is different from the global one, a coordinate system
    // transformation function is required.

    // planar area radius.
    // Range in (0, +Inf).
    float plane_radius;

    // the number of divisions on the plane area of bowl, in the direction
    // of the radius.
    // Range in [1, +Inf).
    int plane_divisions;

    // bowl curve curve height.
    // Range in (0, +Inf).
    float curve_height;

    // the number of points on bowl curve curve along radius direction.
    // Range in [1, +Inf).
    int curve_divisions;

    // the number of points along circle (360 degrees)
    // Range in [1, +Inf).
    int angular_divisions;

    // the parabola coefficient of bowl curve curve.
    // The curve formula is z = a * (x^2 + y^2) for sqrt(x^2 + y^2) >
    // plane_radius; a is curve_coefficient.
    // Range in (0, +Inf).
    float curve_coefficient;

    // render output image size.
    Size2dInteger resolution;

    // Include shadows in high details preset.
    bool high_details_shadows;

    // Include reflections in high details preset.
    bool high_details_reflections;

    SurroundView3dParams() :
          plane_radius(0.0f),
          plane_divisions(0),
          curve_height(0.0f),
          curve_divisions(0),
          angular_divisions(0),
          curve_coefficient(0.0f),
          resolution(0, 0),
          high_details_shadows(true),
          high_details_reflections(true) {}

    SurroundView3dParams(float plane_radius_, int plane_divisions_, float curve_height_,
                         int curve_divisions_, int angular_divisions_, float curve_coefficient_,
                         Size2dInteger resolution_) :
          plane_radius(plane_radius_),
          plane_divisions(plane_divisions_),
          curve_height(curve_height_),
          curve_divisions(curve_divisions_),
          angular_divisions(angular_divisions_),
          curve_coefficient(curve_coefficient_),
          resolution(resolution_),
          high_details_shadows(true),
          high_details_reflections(true) {}

    // Checks if data is valid.
    bool IsValid() const {
        return plane_radius > 0 && plane_divisions > 0 && curve_height > 0 &&
                angular_divisions > 0 && curve_coefficient > 0 && curve_divisions > 0 &&
                resolution.IsValid();
    }

    bool operator==(const SurroundView3dParams& rhs) const {
        return plane_radius == rhs.plane_radius && plane_divisions == rhs.plane_divisions &&
                curve_height == rhs.curve_height && curve_divisions == rhs.curve_divisions &&
                angular_divisions == rhs.angular_divisions &&
                curve_coefficient == rhs.curve_coefficient && resolution == rhs.resolution &&
                high_details_shadows == rhs.high_details_shadows &&
                high_details_reflections == rhs.high_details_reflections;
    }

    SurroundView3dParams& operator=(const SurroundView3dParams& rhs) {
        plane_radius = rhs.plane_radius;
        plane_divisions = rhs.plane_divisions;
        curve_height = rhs.curve_height;
        curve_divisions = rhs.curve_divisions;
        angular_divisions = rhs.angular_divisions;
        curve_coefficient = rhs.curve_coefficient;
        resolution = rhs.resolution;
        high_details_shadows = rhs.high_details_shadows;
        high_details_reflections = rhs.high_details_reflections;
        return *this;
    }
};

// surround view camera parameters with native types only.
struct SurroundViewCameraParams {
    // All calibration data |intrinsics|, |rvec| and |tvec|
    // follow OpenCV format excepting using native arrays, refer:
    // https://docs.opencv.org/3.4.0/db/d58/group__calib3d__fisheye.html
    // camera intrinsics. It is the 1d array of camera matrix(3X3) with row first.
    float intrinsics[9];

    // lens distortion parameters.
    float distorion[4];

    // rotation vector.
    float rvec[3];

    // translation vector.
    float tvec[3];

    // camera image size (width, height).
    Size2dInteger size;

    // fisheye circular fov.
    float circular_fov;

    // Full path and filename to the validity mask image file.
    // Mask specifies the valid region of pixels within input camera image.
    std::string validity_mask_filename;

    bool operator==(const SurroundViewCameraParams& rhs) const {
        return (0 == std::memcmp(intrinsics, rhs.intrinsics, 9 * sizeof(float))) &&
                (0 == std::memcmp(distorion, rhs.distorion, 4 * sizeof(float))) &&
                (0 == std::memcmp(rvec, rhs.rvec, 3 * sizeof(float))) &&
                (0 == std::memcmp(tvec, rhs.tvec, 3 * sizeof(float))) && size == rhs.size &&
                circular_fov == rhs.circular_fov;
    }

    SurroundViewCameraParams& operator=(const SurroundViewCameraParams& rhs) {
        std::memcpy(intrinsics, rhs.intrinsics, 9 * sizeof(float));
        std::memcpy(distorion, rhs.distorion, 4 * sizeof(float));
        std::memcpy(rvec, rhs.rvec, 3 * sizeof(float));
        std::memcpy(tvec, rhs.tvec, 3 * sizeof(float));
        size = rhs.size;
        circular_fov = rhs.circular_fov;
        return *this;
    }
};

// 3D vertex of an overlay object.
struct OverlayVertex {
    // Position in 3d coordinates in world space in order X,Y,Z.
    float pos[3];
    // RGBA values, A is used for transparency.
    uint8_t rgba[4];

    bool operator==(const OverlayVertex& rhs) const {
        return (0 == std::memcmp(pos, rhs.pos, 3 * sizeof(float))) &&
                (0 == std::memcmp(rgba, rhs.rgba, 4 * sizeof(uint8_t)));
    }

    OverlayVertex& operator=(const OverlayVertex& rhs) {
        std::memcpy(pos, rhs.pos, 3 * sizeof(float));
        std::memcpy(rgba, rhs.rgba, 4 * sizeof(uint8_t));
        return *this;
    }
};

// Overlay is a list of vertices (may be a single or multiple objects in scene)
// coming from a single source or type of sensor.
struct Overlay {
    // Uniqiue Id identifying each overlay.
    uint16_t id;

    // List of overlay vertices. 3 consecutive vertices form a triangle.
    std::vector<OverlayVertex> vertices;

    // Constructor initializing all member.
    Overlay(uint16_t id_, const std::vector<OverlayVertex>& vertices_) {
        id = id_;
        vertices = vertices_;
    }

    // Default constructor.
    Overlay() {
        id = 0;
        vertices = std::vector<OverlayVertex>();
    }
};

// -----------   Structs related to car model  ---------------

// 3D Vertex of a car model with normal and optionally texture coordinates.
struct CarVertex {
    // 3d position in (x, y, z).
    std::array<float, 3> pos;

    // unit normal at vertex, used for diffuse shading.
    std::array<float, 3> normal;

    // texture coordinates, valid in range [0, 1]. (-1, -1) implies no
    // texture sampling. Note: only a single texture coordinate is currently
    // supported per vertex. This struct will need to be extended with another
    // tex_coord if multiple textures are needed per vertex.
    std::array<float, 2> tex_coord;

    // Default constructor.
    CarVertex() {
        pos = {0, 0, 0};
        normal = {1, 0, 0};
        tex_coord = {-1.0f, -1.0f};
    }

    CarVertex(const std::array<float, 3>& _pos, const std::array<float, 3>& _normal,
              const std::array<float, 2> _tex_coord) :
          pos(_pos), normal(_normal), tex_coord(_tex_coord) {}
};

// Type of texture (color, bump, procedural etc.)
// Currently only color is supported.
enum CarTextureType : uint32_t {
    // Texture map is applied to all color parameters: Ka, Kd and Ks.
    // Data type of texture is RGB with each channel a uint8_t.
    kKa = 0,
    kKd,
    kKs,

    // Texture for bump maps. Data type is 3 channel float.
    kBumpMap
};

// Textures to be used for rendering car model.
struct CarTexture {
    // Type and number of channels are dependant on each car texture type.
    int width;
    int height;
    int channels;
    int bytes_per_channel;
    uint8_t* data;

    CarTexture() {
        width = 0;
        height = 0;
        channels = 0;
        bytes_per_channel = 0;
        data = nullptr;
    }
};

// Material parameters for a car part.
// Refer to MTL properties: http://paulbourke.net/dataformats/mtl/
struct CarMaterial {
    // Illumination model - 0, 1, 2 currently supported
    // 0 = Color on and Ambient off
    // 1 = Color on and Ambient on
    // 2 = Highlight on
    // 3 = Reflection on and Ray trace on
    // 4 - 10 = Reflection/Transparency options not supported,
    //          Will default to option 3.
    uint8_t illum;

    std::array<float, 3> ka;  // Ambient RGB [0, 1]
    std::array<float, 3> kd;  // Diffuse RGB [0, 1]
    std::array<float, 3> ks;  // Specular RGB [0, 1]

    // Dissolve factor [0, 1], 0 = full transparent, 1 = full opaque.
    float d;

    // Specular exponent typically range from 0 to 1000.
    // A high exponent results in a tight, concentrated highlight.
    float ns;

    // Set default values of material.
    CarMaterial() {
        illum = 0;                // Color on, ambient off
        ka = {0.0f, 0.0f, 0.0f};  // No ambient.
        kd = {0.0f, 0.0f, 0.0f};  // No dissolve.
        ks = {0.0f, 0.0f, 0.0f};  // No specular.
        d = 1.0f;                 // Fully opaque.
        ns = 0;                   // No specular exponent.
    }

    // Map for texture type to a string id of a texture.
    std::map<CarTextureType, std::string> textures;
};

// Type alias for 4x4 homogenous matrix, in row-major order.
using Mat4x4 = std::array<float, 16>;

// Represents a part of a car model.
// Each car part is a object in the car that is individually animated and
// has the same illumination properties. A car part may contain sub parts.
struct CarPart {
    // Car part vertices.
    std::vector<CarVertex> vertices;

    // Properties/attributes describing car material.
    CarMaterial material;

    // Model matrix to transform the car part from object space to its parent's
    // coordinate space.
    // The car's vertices are transformed by performing:
    // parent_model_mat * model_mat * car_part_vertices to transform them to the
    // global coordinate space.
    // Model matrix must be a homogenous matrix with orthogonal rotation matrix.
    Mat4x4 model_mat;

    // Id of parent part. Parent part's model matrix is used to animate this part.
    // empty string implies the part has no parent.
    std::string parent_part_id;

    // Ids of child parts. If current part is animated all its child parts
    // are animated as well. Empty vector implies part has not children.
    std::vector<std::string> child_part_ids;

    CarPart(const std::vector<CarVertex>& car_vertices, const CarMaterial& car_material,
            const Mat4x4& car_model_mat, std::string car_parent_part_id,
            const std::vector<std::string>& car_child_part_ids) :
          vertices(car_vertices),
          material(car_material),
          model_mat(car_model_mat),
          parent_part_id(car_parent_part_id),
          child_part_ids(car_child_part_ids) {}

    CarPart& operator=(const CarPart& car_part) {
        this->vertices = car_part.vertices;
        this->material = car_part.material;
        this->model_mat = car_part.model_mat;
        this->parent_part_id = car_part.parent_part_id;
        this->child_part_ids = car_part.child_part_ids;
        return *this;
    }
};

struct AnimationParam {
    // part id
    std::string part_id;

    // model matrix.
    Mat4x4 model_matrix;

    // bool flag indicating if the model matrix is updated from last
    // SetAnimations() call.
    bool is_model_update;

    // gamma.
    float gamma;

    // bool flag indicating if gamma is updated from last
    // SetAnimations() call.
    bool is_gamma_update;

    // texture id.
    std::string texture_id;

    // bool flag indicating if texture is updated from last
    // SetAnimations() call.
    bool is_texture_update;

    // Default constructor, no animations are updated.
    AnimationParam() {
        is_model_update = false;
        is_gamma_update = false;
        is_texture_update = false;
    }

    // Constructor with car part name.
    explicit AnimationParam(const std::string& _part_id) :
          part_id(_part_id),
          is_model_update(false),
          is_gamma_update(false),
          is_texture_update(false) {}

    void SetModelMatrix(const Mat4x4& model_mat) {
        is_model_update = true;
        model_matrix = model_mat;
    }

    void SetGamma(float gamma_value) {
        is_gamma_update = true;
        gamma = gamma_value;
    }

    void SetTexture(const std::string& tex_id) {
        is_texture_update = true;
        texture_id = tex_id;
    }
};

enum Format {
    GRAY = 0,
    RGB = 1,
    RGBA = 2,
};

// collection of surround view static data params.
struct SurroundViewStaticDataParams {
    std::vector<SurroundViewCameraParams> cameras_params;

    // surround view 2d parameters.
    SurroundView2dParams surround_view_2d_params;

    // surround view 3d parameters.
    SurroundView3dParams surround_view_3d_params;

    // undistortion focal length scales.
    std::vector<float> undistortion_focal_length_scales;

    // car model bounding box for 2d surround view.
    BoundingBox car_model_bb;

    // map of texture name to a car texture. Lists all textures to be
    // used for car model rendering.
    std::map<std::string, CarTexture> car_textures;

    // map of car id to a car part. Lists all car parts to be used
    // for car model rendering.
    std::map<std::string, CarPart> car_parts;

    SurroundViewStaticDataParams(const std::vector<SurroundViewCameraParams>& sv_cameras_params,
                                 const SurroundView2dParams& sv_2d_params,
                                 const SurroundView3dParams& sv_3d_params,
                                 const std::vector<float>& scales, const BoundingBox& bb,
                                 const std::map<std::string, CarTexture>& textures,
                                 const std::map<std::string, CarPart>& parts) :
          cameras_params(sv_cameras_params),
          surround_view_2d_params(sv_2d_params),
          surround_view_3d_params(sv_3d_params),
          undistortion_focal_length_scales(scales),
          car_model_bb(bb),
          car_textures(textures),
          car_parts(parts) {}
};

struct SurroundViewInputBufferPointers {
    void* gpu_data_pointer;
    void* cpu_data_pointer;
    Format format;
    int width;
    int height;
    SurroundViewInputBufferPointers() :
          gpu_data_pointer(nullptr), cpu_data_pointer(nullptr), width(0), height(0) {}
    SurroundViewInputBufferPointers(void* gpu_data_pointer_, void* cpu_data_pointer_,
                                    Format format_, int width_, int height_) :
          gpu_data_pointer(gpu_data_pointer_),
          cpu_data_pointer(cpu_data_pointer_),
          format(format_),
          width(width_),
          height(height_) {}
};

// Currently we keep both cpu and gpu data pointers, and only one of them should
// be valid at a certain point. Users need to check null before they make use of
// the data pointers.
// TODO(b/174778117): consider use only one data pointer once GPU migration is
// done. If we are going to keep both cpu and gpu data pointer, specify the type
// of data for cpu data pointer, instead of using a void pointer.
struct SurroundViewResultPointer {
    void* gpu_data_pointer;
    void* cpu_data_pointer;
    Format format;
    int width;
    int height;
    bool is_data_preallocated;
    SurroundViewResultPointer() :
          gpu_data_pointer(nullptr),
          cpu_data_pointer(nullptr),
          width(0),
          height(0),
          is_data_preallocated(false) {}

    // Constructor with result data pointer being allocated within core lib.
    // Use for cases when no already existing buffer is available.
    SurroundViewResultPointer(Format format_, int width_, int height_) :
          gpu_data_pointer(nullptr),
          format(format_),
          width(width_),
          height(height_),
          is_data_preallocated(false) {
        // default formate is gray.
        const int byte_per_pixel = format_ == RGB ? 3 : format_ == RGBA ? 4 : 1;
        cpu_data_pointer = static_cast<void*>(new char[width * height * byte_per_pixel]);
    }

    // Constructor with pre-allocated data.
    // Use for cases when results must be added to an existing allocated buffer.
    // Example, pre-allocated buffer of a display.
    SurroundViewResultPointer(void* gpu_data_pointer_, void* cpu_data_pointer_, Format format_,
                              int width_, int height_) :
          gpu_data_pointer(gpu_data_pointer_),
          cpu_data_pointer(cpu_data_pointer_),
          format(format_),
          width(width_),
          height(height_),
          is_data_preallocated(true) {}

    ~SurroundViewResultPointer() {
        if (cpu_data_pointer) {
            if (!is_data_preallocated) {
                delete[] static_cast<char*>(cpu_data_pointer);
            }
            cpu_data_pointer = nullptr;
        }
    }
};

class SurroundView {
public:
    virtual ~SurroundView() = default;

    // Sets SurroundView static data.
    // For details of SurroundViewStaticDataParams, please refer to the
    // definition.
    virtual bool SetStaticData(const SurroundViewStaticDataParams& static_data_params) = 0;

    // Starts 2d pipeline. Returns false if error occurs.
    virtual bool Start2dPipeline() = 0;

    // Starts 3d pipeline. Returns false if error occurs.
    virtual bool Start3dPipeline() = 0;

    // Stops 2d pipleline. It releases resource owned by the pipeline.
    // Returns false if error occurs.
    virtual void Stop2dPipeline() = 0;

    // Stops 3d pipeline. It releases resource owned by the pipeline.
    virtual void Stop3dPipeline() = 0;

    // Updates 2d output resolution on-the-fly. Starts2dPipeline() must be called
    // before this can be called. For quality assurance, the |resolution| should
    // not be larger than the original one. This call is not thread safe and there
    // is no sync between Get2dSurroundView() and this call.
    virtual bool Update2dOutputResolution(const Size2dInteger& resolution) = 0;

    // Updates 3d output resolution on-the-fly. Starts3dPipeline() must be called
    // before this can be called. For quality assurance, the |resolution| should
    // not be larger than the original one. This call is not thread safe and there
    // is no sync between Get3dSurroundView() and this call.
    virtual bool Update3dOutputResolution(const Size2dInteger& resolution) = 0;

    // Projects camera's pixel location to surround view 2d image location.
    // |camera_point| is the pixel location in raw camera's space.
    // |camera_index| is the camera's index.
    // |surround_view_2d_point| is the surround view 2d image pixel location.
    virtual bool GetProjectionPointFromRawCameraToSurroundView2d(
            const Coordinate2dInteger& camera_point, int camera_index,
            Coordinate2dFloat* surround_view_2d_point) = 0;

    // Projects camera's pixel location to surround view 3d bowl coordinate.
    // |camera_point| is the pixel location in raw camera's space.
    // |camera_index| is the camera's index.
    // |surround_view_3d_point| is the surround view 3d vertex.
    virtual bool GetProjectionPointFromRawCameraToSurroundView3d(
            const Coordinate2dInteger& camera_point, int camera_index,
            Coordinate3dFloat* surround_view_3d_point) = 0;

    // Gets 2d surround view image.
    // It takes input_pointers as input, and output is result_pointer.
    // Please refer to the definition of SurroundViewInputBufferPointers and
    // SurroundViewResultPointer.
    virtual bool Get2dSurroundView(
            const std::vector<SurroundViewInputBufferPointers>& input_pointers,
            SurroundViewResultPointer* result_pointer) = 0;

    // Gets 3d surround view image.
    // It takes |input_pointers| and |view_matrix| as input, and output is
    // |result_pointer|. |view_matrix| is 4 x 4 matrix.
    // Please refer to the definition of
    // SurroundViewInputBufferPointers and
    // SurroundViewResultPointer.
    virtual bool Get3dSurroundView(
            const std::vector<SurroundViewInputBufferPointers>& input_pointers,
            const std::array<std::array<float, 4>, 4>& view_matrix,
            SurroundViewResultPointer* result_pointer) = 0;

    // Gets 3d surround view image overload.
    // It takes |input_pointers|, |quaternion| and |translation| as input,
    // and output is |result_pointer|.
    // |quaternion| is 4 x 1 array (X, Y, Z, W).
    // It is required to be unit quaternion as rotation quaternion.
    // |translation| is 3 X 1 array (x, y, z).
    // Please refer to the definition of
    // SurroundViewInputBufferPointers and
    // SurroundViewResultPointer.
    virtual bool Get3dSurroundView(
            const std::vector<SurroundViewInputBufferPointers>& input_pointers,
            const std::array<float, 4>& quaternion, const std::array<float, 3>& translation,
            SurroundViewResultPointer* result_pointer) = 0;

    // Sets 3d overlays.
    virtual bool Set3dOverlay(const std::vector<Overlay>& overlays) = 0;

    // Animates a set of car parts.
    // Only updated car parts are included.
    // |car_animations| is a vector of AnimationParam specifying updated
    // car parts with updated animation parameters.
    virtual bool SetAnimations(const std::vector<AnimationParam>& car_animations) = 0;

    // for test only.
    virtual std::vector<SurroundViewInputBufferPointers> ReadImages(const char* filename0,
                                                                    const char* filename1,
                                                                    const char* filename2,
                                                                    const char* filename3) = 0;

    virtual void WriteImage(const SurroundViewResultPointer result_pointerer,
                            const char* filename) = 0;
};

SurroundView* Create();

}  // namespace surround_view
}  // namespace android_auto

#endif  // WIRELESS_ANDROID_AUTOMOTIVE_CAML_SURROUND_VIEW_CORE_LIB_H_
