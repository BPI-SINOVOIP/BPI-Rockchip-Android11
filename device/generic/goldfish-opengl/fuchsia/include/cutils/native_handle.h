#ifndef __CUTILS_NATIVE_HANDLE_H__
#define __CUTILS_NATIVE_HANDLE_H__

typedef struct native_handle {
    int version;
    int numFds;
    int numInts;
} native_handle_t;

#endif
