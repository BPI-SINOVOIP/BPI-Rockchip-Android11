#include <stdio.h>
#include <errno.h>
#include <cutils/properties.h>
#include <string.h>
#include <stdlib.h>
#include "handle.h"

#define MODULE_NAME     "deviceinfo"
#define MODULE_VERSION     "V0.5"

static void usage(void);
static void dump_info(void);
static int shell(char *cmd, char *result);

static struct {
    char *property;
    char *info;
} system_property[] = {
    {"ro.product.name","Device Name"},
    {"ro.product.model","Device Model"},
    {"ro.target.product","Product Type"},
    {"ro.serialno","Serial Number"},
    {"ro.build.version.release","Android Version"},
    {"ro.build.version.sdk","APILevel"},
    {"ro.build.date","Build Time"},
    {"ro.build.type","Build Type"},
    {"ro.build.version.incremental","Build Version"},
    {"ro.vendor.build.security_patch","Security Patch Level"},
    {"ro.com.google.gmsversion","GMS Version"},
    {"ro.com.google.gtvsversion","GTVS Version"},
    {"persist.vendor.framebuffer.main","Screen Reslution"},
    {"ro.sf.lcd_density","Screen Density"},
    {"ro.rksdk.version","SDK Version"},
    {"ro.product.cpu.abilist","CPU abi"},
    {"ro.boot.selinux","Selinux"},
    {"ro.boot.storagemedia","Flash type"},
    {0,0},
};

static struct {
    char *cmd;
    char *info;
    void (*func)(char *value);
} system_node[] = {
    {"dmesg|grep GiB |awk '{ print $5 $6 $7 }'","Emmc Size : ", enter_handle},//8GB
    {"cat d/mmc2/ios |head -8|tail -1|awk '{ print $4 $5 }'","Emmc Timing : ", enter_handle},//hs200
    {"cat d/mmc2/clock","Emmc Freqs : ", enter_handle},//150MHZ
    {"cat /sys/kernel/debug/clk/clk_wifi/clk_rate","Wifi Freqs : ", enter_handle},//2.4G
    {"cat /sys/bus/sdio/devices/mmc1:0001:1/vendor","Wifi Vendor ID : ", enter_handle},//
    {"cat /sys/bus/sdio/devices/mmc1:0001:1/device","Wifi Device ID : ", enter_handle},//
    {"cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies","CPU Freqs : ", enter_handle},
    {"cat /sys/class/devfreq/*.gpu/available_frequencies","GPU Freqs : ", enter_handle},
    {"cat /sys/class/devfreq/dmc/available_frequencies","DDR Freqs : ", enter_handle},
    {"cat /proc/meminfo | grep MemTotal", "", enter_handle},
    {"dumpsys uimode|grep mCurUiMode", "UiMode:", enter_handle},
    {0,0, NULL},
};

static struct {
    char *cmd;
    void (*func)(char *value);
} save_node[] = {
    {"mkdir -p /data/deviceinfo/dumpinfo", enter_handle},
    {"getprop |grep version > /data/deviceinfo/dumpinfo/allversion.txt", enter_handle},
    {"dumpsys meminfo > /data/deviceinfo/dumpinfo/meminfo.txt", enter_handle},
    {"busybox cp /vendor/commit_id.xml /data/deviceinfo/dumpinfo/", enter_handle},
    {"dmesg > /data/deviceinfo/dumpinfo/dmesg.txt", enter_handle},
    {"logcat -d > /data/deviceinfo/dumpinfo/logcat.txt", enter_handle},
    {"getprop > /data/deviceinfo/dumpinfo/getprop.txt", enter_handle},
    {"bugreport > /data/deviceinfo/dumpinfo/bugreport.txt", enter_handle},
    {"cd /data/deviceinfo/;tar -zcvf dumpinfo-device.tar.gz dumpinfo/;cd -", enter_handle},
    {0, NULL},
};

