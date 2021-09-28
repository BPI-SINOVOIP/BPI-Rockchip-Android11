#ifndef __CUTILS_LOG_H__
#define __CUTILS_LOG_H__

#ifndef LOG_TAG
#define LOG_TAG nullptr
#endif

enum {
  ANDROID_LOG_UNKNOWN = 0,
  ANDROID_LOG_DEFAULT,
  ANDROID_LOG_VERBOSE,
  ANDROID_LOG_DEBUG,
  ANDROID_LOG_INFO,
  ANDROID_LOG_WARN,
  ANDROID_LOG_ERROR,
  ANDROID_LOG_FATAL,
  ANDROID_LOG_SILENT,
};

#define android_printLog(prio, tag, format, ...) \
  __android_log_print(prio, tag, "[prio %d] " format, prio, ##__VA_ARGS__)

#define LOG_PRI(priority, tag, ...) android_printLog(priority, tag, __VA_ARGS__)
#define ALOG(priority, tag, ...) LOG_PRI(ANDROID_##priority, tag, __VA_ARGS__)

#define __android_second(dummy, second, ...) second
#define __android_rest(first, ...) , ##__VA_ARGS__

#define android_printAssert(condition, tag, format, ...)                \
  __android_log_assert(condition, tag, "assert: condition: %s " format, condition, ##__VA_ARGS__)

#define LOG_ALWAYS_FATAL_IF(condition, ...)                              \
  ((condition)                                                           \
       ? ((void)android_printAssert(#condition, LOG_TAG, ##__VA_ARGS__)) \
       : (void)0)

#define LOG_ALWAYS_FATAL(...) \
  (((void)android_printAssert(NULL, LOG_TAG, ##__VA_ARGS__)))

#define ALOGV(...) ((void)ALOG(LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#define ALOGE(...) ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__))
#define ALOGW(...) ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__))
#define ALOGD(...) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))

#define LOG_FATAL_IF(cond, ...) LOG_ALWAYS_FATAL_IF(cond, ##__VA_ARGS__)

#define LOG_FATAL(...) LOG_ALWAYS_FATAL(__VA_ARGS__)

#define ALOG_ASSERT(cond, ...) LOG_FATAL_IF(!(cond), ##__VA_ARGS__)

extern "C" {

int __android_log_print(int priority, const char* tag, const char* format, ...);

[[noreturn]] void __android_log_assert(const char* condition, const char* tag,
                                       const char* format, ...);

}

#endif
