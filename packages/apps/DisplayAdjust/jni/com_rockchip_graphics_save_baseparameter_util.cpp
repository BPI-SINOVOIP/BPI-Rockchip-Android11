//
// Created by ly on 2021/5/26.
//
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include "com_rockchip_graphics_save_baseparameter_util.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG , "outputImage", __VA_ARGS__)    // DEBUG
#define BASEPARAMETER_IMAGE_SIZE 1024*1024

static char const *const device_template[] =
{
    "/dev/block/platform/1021c000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/30020000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/fe330000.sdhci/by-name/baseparameter",
    "/dev/block/platform/ff520000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/ff0f0000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/30030000.nandc/by-name/baseparameter",
    "/dev/block/rknand_baseparameter",
    "/dev/block/by-name/baseparameter",
    NULL
};

const char* get_baseparameter_file() {
    int i = 0;
    while (device_template[i]) {
        if (!access(device_template[i], R_OK | W_OK))
            return device_template[i];
        i++;
    }
    return NULL;
}

int dump_baseparameter(const char *file_path) {
    int file;
    int ret;
    const char *baseparameterfile = get_baseparameter_file();
    if (!baseparameterfile) {
        sync();
        return -ENOENT;
    }
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        LOGD("base paramter file can not be opened \n");
        sync();
        return -EIO;
    }
    char *data = (char *)malloc(BASEPARAMETER_IMAGE_SIZE);
    lseek(file, 0L, SEEK_SET);
    ret = read(file, data, BASEPARAMETER_IMAGE_SIZE);
    if (ret < 0) {
        LOGD("fail to read");
        close(file);
        free(data);
        return -EIO;
    }
    close(file);
    file = open(file_path, O_CREAT | O_WRONLY, 0666);
    LOGD("open %s file %d errno %d", file_path, file, errno);
    if (file < 0) {
        LOGD("fail to open");
        free(data);
        return -EIO;
    }
    lseek(file, BASEPARAMETER_IMAGE_SIZE-1, SEEK_SET);
    ret = write(file, "\0", 1);
    if (ret < 0) {
        LOGD("fail to write");
        close(file);
        free(data);
        return -EIO;
    }
    lseek(file, 0L, SEEK_SET);
    ret = write(file, (char*)data, BASEPARAMETER_IMAGE_SIZE);
    fsync(file);
    close(file);
    free(data);
    LOGD("dump_baseparameter %s success\n", file_path);

    return 0;
}

JNIEXPORT jint JNICALL Java_com_rockchip_graphics_SaveBaseParameterUtil_outputImage
  (JNIEnv * env, jclass clazz, jstring path)
  {
    const char* mPath = env->GetStringUTFChars(path, NULL);
    int ret = dump_baseparameter(mPath);
    env->ReleaseStringUTFChars(path, mPath);
    return ret;
  }

