/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_IMAGE_H_
#define ART_RUNTIME_IMAGE_H_

#include <string.h>

#include "base/enums.h"
#include "base/iteration_range.h"
#include "mirror/object.h"
#include "runtime_globals.h"

namespace art {

class ArtField;
class ArtMethod;
template <class MirrorType> class ObjPtr;

namespace linker {
class ImageWriter;
}  // namespace linker

class ObjectVisitor {
 public:
  virtual ~ObjectVisitor() {}

  virtual void Visit(mirror::Object* object) = 0;
};

class PACKED(4) ImageSection {
 public:
  ImageSection() : offset_(0), size_(0) { }
  ImageSection(uint32_t offset, uint32_t size) : offset_(offset), size_(size) { }
  ImageSection(const ImageSection& section) = default;
  ImageSection& operator=(const ImageSection& section) = default;

  uint32_t Offset() const {
    return offset_;
  }

  uint32_t Size() const {
    return size_;
  }

  uint32_t End() const {
    return Offset() + Size();
  }

  bool Contains(uint64_t offset) const {
    return offset - offset_ < size_;
  }

 private:
  uint32_t offset_;
  uint32_t size_;
};

// Header of image files written by ImageWriter, read and validated by Space.
// Packed to object alignment since the first object follows directly after the header.
static_assert(kObjectAlignment == 8, "Alignment check");
class PACKED(8) ImageHeader {
 public:
  enum StorageMode : uint32_t {
    kStorageModeUncompressed,
    kStorageModeLZ4,
    kStorageModeLZ4HC,
    kStorageModeCount,  // Number of elements in enum.
  };
  static constexpr StorageMode kDefaultStorageMode = kStorageModeUncompressed;

  // Solid block of the image. May be compressed or uncompressed.
  class PACKED(4) Block final {
   public:
    Block(StorageMode storage_mode,
          uint32_t data_offset,
          uint32_t data_size,
          uint32_t image_offset,
          uint32_t image_size)
        : storage_mode_(storage_mode),
          data_offset_(data_offset),
          data_size_(data_size),
          image_offset_(image_offset),
          image_size_(image_size) {}

    bool Decompress(uint8_t* out_ptr, const uint8_t* in_ptr, std::string* error_msg) const;

    StorageMode GetStorageMode() const {
      return storage_mode_;
    }

    uint32_t GetDataSize() const {
      return data_size_;
    }

    uint32_t GetImageSize() const {
      return image_size_;
    }

   private:
    // Storage method for the image, the image may be compressed.
    StorageMode storage_mode_ = kDefaultStorageMode;

    // Compressed offset and size.
    uint32_t data_offset_ = 0u;
    uint32_t data_size_ = 0u;

    // Image offset and size (decompressed or mapped location).
    uint32_t image_offset_ = 0u;
    uint32_t image_size_ = 0u;
  };

  ImageHeader() {}
  ImageHeader(uint32_t image_reservation_size,
              uint32_t component_count,
              uint32_t image_begin,
              uint32_t image_size,
              ImageSection* sections,
              uint32_t image_roots,
              uint32_t oat_checksum,
              uint32_t oat_file_begin,
              uint32_t oat_data_begin,
              uint32_t oat_data_end,
              uint32_t oat_file_end,
              uint32_t boot_image_begin,
              uint32_t boot_image_size,
              uint32_t boot_image_component_count,
              uint32_t boot_image_checksum,
              uint32_t pointer_size);

  bool IsValid() const;
  const char* GetMagic() const;

  uint32_t GetImageReservationSize() const {
    return image_reservation_size_;
  }

  uint32_t GetComponentCount() const {
    return component_count_;
  }

  uint8_t* GetImageBegin() const {
    return reinterpret_cast<uint8_t*>(image_begin_);
  }

  size_t GetImageSize() const {
    return image_size_;
  }

  uint32_t GetImageChecksum() const {
    return image_checksum_;
  }

  void SetImageChecksum(uint32_t image_checksum) {
    image_checksum_ = image_checksum;
  }

  uint32_t GetOatChecksum() const {
    return oat_checksum_;
  }

  void SetOatChecksum(uint32_t oat_checksum) {
    oat_checksum_ = oat_checksum;
  }

  // The location that the oat file was expected to be when the image was created. The actual
  // oat file may be at a different location for application images.
  uint8_t* GetOatFileBegin() const {
    return reinterpret_cast<uint8_t*>(oat_file_begin_);
  }

  uint8_t* GetOatDataBegin() const {
    return reinterpret_cast<uint8_t*>(oat_data_begin_);
  }

