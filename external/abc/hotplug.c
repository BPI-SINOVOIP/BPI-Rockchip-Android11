#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <blkid/blkid.h>
#include <string.h>
#include "misc.h"
#include "config.h"


#define UEVENT_MSG_LEN 4096
struct uevent
{
    const char *action;
    const char *path;
    const char *subsystem;
    const char *firmware;
    int major;
    int minor;
    const char *devname;
    const char *devtype;
};

static void parse_event(const char *msg, struct uevent *uevent);
static int open_uevent_socket(void);
int MonitorNetlinkUevent()
{
    int device_fd = -1;
    char msg[UEVENT_MSG_LEN+2];
    int n;

    device_fd = open_uevent_socket();
    printf("device_fd = %d\n", device_fd);

    do
    {
        while((n = recv(device_fd, msg, UEVENT_MSG_LEN, 0)) > 0)
        {
            struct uevent uevent;

            if(n == UEVENT_MSG_LEN)
                continue;

            msg[n] = '\0';
            msg[n+1] = '\0';

            parse_event(msg, &uevent);
        }
    }
    while(1);
}

static int open_uevent_socket(void)
{
    struct sockaddr_nl addr;
    int sz = 64*1024;
    int s;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0xffffffff;

    s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (s < 0)
        return -1;

    setsockopt(s, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));

    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        close(s);
        return -1;
    }

    return s;
}

static void parse_event(const char *msg, struct uevent *uevent)
{
    uevent->action = "";
    uevent->path = "";
    uevent->subsystem = "";
    uevent->firmware = "";
    uevent->major = -1;
    uevent->minor = -1;
    uevent->devname = "";
    uevent->devtype = "";
    const char *puuid;
    char path_sd[60] = LOG_SD_PATH;
    printf("========================================================\n");
    while (*msg)
    {

        printf("%s\n", msg);

        if (!strncmp(msg, "ACTION=", 7))
        {
            msg += 7;
            uevent->action = msg;
        }
        else if (!strncmp(msg, "DEVPATH=", 8))
        {
            msg += 8;
            uevent->path = msg;
        }
        else if (!strncmp(msg, "SUBSYSTEM=", 10))
        {
            msg += 10;
            uevent->subsystem = msg;
        }
        else if (!strncmp(msg, "FIRMWARE=", 9))
        {
            msg += 9;
            uevent->firmware = msg;
        }
        else if (!strncmp(msg, "MAJOR=", 6))
        {
            msg += 6;
            uevent->major = atoi(msg);
        }
        else if (!strncmp(msg, "MINOR=", 6))
        {
            msg += 6;
            uevent->minor = atoi(msg);
        }
        else if (!strncmp(msg, "DEVNAME=", 8))
        {
            msg += 8;
            uevent->devname = msg;
        }
        else if (!strncmp(msg, "DEVTYPE=", 8))
        {
            msg += 8;
            uevent->devtype = msg;
        }
        while(*msg++)
            ;
    }
    /*ÊÇ·ñsd¿¨²åÈë*/
    printf("event { action = '%s', path = '%s', subsystem = '%s', firmware = '%s', major = %d, minor = %d , devname = '%s', devtype = '%s'}\n",
           uevent->action, uevent->path, uevent->subsystem,
           uevent->firmware, uevent->major, uevent->minor, uevent->devname, uevent->devtype);
    if (!strncmp(uevent->action, "add",3) && !strncmp(uevent->subsystem, "block",5) && !strncmp(uevent->devname, "mmcblk0p1",9) \
        && !strncmp(uevent->devtype,"partition",9))
    {
        sleep(2);
        blkid_cache cache;
        blkid_get_cache(&cache, "/dev/null");
        puuid = blkid_get_tag_value(cache, "UUID", "/dev/block/mmcblk0p1");
        blkid_put_cache(cache);
        printf("The puuid by probing is %s\n", puuid==NULL?"<dont know>":puuid);

        strcat(path_sd,puuid);
        printf("The path of sdcard is %s\n",path_sd);
        copy_all_logs_to_storage(path_sd);
    }
}

