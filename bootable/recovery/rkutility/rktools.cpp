/*************************************************************************
	> File Name: rktools.cpp
	> Author: jkand.huang
	> Mail: jkand.huang@rock-chips.com
	> Created Time: Mon 23 Jan 2017 02:36:42 PM CST
 ************************************************************************/

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
//#include <fs_mgr.h>
//#include "common.h"
#include <cutils/properties.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string>

#include <mntent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>

#include <string>
#include <vector>



#include "rkutility/rktools.h"
#include "rkutility/sdboot.h"



//static bool isLedFlash = false;
//static pthread_t tid;
//static int last_state = 0;

using namespace std;

struct MountedVolume {
  std::string device;
  std::string mount_point;
  std::string filesystem;
  std::string flags;
};


/**
 * 从/proc/cmdline 获取串口的节点
 *
*/
char *getSerial(){
    char *ans = (char*)malloc(20);
    char param[1024];
    int fd, ret;
    char *s = NULL;
    fd = open("/proc/cmdline", O_RDONLY);
    ret = read(fd, (char*)param, 1024);
    printf("cmdline=%s\n",param);
    s = strstr(param,"console");
    if(s == NULL){
        printf("no found console in cmdline\n");
        free(ans);
        ans = NULL;
        return ans;
    }else{
        s = strstr(s, "=");
        if(s == NULL){
            free(ans);
            ans = NULL;
            return ans;
        }

        strcpy(ans, "/dev/");
        char *str = ans + 5;
        s++;
        while(*s != ' '){
            *str = *s;
            str++;
            s++;
        }
        *str = '\0';
        printf("read console from cmdline is %s\n", ans);
    }

    return ans;
}


#if 0
void *thrd_led_func(void *arg) {
    FILE * ledFd = NULL;
    bool onoff = false;
    char real_net_file_path[128] = "\0";
    if((ledFd = fopen(NET_FILE_PATH, "w")) != NULL){
        strcpy(real_net_file_path, NET_FILE_PATH);
        fclose(ledFd);
    }else if((ledFd = fopen(NET_FILE_PATH_NEW, "w")) != NULL){
        strcpy(real_net_file_path, NET_FILE_PATH_NEW);
        fclose(ledFd);
    }

    while(isLedFlash) {
        ledFd = fopen(real_net_file_path, "w");
        if(ledFd == NULL)
        {
            usleep(500 * 1000);
            continue;
        }
        if(onoff) {
            fprintf(ledFd, "%d", OFF_VALUE);
            onoff = false;
        }else {
            fprintf(ledFd, "%d", ON_VALUE);
            onoff = true;
        }

        fclose(ledFd);
        usleep(500 * 1000);
    }

    printf("stopping led thread, close led and exit\n");

    ledFd = fopen(real_net_file_path, "w");
    if(ledFd != NULL){
        fprintf(ledFd, "%d", last_state);
        fclose(ledFd);
    }
    pthread_exit(NULL);
    return NULL;
}

void startLed() {
    isLedFlash = true;
    if (pthread_create(&tid,NULL,thrd_led_func,NULL)!=0) {
        printf("Create led thread error!\n");
    }

    printf("tid in led pthread: %ld.\n",tid);
}

void stopLed(int state) {
    last_state = state;
    void *tret;
    isLedFlash = false;

    if (pthread_join(tid, &tret)!=0){
        printf("Join led thread error!\n");
    }else {
        printf("join led thread success!\n");
    }
}
#endif

/**
 *  设置flash 节点
 */
static char result_point[4][20]; //0-->emmc, 1-->sdcard, 2-->SDIO, 3-->SDcombo
int readFile(DIR* dir, char* filename){
    char name[30] = {'\0'};
    strcpy(name, filename);
    strcat(name, "/type");
    int fd = openat(dirfd(dir), name, O_RDONLY);
    if(fd == -1){
        printf("Error: openat %s error %s.\n", name, strerror(errno));
        return -1;
    }
    char resultBuf[10] = {'\0'};
    read(fd, resultBuf, sizeof(resultBuf));
    for(int i = 0; i < (int)strlen(resultBuf); i++){
        if(resultBuf[i] == '\n'){
            resultBuf[i] = '\0';
            break;
        }
    }
    for(int i = 0; i < 4; i++){
        if(strcmp(typeName[i], resultBuf) == 0){
            //printf("type is %s.\n", typeName[i]);
            return i;
        }
    }

    printf("Error:no found type!\n");
    return -1;
}