  uint8_t* GetOatDataEnd() const {
    return reinterpret_cast<uint8_t*>(oat_data_end_);
  }

  uint8_t* GetOatFileEnd() const {
    return reinterpret_cast<uint8_t*>(oat_file_end_);
  }

  PointerSize GetPointerSize() const;

  uint32_t GetPointerSizeUnchecked() const {
    return pointer_size_;
  }

  static std::string GetOatLocationFromImageLocation(const std::string& image) {
    return GetLocationFromImageLocation(image, "oat");
  }

  static std::string GetVdexLocationFromImageLocation(const std::string& image) {
    return GetLocationFromImageLocation(image, "vdex");
  }

  enum ImageMethod {
    kResolutionMethod,
    kImtConflictMethod,
    kImtUnimplementedMethod,
    kSaveAllCalleeSavesMethod,
    kSaveRefsOnlyMethod,
    kSaveRefsAndArgsMethod,
    kSaveEverythingMethod,
    kSaveEverythingMethodForClinit,
    kSaveEverythingMethodForSuspendCheck,
    kImageMethodsCount,  // Number of elements in enum.
  };

  enum ImageRoot {
    kDexCaches,
    kClassRoots,
    kSpecialRoots,                    // Different for boot image and app image, see aliases below.
    kImageRootsMax,

    // Aliases.
    kAppImageClassLoader = kSpecialRoots,   // The class loader used to build the app image.
    kBootImageLiveObjects = kSpecialRoots,  // Array of boot image objects that must be kept live.
  };

  enum BootImageLiveObjects {
    kOomeWhenThrowingException,       // Pre-allocated OOME when throwing exception.
    kOomeWhenThrowingOome,            // Pre-allocated OOME when throwing OOME.
    kOomeWhenHandlingStackOverflow,   // Pre-allocated OOME when handling StackOverflowError.
    kNoClassDefFoundError,            // Pre-allocated NoClassDefFoundError.
    kClearedJniWeakSentinel,          // Pre-allocated sentinel for cleared weak JNI references.
    kIntrinsicObjectsStart
  };

  /*
   * This describes the number and ordering of sections inside of Boot
   * and App Images.  It is very important that changes to this struct
   * are reflected in the compiler and loader.
   *
   * See:
   *   - ImageWriter::ImageInfo::CreateImageSections()
   *   - ImageWriter::Write()
   *   - ImageWriter::AllocMemory()
   */
  enum ImageSections {
    kSectionObjects,
    kSectionArtFields,
    kSectionArtMethods,
    kSectionRuntimeMethods,
    kSectionImTables,
    kSectionIMTConflictTables,
    kSectionDexCacheArrays,
    kSectionInternedStrings,
    kSectionClassTable,
    kSectionStringReferenceOffsets,
    kSectionMetadata,
    kSectionImageBitmap,
    kSectionCount,  // Number of elements in enum.
  };

  static size_t NumberOfImageRoots(bool app_image ATTRIBUTE_UNUSED) {
    // At the moment, boot image and app image have the same number of roots,
    // though the meaning of the kSpecialRoots is different.
    return kImageRootsMax;
  }

  ArtMethod* GetImageMethod(ImageMethod index) const;

  ImageSection& GetImageSection(ImageSections index) {
    DCHECK_LT(static_cast<size_t>(index), kSectionCount);
    return sections_[index];
  }

  const ImageSection& GetImageSection(ImageSections index) const {
    DCHECK_LT(static_cast<size_t>(index), kSectionCount);
    return sections_[index];
  }

  const ImageSection& GetObjectsSection() const {
    return GetImageSection(kSectionObjects);
  }

  const ImageSection& GetFieldsSection() const {
    return GetImageSection(ImageHeader::kSectionArtFields);
  }

  const ImageSection& GetMethodsSection() const {
    return GetImageSection(kSectionArtMethods);
  }

  const ImageSection& GetRuntimeMethodsSection() const {
    return GetImageSection(kSectionRuntimeMethods);
  }

  const ImageSection& GetImTablesSection() const {
    return GetImageSection(kSectionImTables);
  }

  const ImageSection& GetIMTConflictTablesSection() const {
    return GetImageSection(kSectionIMTConflictTables);
  }

  const ImageSection& GetDexCacheArraysSection() const {
    return GetImageSection(kSectionDexCacheArrays);
  }

  const ImageSection& GetInternedStringsSection() const {
    return GetImageSection(kSectionInternedStrings);
  }

