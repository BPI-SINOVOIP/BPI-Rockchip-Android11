#ifndef __HARDWARE_HARDWARE_H
#define __HARDWARE_HARDWARE_H

#include <stdint.h>

#define MAKE_TAG_CONSTANT(A,B,C,D) (((A) << 24) | ((B) << 16) | ((C) << 8) | (D))

#define HARDWARE_MODULE_TAG MAKE_TAG_CONSTANT('H', 'W', 'M', 'T')
#define HARDWARE_DEVICE_TAG MAKE_TAG_CONSTANT('H', 'W', 'D', 'T')

#define HARDWARE_HAL_API_VERSION 0

struct hw_module_t;
struct hw_module_methods_t;
struct hw_device_t;

typedef struct hw_module_t {
    uint32_t tag;
    uint16_t module_api_version;
    uint16_t hal_api_version;
    const char *id;
    const char *name;
    const char *author;
    struct hw_module_methods_t* methods;
    void* dso;
} hw_module_t;

typedef struct hw_module_methods_t {
    int (*open)(const struct hw_module_t* module, const char* id,
                struct hw_device_t** device);
} hw_module_methods_t;

typedef struct hw_device_t {
    uint32_t tag;
    uint32_t version;
    struct hw_module_t* module;
    int (*close)(struct hw_device_t* device);
} hw_device_t;

#endif
