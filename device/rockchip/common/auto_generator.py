#!/usr/bin/env python
import sys
import os
import re
import zipfile
import shutil
import logging
import string

templet = """include $(CLEAR_VARS)
LOCAL_MODULE := %s
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_PATH := $(TARGET_OUT_ODM)/%s
LOCAL_SRC_FILES := $(LOCAL_MODULE)$(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_CERTIFICATE := PRESIGNED
#LOCAL_DEX_PREOPT := false
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_JNI_SHARED_LIBRARIES_ABI := %s
MY_LOCAL_PREBUILT_JNI_LIBS := %s
MY_APP_LIB_PATH := $(TARGET_OUT_ODM)/%s/$(LOCAL_MODULE)/lib/$(LOCAL_JNI_SHARED_LIBRARIES_ABI)
ifneq ($(LOCAL_JNI_SHARED_LIBRARIES_ABI), None)
$(warning MY_APP_LIB_PATH=$(MY_APP_LIB_PATH))
LOCAL_POST_INSTALL_CMD := \
    mkdir -p $(MY_APP_LIB_PATH) \
    $(foreach lib, $(MY_LOCAL_PREBUILT_JNI_LIBS), ; cp -f $(LOCAL_PATH)/$(lib) $(MY_APP_LIB_PATH)/$(notdir $(lib)))
endif
include $(BUILD_PREBUILT)

"""

copy_app_templet = """LOCAL_PATH := $(my-dir)
include $(CLEAR_VARS)
LOCAL_APK_NAME := %s
LOCAL_POST_PROCESS_COMMAND := $(shell mkdir -p $(TARGET_OUT_ODM)/%s/$(LOCAL_APK_NAME) && cp $(LOCAL_PATH)/$(LOCAL_APK_NAME).apk $(TARGET_OUT_ODM)/%s/$(LOCAL_APK_NAME)/)
"""