void init_sd_emmc_point(){
    DIR* dir = opendir("/sys/bus/mmc/devices/");
    if(dir != NULL){
		printf("exit=================\n");
        memset(result_point, 0, 4*20);
        dirent* de;
        while((de = readdir(dir))){
            if(strncmp(de->d_name, "mmc", 3) == 0){
                //printf("find mmc is %s.\n", de->d_name);
                char flag = de->d_name[3];
                int ret = -1;
                ret = readFile(dir, de->d_name);
                if(ret != -1){
                    strcpy(result_point[ret], point_items[flag - '0']);
                }else{
                    strcpy(result_point[ret], "");
                }
                printf("result_point[%d] is ----%s\n",ret,result_point[ret]);
            }
        }

    }
    closedir(dir);
}

void setFlashPoint(){
    init_sd_emmc_point();
    setenv(EMMC_POINT_NAME, result_point[MMC], 1);
    //SDcard 有两个挂载点
	printf("111\n");
    if(access(result_point[SD], F_OK) == 0)
        setenv(SD_POINT_NAME_2, result_point[SD], 1);
    char name_t[22];
    if(strlen(result_point[SD]) > 0){
        strcpy(name_t, result_point[SD]);
        strcat(name_t, "p1");
    }
    if(access(name_t, F_OK) == 0)
        setenv(SD_POINT_NAME, name_t, 1);

    printf("emmc_point is %s\n", getenv(EMMC_POINT_NAME));
    printf("sd_point is %s\n", getenv(SD_POINT_NAME));
    printf("sd_point_2 is %s\n", getenv(SD_POINT_NAME_2));
}

void dumpCmdArgs(int argc, char** argv) {
    fprintf(stdout, "=== start %s:%d ===\n", __func__, __LINE__);
    for(int i = 0; i < argc; i++)
    {
        fprintf(stdout, "argv[%d] =  %s.\n", i, argv[i]);
    }
}

int getEmmcState() {
    char bootmode[256];
    int result = 0;

    property_get("ro.boot.mode", bootmode, "unknown");
    printf("ro.boot.mode = %s \n", bootmode);

    if(!strcmp(bootmode, "nvme")) {
        result = 2;
    } else if(!strcmp(bootmode, "emmc")) {
        result = 1;
    }else {
        result = 0;
    }

    return result;
}

/**
 * 分割字符串
 */
void SplitString(const std::string& s, std::string& result, const std::string& c){
#if 0
    if(getEmmcState() != 0){
        result = s;
        return ;
    }
    std::string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while(std::string::npos != pos2){
        result = s.substr(pos1, pos2-pos1);
        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }
    if(pos1 != s.length())
        result = s.substr(pos1);
    result = "/dev/block/rknand_" + result;
#else
	result = s;
	(void)c;
#endif
    printf("SplitString :: result is %s.\n", result.c_str());
}

bool IsSpecialName(const char *str){
    if(getEmmcState() != 0){
        return false;
    }
    string fileName[5] = {"uboot.img", "trust.img", "resource.img", "recovery.img", "boot.img"};
    for(int i = 0; i < 5; i++){
        if(fileName[i].compare(str) == 0){
            printf("func is %s.\n", fileName[i].c_str());
            return true;
        }
    }
    return false;
}

static std::vector<MountedVolume*> g_mounts_state;

bool rktools_scan_mounted_volumes() {
  for (size_t i = 0; i < g_mounts_state.size(); ++i) {
    delete g_mounts_state[i];
  }
  g_mounts_state.clear();

  // Open and read mount table entries.
  FILE* fp = setmntent("/proc/mounts", "re");
  if (fp == NULL) {
    return false;
  }
  mntent* e;
  while ((e = getmntent(fp)) != NULL) {
    MountedVolume* v = new MountedVolume;
    v->device = e->mnt_fsname;
    v->mount_point = e->mnt_dir;
    v->filesystem = e->mnt_type;
    v->flags = e->mnt_opts;
    g_mounts_state.push_back(v);
  }
  endmntent(fp);
  return true;
}

MountedVolume* rktools_find_mounted_volume_by_mount_point(const char* mount_point) {
  for (size_t i = 0; i < g_mounts_state.size(); ++i) {
    if (g_mounts_state[i]->mount_point == mount_point) return g_mounts_state[i];
  }
  return nullptr;
}

int rktools_unmount_mounted_volume(MountedVolume* volume) {
  // Intentionally pass the empty string to umount if the caller tries to unmount a volume they
  // already unmounted using this function.
  std::string mount_point = volume->mount_point;
  volume->mount_point.clear();
  int result = umount(mount_point.c_str());
  if (result == -1) {
    printf("Failed to umount mount_point=%s \n", mount_point.c_str());
  }
  return result;
}