  const ImageSection& GetClassTableSection() const {
    return GetImageSection(kSectionClassTable);
  }

  const ImageSection& GetImageStringReferenceOffsetsSection() const {
    return GetImageSection(kSectionStringReferenceOffsets);
  }

  const ImageSection& GetMetadataSection() const {
    return GetImageSection(kSectionMetadata);
  }

  const ImageSection& GetImageBitmapSection() const {
    return GetImageSection(kSectionImageBitmap);
  }

  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<mirror::Object> GetImageRoot(ImageRoot image_root) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<mirror::ObjectArray<mirror::Object>> GetImageRoots() const
      REQUIRES_SHARED(Locks::mutator_lock_);

  void RelocateImageReferences(int64_t delta);
  void RelocateBootImageReferences(int64_t delta);

  uint32_t GetBootImageBegin() const {
    return boot_image_begin_;
  }

  uint32_t GetBootImageSize() const {
    return boot_image_size_;
  }

  uint32_t GetBootImageComponentCount() const {
    return boot_image_component_count_;
  }

  uint32_t GetBootImageChecksum() const {
    return boot_image_checksum_;
  }

  uint64_t GetDataSize() const {
    return data_size_;
  }

  bool IsAppImage() const;

  uint32_t GetImageSpaceCount() const;

  // Visit mirror::Objects in the section starting at base.
  // TODO: Delete base parameter if it is always equal to GetImageBegin.
  void VisitObjects(ObjectVisitor* visitor,
                    uint8_t* base,
                    PointerSize pointer_size) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit ArtMethods in the section starting at base. Includes runtime methods.
  // TODO: Delete base parameter if it is always equal to GetImageBegin.
  // NO_THREAD_SAFETY_ANALYSIS for template visitor pattern.
  template <typename Visitor>
  void VisitPackedArtMethods(const Visitor& visitor,
                             uint8_t* base,
                             PointerSize pointer_size) const NO_THREAD_SAFETY_ANALYSIS;

  // Visit ArtMethods in the section starting at base.
  // TODO: Delete base parameter if it is always equal to GetImageBegin.
  // NO_THREAD_SAFETY_ANALYSIS for template visitor pattern.
  template <typename Visitor>
  void VisitPackedArtFields(const Visitor& visitor, uint8_t* base) const NO_THREAD_SAFETY_ANALYSIS;

  template <typename Visitor>
  void VisitPackedImTables(const Visitor& visitor,
                           uint8_t* base,
                           PointerSize pointer_size) const;

  template <typename Visitor>
  void VisitPackedImtConflictTables(const Visitor& visitor,
                                    uint8_t* base,
                                    PointerSize pointer_size) const;

  IterationRange<const Block*> GetBlocks() const {
    return GetBlocks(GetImageBegin());
  }

  IterationRange<const Block*> GetBlocks(const uint8_t* image_begin) const {
    const Block* begin = reinterpret_cast<const Block*>(image_begin + blocks_offset_);
    return {begin, begin + blocks_count_};
  }

  // Return true if the image has any compressed blocks.
  bool HasCompressedBlock() const {
    return blocks_count_ != 0u;
  }

  uint32_t GetBlockCount() const {
    return blocks_count_;
  }

 private:
  static const uint8_t kImageMagic[4];
  static const uint8_t kImageVersion[4];

  static std::string GetLocationFromImageLocation(const std::string& image,
                                                  const std::string& extension) {
    std::string filename = image;
    if (filename.length() <= 3) {
      filename += "." + extension;
    } else {
      filename.replace(filename.length() - 3, 3, extension);
    }
    return filename;
  }

  uint8_t magic_[4];
  uint8_t version_[4];

  // The total memory reservation size for the image.
  // For boot image or boot image extension, the primary image includes the reservation
  // for all image files and oat files, secondary images have the reservation set to 0.
  // App images have reservation equal to `image_size_` rounded up to page size because
  // their oat files are mmapped independently.
  uint32_t image_reservation_size_ = 0u;

  // The number of components.
  // For boot image or boot image extension, the primary image stores the total number
  // of images, secondary images have this set to 0.
  // App images have 1 component.
  uint32_t component_count_ = 0u;

  // Required base address for mapping the image.
  uint32_t image_begin_ = 0u;

  // Image size, not page aligned.
  uint32_t image_size_ = 0u;

  // Image file checksum (calculated with the checksum field set to 0).
  uint32_t image_checksum_ = 0u;

  // Checksum of the oat file we link to for load time sanity check.
  uint32_t oat_checksum_ = 0u;