def main(argv):
    preinstall_dir = os.path.join(argv[1],argv[2])
    if os.path.exists(preinstall_dir):
        #Use to include modules
        isfound = 'not_found_lib'
        include_path = preinstall_dir + '/preinstall.mk'
        android_path = preinstall_dir + '/Android.mk'
        target_arch = argv[4]

        if os.path.exists(include_path):
            os.remove(include_path)
        if os.path.exists(android_path):
            os.remove(android_path)

        includefile = file(include_path, 'w')
        androidfile = file(android_path, 'w')

        androidfile.write("include $(call all-subdir-makefiles)\n\n")

        MY_LOCAL_PREBUILT_JNI_LIBS = '\\' + '\n'

        for root, dirs, files in os.walk(preinstall_dir):
            for file_name in files:
                p = re.compile(r'\S*(?=.apk\b)')
                found = p.search(file_name)
                if found:
                    include_apk_path = preinstall_dir + '/' + found.group()
                    makefile_path = include_apk_path + '/Android.mk'
                    apk = preinstall_dir + '/' + found.group() + '.apk'
                    try:
                        zfile = zipfile.ZipFile(apk,'r')
                    except:
                        if os.path.exists(include_apk_path):
                            shutil.rmtree(include_apk_path)
                        os.makedirs(include_apk_path)
                        apkpath = preinstall_dir + '/' + found.group() + '/'
                        shutil.move(apk,apkpath)
                        makefile = file(makefile_path,'w')
                        makefile.write("LOCAL_PATH := $(my-dir)\n\n")
                        makefile.write(templet % (found.group(),argv[3],'None',MY_LOCAL_PREBUILT_JNI_LIBS,argv[3]))
                        continue
                    for lib_name in zfile.namelist():
                        include_apklib_path = include_apk_path + '/lib' + '/arm'
                        if os.path.exists(include_apk_path):
                            shutil.rmtree(include_apk_path)
                        os.makedirs(include_apklib_path)
                        makefile = file(makefile_path,'w')
                        makefile.write("LOCAL_PATH := $(my-dir)\n\n")
                        apkpath = preinstall_dir + '/' + found.group() + '/'
                    if target_arch == 'arm64':
                        for lib_name in zfile.namelist():
                            lib = re.compile(r'\A(lib/arm64-v8a/)+?')
                            find_name = 'lib/arm64-v8a/'
                            if lib_name.find(find_name) == -1:
                                continue
                            libfound = lib.search(lib_name)
                            if libfound:
                                isfound = 'arm64-v8a'
                                data = zfile.read(lib_name)
                                string = lib_name.split(libfound.group())
                                libfile = include_apklib_path + '/' + string[1]
                                MY_LOCAL_PREBUILT_JNI_LIBS += '\t' + 'lib/arm64' + '/' + string[1] + '\\' + '\n'
                                if (os.path.isdir(libfile)):
                                    continue
                                else:
                                    includelib = file(libfile, 'w')
                                    includelib.write(data)
                        try:
                            if cmp(isfound, 'not_found_lib'):
                                include_apklib_path_arm64 = include_apk_path + '/lib/arm64'
                                os.rename(include_apklib_path, include_apklib_path_arm64)
                        except Exception as e:
                            logging.warning('rename dir faild for:' + e)
                    if not cmp(isfound,'not_found_lib'):
                        for lib_name in zfile.namelist():
                            lib = re.compile(r'\A(lib/armeabi-v7a/)+?')
                            find_name = 'lib/armeabi-v7a/'
                            #if not cmp(lib_name,find_name):
                            #    continue
                            if lib_name.find(find_name) == -1:
                                continue
                            libfound = lib.search(lib_name)
                            if libfound:
                                isfound = 'armeabi-v7a'
                                data = zfile.read(lib_name)
                                string = lib_name.split(libfound.group())
                                libfile = include_apklib_path + '/' + string[1]
                                MY_LOCAL_PREBUILT_JNI_LIBS += '\t' + 'lib/arm' + '/' + string[1] + '\\' + '\n'
                                if(os.path.isdir(libfile)):
                                    continue
                                else:
                                    includelib = file(libfile,'w')
                                    includelib.write(data)
                    if not cmp(isfound,'not_found_lib'):
                        for lib_name in zfile.namelist():
                            lib = re.compile(r'\A(lib/armeabi/)+?')
                            find_name = 'lib/armeabi/'
                            #if not cmp(lib_name,find_name):
                            #    continue
                            if lib_name.find(find_name) == -1:
                                continue
                            libfound = lib.search(lib_name)
                            if libfound:
                                data = zfile.read(lib_name)
                                string = lib_name.split(libfound.group())
                                libfile = include_apklib_path + '/' + string[1]
                                MY_LOCAL_PREBUILT_JNI_LIBS += '\t' + 'lib/arm' + '/' + string[1] + '\\' + '\n'
                                if(os.path.isdir(libfile)):
				        continue
				else:
                                    includelib = file(libfile,'w')
                                    includelib.write(data)
                    tmp_jni_libs = '\\' + '\n'
                    if not cmp(MY_LOCAL_PREBUILT_JNI_LIBS,tmp_jni_libs):
                        nolibpath = preinstall_dir + '/' + found.group() + '/lib'
                        shutil.rmtree(nolibpath)
                        makefile.write(templet % (found.group(),argv[3],'None',MY_LOCAL_PREBUILT_JNI_LIBS,argv[3]))
                    else:
                        if isfound == 'arm64-v8a':
                            makefile.write(templet % (found.group(),argv[3], 'arm64', MY_LOCAL_PREBUILT_JNI_LIBS,argv[3]))
                        else:
                            makefile.write(templet % (found.group(),argv[3],'arm',MY_LOCAL_PREBUILT_JNI_LIBS,argv[3]))
                    shutil.move(apk,apkpath)
                    isfound = 'not_found_lib'
                    MY_LOCAL_PREBUILT_JNI_LIBS = '\\' + '\n'
                    makefile.close()
            break
        for root, dirs,files in os.walk(preinstall_dir):
            for dir_file in dirs:
                includefile.write('PRODUCT_PACKAGES += %s\n' %dir_file)
            break
        includefile.close()

if __name__=="__main__":
  main(sys.argv)
