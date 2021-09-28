#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_VERSION_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_VERSION_H_  // NOLINT

#define DYNAMIC_DEPTH_VERSION_MAJOR 1
#define DYNAMIC_DEPTH_VERSION_MINOR 0
#define DYNAMIC_DEPTH_VERSION_REVISION 0

// Classic CPP stringifcation; the extra level of indirection allows the
// preprocessor to expand the macro before being converted to a string.
#define DYNAMIC_DEPTH_TO_STRING_HELPER(x) #x
#define DYNAMIC_DEPTH_TO_STRING(x) DYNAMIC_DEPTH_TO_STRING_HELPER(x)

// The xdmlib version as a string; for example "1.9.0".
#define DYNAMIC_DEPTH_VERSION_STRING                   \
  DYNAMIC_DEPTH_TO_STRING(DYNAMIC_DEPTH_VERSION_MAJOR) \
  "." DYNAMIC_DEPTH_TO_STRING(                         \
      DYNAMIC_DEPTH_VERSION_MINOR) "." \
      DYNAMIC_DEPTH_TO_STRING(DYNAMIC_DEPTH_VERSION_REVISION)

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_VERSION_H_  // NOLINT