static struct {
    char *cmd;
    char *info;
    void (*func)(char *value);
} dvfs_node[] = {
    {"cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq","CPU CurFreq :", enter_handle},
    {"cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies","CPU Freqs :", enter_handle},
    {"cat /sys/class/devfreq/*.gpu/cur_freq","GPU CurFreq : ", enter_handle},
    {"cat /sys/class/devfreq/*.gpu/available_frequencies","GPU Freqs :", enter_handle},
    {"cat /sys/class/devfreq/dmc/cur_freq","DDR CurFreq :", enter_handle},
    {"cat /sys/class/devfreq/dmc/available_frequencies","DDR Freqs :", enter_handle},
    {"cat /sys/kernel/debug/clk/clk_rga/clk_rate","RGA Freqs : ", enter_handle},
    {"cat /sys/kernel/debug/clk/aclk_vpu/clk_rate","VPU Freqs : ", enter_handle},
    {"cat d/mmc2/clock","Emmc Freqs : ", enter_handle},
    //{"cat /sys/kernel/debug/regulator/regulator_summary","Regulator Summary : \n", enter_handle},
    {"cat /d/opp/opp_summary","OPP Summary :\n", enter_handle},
    {0,0, NULL},
};

static int shell(char *cmd, char *result)
{
    FILE *fstream=NULL;
    char buff[1024];
    memset(buff,0,sizeof(buff));
    if(NULL==(fstream=popen(cmd,"r")))
    {
        fprintf(stderr,"execute command failed: %s",strerror(errno));
        return -1;
    }
    while(NULL!=fgets(buff, sizeof(buff), fstream)) {
        strcat(result, buff);
        memset(buff, 0, sizeof(buff));
    }
//        printf("result:%s",result);
    pclose(fstream);
    return 0;
}

void dump_drm_info()
{
  char value[1024];
  if(shell("getprop drm.service.enabled", value) == 0)
  {
      if(strstr(value,"true")){
         printf("DRM Support: true\n");
         memset(value, 0, sizeof(value));
         //widewine drm info
         if(shell("test -e vendor/lib/libRkWvClient.so && echo L1", value) == 0)
         {
            if(!strcmp(value,"L1")){
                printf("WideWine DRM Libs:L1\n");
            } else {
                printf("WideWine DRM Libs:L3\n");
            }
         }
         //playready drm info
         memset(value, 0, sizeof(value));
         if(shell("test -e vendor/lib/mediadrm/libplayreadydrmplugin.so && echo Support", value) == 0)
         {
            if(!strcmp(value,"Support")){
                memset(value, 0, sizeof(value));
                shell("test -e vendor/lib/optee_armtz/d71d2527-5741-40a9-9ef51a2ece05631d.ta && echo SL3000", value);
                if(!strcmp(value,"SL3000")){
                   printf("PlayReady DRM Libs:SL3000\n");
                } else {
                   printf("PlayReady DRM Libs:SL2000\n");
                }
            } else {
                printf("PlayReady DRM Libs: Unsupport\n");
            }
         }
      } else {
         printf("DRM Support: false\n");

      }
  }
}

void save_last_log(void)
{
    FILE *fstream=NULL;
    char cmd[1024 * 1024];
    memset(cmd, 0, sizeof(cmd));
    printf("==========================Print last log console start==================================\n");
    if(shell("cat /sys/fs/pstore/console-ramoops-0",cmd)!=0){
       sprintf(cmd," ");
    }
    printf("%s \n",cmd);
    memset(cmd, 0, sizeof(cmd));
    printf("==========================Print last log console end==================================\n\n");
    printf("==========================Print last log android start==================================\n");
    if(shell("logcat -L",cmd)!=0){
       sprintf(cmd," ");
    }
    printf("%s \n",cmd);
    memset(cmd, 0, sizeof(cmd));
    printf("==========================Print last log android end==================================\n");
}

