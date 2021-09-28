// This is not your usual header guard. The macro
// PHOTOS_EDITING_FORMATS_DYNAMIC_DEPTH_INTERNAL_BASE_WARNINGS_DISABLED shows up
// again in reenable_warnings.h.
#ifndef DYNAMIC_DEPTH_WARNINGS_DISABLED  // NOLINT
#define DYNAMIC_DEPTH_WARNINGS_DISABLED

#ifdef _MSC_VER
#pragma warning(push)
// Disable the warning C4251 which is triggered by stl classes in
// xmpmeta's public interface. To quote MSDN: "C4251 can be ignored "
// "if you are deriving from a type in the Standard C++ Library"
#pragma warning(disable : 4251)
#endif

#endif  // DYNAMIC_DEPTH_WARNINGS_DISABLED
