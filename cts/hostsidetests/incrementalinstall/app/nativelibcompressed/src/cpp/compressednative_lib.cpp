#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_android_incrementalinstall_incrementaltestapp_nativelib_CompressedNativeLib_getString(
    JNIEnv *env, jobject /* this */) {
  std::string str = "String from compressed native lib";
  return env->NewStringUTF(str.c_str());
}
