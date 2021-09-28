#ifndef __CUTILS_PROPERTIES_H__
#define __CUTILS_PROPERTIES_H__

#define PROPERTY_VALUE_MAX 92

extern "C" {

int property_get(const char* key, char* value, const char* default_value);

}

#endif
