#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <vector>

#include "system/camera_metadata.h"

#define OK    0

static inline uint32_t AlignUp(uint32_t num, uint32_t alignment) {
    return (num + (alignment - 1)) & (~(alignment - 1));
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const size_t sizeof_int = sizeof(uint32_t);
    if (data == nullptr || size < 2 * sizeof_int) {
        return 0;
    }
    camera_metadata_t *m = NULL;

    // Use first int as capacity, and the following one as data capacity.
    uint32_t entry_capacity = *reinterpret_cast<const uint32_t*>(data) % 0xFF;
    uint32_t data_capacity  = *reinterpret_cast<const uint32_t*>(data + sizeof_int) % 0xFFF;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    size_t i = 2 * sizeof_int;
    std::vector<uint32_t> tags;
    // Do we have at least 2 ints left?
    while(i + (2 * sizeof_int) < size) {
        // Use one int as tag Id, and the following one as data size.
        // Note that i is already aligned at this point.
        uint32_t tag = *reinterpret_cast<const uint32_t*>(data + i);
        uint32_t data_count = *reinterpret_cast<const uint32_t*>(
            data + i + sizeof_int) % 0xFF;

        i += 2 * sizeof_int;

        int32_t tag_type = get_camera_metadata_tag_type(tag);

        // If the tag doesn't exists, just try to add it anyway to try that path,
        // but skip the rest of the loop.
        if (tag_type == -1) {
            add_camera_metadata_entry(m, tag, data, data_count);
            validate_camera_metadata_structure(m, NULL);
            continue;
        }

        size_t tag_data_size = camera_metadata_type_size[tag_type] * data_count;

        // Is there enough data left to consider this tag/size pair?
        if (i + tag_data_size >= size) {
            continue;
        }

        const void* tag_data = data + i;
        // add then remove
        add_camera_metadata_entry(m, tag, tag_data, data_count);
        validate_camera_metadata_structure(m, NULL);
        camera_metadata_ro_entry_t entry;
        if (OK == find_camera_metadata_ro_entry(m, tag, &entry)) {
            delete_camera_metadata_entry(m, entry.index);
            validate_camera_metadata_structure(m, NULL);
        }

        // add back
        add_camera_metadata_entry(m, tag, tag_data, data_count);
        tags.push_back(tag);

        get_camera_metadata_section_name(tag);
        get_camera_metadata_tag_name(tag);
        get_camera_metadata_tag_type(tag);

        i += AlignUp(tag_data_size, sizeof_int);
    }

    for (auto tag: tags){
        camera_metadata_ro_entry_t entry;
        if (OK == find_camera_metadata_ro_entry(m, tag, &entry)) {
            delete_camera_metadata_entry(m, entry.index);
            validate_camera_metadata_structure(m, NULL);
        }
    }

    free_camera_metadata(m);
    return 0;
}