  // Start address for oat file. Will be before oat_data_begin_ for .so files.
  uint32_t oat_file_begin_ = 0u;

  // Required oat address expected by image Method::GetCode() pointers.
  uint32_t oat_data_begin_ = 0u;

  // End of oat data address range for this image file.
  uint32_t oat_data_end_ = 0u;

  // End of oat file address range. will be after oat_data_end_ for
  // .so files. Used for positioning a following alloc spaces.
  uint32_t oat_file_end_ = 0u;

  // Boot image begin and end (only applies to boot image extension and app image headers).
  uint32_t boot_image_begin_ = 0u;
  uint32_t boot_image_size_ = 0u;  // Includes heap (*.art) and code (.oat).

  // Number of boot image components that this image depends on and their composite checksum
  // (only applies to boot image extension and app image headers).
  uint32_t boot_image_component_count_ = 0u;
  uint32_t boot_image_checksum_ = 0u;

  // Absolute address of an Object[] of objects needed to reinitialize from an image.
  uint32_t image_roots_ = 0u;

  // Pointer size, this affects the size of the ArtMethods.
  uint32_t pointer_size_ = 0u;

  // Image section sizes/offsets correspond to the uncompressed form.
  ImageSection sections_[kSectionCount];

  // Image methods, may be inside of the boot image for app images.
  uint64_t image_methods_[kImageMethodsCount];

  // Data size for the image data excluding the bitmap and the header. For compressed images, this
  // is the compressed size in the file.
  uint32_t data_size_ = 0u;

  // Image blocks, only used for compressed images.
  uint32_t blocks_offset_ = 0u;
  uint32_t blocks_count_ = 0u;

  friend class linker::ImageWriter;
};

/*
 * This type holds the information necessary to fix up AppImage string
 * references.
 *
 * The first element of the pair is an offset into the image space.  If the
 * offset is tagged (testable using HasDexCacheNativeRefTag) it indicates the location
 * of a DexCache object that has one or more native references to managed
 * strings that need to be fixed up.  In this case the second element has no
 * meaningful value.
 *
 * If the first element isn't tagged then it indicates the location of a
 * managed object with a field that needs fixing up.  In this case the second
 * element of the pair is an object-relative offset to the field in question.
 */
typedef std::pair<uint32_t, uint32_t> AppImageReferenceOffsetInfo;

/*
 * Tags the last bit.  Used by AppImage logic to differentiate between pointers
 * to managed objects and pointers to native reference arrays.
 */
template<typename T>
T SetDexCacheStringNativeRefTag(T val) {
  static_assert(std::is_integral<T>::value, "Expected integral type.");

  return val | 1u;
}

/*
 * Tags the second last bit.  Used by AppImage logic to differentiate between pointers
 * to managed objects and pointers to native reference arrays.
 */
template<typename T>
T SetDexCachePreResolvedStringNativeRefTag(T val) {
  static_assert(std::is_integral<T>::value, "Expected integral type.");

  return val | 2u;
}

/*
 * Retrieves the value of the last bit.  Used by AppImage logic to
 * differentiate between pointers to managed objects and pointers to native
 * reference arrays.
 */
template<typename T>
bool HasDexCacheStringNativeRefTag(T val) {
  static_assert(std::is_integral<T>::value, "Expected integral type.");

  return (val & 1u) != 0u;
}

/*
 * Retrieves the value of the second last bit.  Used by AppImage logic to
 * differentiate between pointers to managed objects and pointers to native
 * reference arrays.
 */
template<typename T>
bool HasDexCachePreResolvedStringNativeRefTag(T val) {
  static_assert(std::is_integral<T>::value, "Expected integral type.");

  return (val & 2u) != 0u;
}

/*
 * Sets the last bit of the value to 0.  Used by AppImage logic to
 * differentiate between pointers to managed objects and pointers to native
 * reference arrays.
 */
template<typename T>
T ClearDexCacheNativeRefTags(T val) {
  static_assert(std::is_integral<T>::value, "Expected integral type.");

  return val & ~3u;
}

std::ostream& operator<<(std::ostream& os, const ImageHeader::ImageMethod& method);
std::ostream& operator<<(std::ostream& os, const ImageHeader::ImageRoot& root);
std::ostream& operator<<(std::ostream& os, const ImageHeader::ImageSections& section);
std::ostream& operator<<(std::ostream& os, const ImageSection& section);
std::ostream& operator<<(std::ostream& os, const ImageHeader::StorageMode& mode);

}  // namespace art

#endif  // ART_RUNTIME_IMAGE_H_
