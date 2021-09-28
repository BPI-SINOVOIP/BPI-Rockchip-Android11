#include <stdio.h>
#include <errno.h>
#include <cutils/properties.h>
#include <string.h>
#include <stdlib.h>
#include "handle.h"

#define MODULE_NAME     "net"
#define MODULE_VERSION     "V1.0"

static void usage(void);
static void dump_info(void);
static int shell(char *cmd, char *result);

static struct {
    char *property;
    char *info;
} system_property[] = {
    {"vendor.wifi.state","Wifi State"},
    {0,0},
};

static struct {
    char *cmd;
    char *info;
    void (*func)(char *value);
} system_node[] = {
    {"busybox ifconfig eth0","Ethernet Config : \n", enter_handle},
    {"busybox ifconfig wlan0","Wlan Config : \n", enter_handle},
    {0,0, NULL},
};

static struct {
    char *cmd;
    void (*func)(char *value);
} save_node[] = {
    {"mkdir -p /data/net/net_log", enter_handle},
    {"getprop |grep version > /data/net/net_log/allversion.txt", enter_handle},
    {"busybox cp /vendor/commit_id.xml /data/net/net_log/", enter_handle},
    {"dmesg > /data/net/net_log/dmesg.txt", enter_handle},
    {"logcat -d > /data/net/net_log/logcat.txt", enter_handle},
    {"getprop > /data/net/net_log/getprop.txt", enter_handle},
    {"busybox ifconfig > /data/net/net_log/ifconfig.txt", enter_handle},
    {"cd /data/net/;tar -zcvf net_log.tar.gz net_log/;cd -", enter_handle},
    {0, NULL},
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

void save_log(void)
{
    FILE *fstream=NULL;
    char cmd[1024 * 10];
    printf("Start: save net info log...\n");
    system("rm data/net/ -rf");
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
     printf("End:already save dump info to data/net/net_log.tar.gz\n");
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
      /*  if(system_node[i].func)
        {
            system_node[i].func(value);
        }
       */
        printf("%s%s \r\n",system_node[i].info, value);
        memset(value, 0, sizeof(value));
    }
}

static void usage(void)
{
    printf("Usage:\r\n");
    printf("       net  -log\n");
    printf("       net  -dump\n");
    printf("       net  -version\n");
    printf("       net  -help\n");
    printf("\n");
    printf("Miscellaneous:\n");
    printf("  -help             Print help information\n");
    printf("  -version          Print version information\n");
    printf("  -dump             Dump network info\n");
    printf("  -log              save network log to data/net/net_log.tar.gz\n");
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
   if(!strcmp(argv[1], "-log"))
   {
       save_log();
       return 0;
   }
   else
   if(!strcmp(argv[1], "-dump"))
   {
       dump_info();
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