void save_system_log(void)
{
    FILE *fstream=NULL;
    char cmd[1024 * 10];
    printf("Start: save dump info log,need few minutes...\n");
    system("rm data/deviceinfo/dumpinfo -rf");
    for(int i=0; save_node[i].cmd; i++)
    {
       //printf("cmd:%s",save_node[i].cmd);
       if(NULL==(fstream=popen(save_node[i].cmd,"r")))
       {
               fprintf(stderr,"execute command failed: %s",strerror(errno));
               //return;
       }
       pclose(fstream);
       fstream=NULL;
     }
     printf("End:already save dump info to data/deviceinfo/dumpinfo-device.tar.gz\n");
}

static void dump_info(void)
{
    char value[1024 * 10];
    for(int i=0; system_property[i].property; i++)
    {
        property_get(system_property[i].property, value, "");
        printf("%s : %s \r\n",system_property[i].info,value);
        memset(value, 0, sizeof(value));
    }

    for(int i=0; system_node[i].cmd; i++)
    {
        if(shell(system_node[i].cmd, value) != 0)
        {
            sprintf(value," ");
        }
        if(system_node[i].func)
        {
            system_node[i].func(value);
        }

        printf("%s%s \r\n",system_node[i].info, value);
        memset(value, 0, sizeof(value));
    }
    dump_drm_info();
    //save_dump_info();
}

static void dvfs_info(void)
{
    char value[102400];
    memset(value, 0, sizeof(value));
    for(int i=0; dvfs_node[i].cmd; i++)
    {
        if(shell(dvfs_node[i].cmd, value) != 0)
        {
            sprintf(value," ");
        }

/*        if(dvfs_node[i].func)
        {
            dvfs_node[i].func(value);
        }
*/
        printf("%s%s \r\n",dvfs_node[i].info, value);
        memset(value, 0, sizeof(value));
    }
}

static void usage(void)
{
    printf("Usage:\r\n");
    printf("       deviceinfo  -devicetest \n");
    printf("       deviceinfo  -stresstest\n");
    printf("       deviceinfo  -log\n");
    printf("       deviceinfo  -lastlog\n");
    printf("       deviceinfo  -dump\n");
    printf("       deviceinfo  -dvfs\n");
    printf("       deviceinfo  -help\n");
    printf("\n");
    printf("Miscellaneous:\n");
    printf("  -help             Print help information\n");
    printf("  -version          Print version information\n");
    printf("  -dvfs             Dump kernel dvfs info\n");
    printf("  -dump             Dump system info\n");
    printf("  -log              save system log to data/deviceinfo\n");
    printf("  -lastlog          Print device lastlog \n");
    printf("  -stresstest       start stresstest\n");
    printf("  -devicetest       start devicetest (agingtest)\n");
}

int main(int argc, char **argv)
{

   int i = 0;

    if(argc < 2)
    {
        printf("%s: Need 2 arguments (see \" %s -help\")\n", MODULE_NAME, MODULE_NAME);
        return 0;
    }

#ifdef LOG_DEBUG
   printf("system - argc = %d \r\n",argc);
   for(i=0;i<argc;i++)
   {
       printf("i = %d  value = %s \r\n",i, argv[i]);
   }
#endif
   if(!strcmp(argv[1],"-version"))
   {
        printf("Version: %s\r\n",MODULE_VERSION);
        return 0;
   }
   else
   if(!strcmp(argv[1], "-stresstest"))
   {
       system("am start -a android.rk.intent.action.startStressTest");
       return 0;
   }
   else
   if(!strcmp(argv[1], "-devicetest"))
   {
       system("am start -a rk.intent.action.startDevicetest");
       return 0;
   }
   else
   if(!strcmp(argv[1], "-log"))
   {
       save_system_log();
       return 0;
   }
   else
   if(!strcmp(argv[1], "-lastlog"))
   {
       save_last_log();
       return 0;
   }
   else
   if(!strcmp(argv[1], "-dump"))
   {
       dump_info();
       return 0;
   }
   else if(!strcmp(argv[1], "-dvfs"))
   { 
       dvfs_info();
       return 0;
   }
   else if(!strcmp(argv[1], "-help"))
   {
       usage();
       return 0;
   }

   printf("%s: no such. (see \" %s -help\")\n", MODULE_NAME, MODULE_NAME);

   return 0;
}
