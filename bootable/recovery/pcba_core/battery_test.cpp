#define LOG_TAG "Battery"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/ioctl.h>
#include <pthread.h>
#include "language.h"
#include "test_case.h"
#include "battery_test.h"

static  int BATTERY_STATUS_UNKNOWN = 1;
static  int BATTERY_STATUS_CHARGING = 2;
static  int BATTERY_STATUS_DISCHARGING = 3;
static  int BATTERY_STATUS_NOT_CHARGING = 4;
static  int BATTERY_STATUS_FULL = 5;

#define POWER_SUPPLY_PATH "/sys/class/power_supply"

struct PowerSupplyPaths {
    char* acOnlinePath;
    char* usbOnlinePath;
    char* wirelessOnlinePath;
    char* batteryStatusPath;
    char* batteryHealthPath;
    char* batteryPresentPath;
    char* batteryCapacityPath;
    char* batteryVoltagePath;
    char* batteryTemperaturePath;
    char* batteryTechnologyPath;
};

struct PowerSupplyPaths gPaths;

int getBatteryVoltage(const char* status)
{
    return atoi(status);
}

int getBatteryStatus(const char* status)
{    
    switch (status[0]) 
    {        
        case 'C': return BATTERY_STATUS_CHARGING;         // Charging        
        case 'D': return BATTERY_STATUS_DISCHARGING;      // Discharging        
        case 'F': return BATTERY_STATUS_FULL;             // Full        
        case 'N': return BATTERY_STATUS_NOT_CHARGING;      // Not charging        
        case 'U': return BATTERY_STATUS_UNKNOWN;          // Unknown                    
        default: {            
            printf("Unknown battery status '%s'", status);            
            return -1;        
        }    
    }
}

static int readFromFile(const char* path, char* buf, size_t size)
{    
    if (!path) {
        printf("Invalid path!");
        return -1;
    }
    int fd = open(path, O_RDONLY, 0);    
    if (fd == -1) 
    {        
        printf("Could not open '%s'", path);        
        return -1;    
    }        
    ssize_t count = read(fd, buf, size);    
    if (count > 0) 
    {        
        while (count > 0 && buf[count-1] == '\n') {
            count--;
            buf[count] = '\0';
        }
    } else {        
        buf[0] = '\0';   
    }     
    close(fd);   
    return count;
}

int BatteryPathInit()
{
    char    path[PATH_MAX];
    struct dirent* entry;

    DIR* dir = opendir(POWER_SUPPLY_PATH);
    if (dir == NULL) {
        printf("Could not open %s\n", POWER_SUPPLY_PATH);
        return -1;
    } else {
        while ((entry = readdir(dir))) {
            const char* name = entry->d_name;
            // ignore "." and ".."
            if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0))) {
                continue;
            }
            char buf[20];
            // Look for "type" file in each subdirectory
            snprintf(path, sizeof(path), "%s/%s/type", POWER_SUPPLY_PATH, name);
            int length = readFromFile(path, buf, sizeof(buf));
            if (length > 0) {
                if (buf[length - 1] == '\n') buf[length - 1] = 0;
                if (strcmp(buf, "Mains") == 0) {
                    snprintf(path, sizeof(path), "%s/%s/online", POWER_SUPPLY_PATH, name);
                    if (access(path, R_OK) == 0) gPaths.acOnlinePath = strdup(path);
                } else if (strcmp(buf, "USB") == 0) {
                    snprintf(path, sizeof(path), "%s/%s/online", POWER_SUPPLY_PATH, name);
                    if (access(path, R_OK) == 0) gPaths.usbOnlinePath = strdup(path);
                } else if (strcmp(buf, "Wireless") == 0) {
                    snprintf(path, sizeof(path), "%s/%s/online", POWER_SUPPLY_PATH, name);
                    if (access(path, R_OK) == 0) gPaths.wirelessOnlinePath = strdup(path);
                } else if (strcmp(buf, "Battery") == 0) {
                    snprintf(path, sizeof(path), "%s/%s/status", POWER_SUPPLY_PATH, name);
                    if (access(path, R_OK) == 0) gPaths.batteryStatusPath = strdup(path);

                    snprintf(path, sizeof(path), "%s/%s/health", POWER_SUPPLY_PATH, name);
                    if (access(path, R_OK) == 0) gPaths.batteryHealthPath = strdup(path);

                    snprintf(path, sizeof(path), "%s/%s/present", POWER_SUPPLY_PATH, name);
                    if (access(path, R_OK) == 0) gPaths.batteryPresentPath = strdup(path);

                    snprintf(path, sizeof(path), "%s/%s/capacity", POWER_SUPPLY_PATH, name);
                    if (access(path, R_OK) == 0) gPaths.batteryCapacityPath = strdup(path);

                    snprintf(path, sizeof(path), "%s/%s/voltage_now", POWER_SUPPLY_PATH, name);
                    if (access(path, R_OK) == 0) {
                        gPaths.batteryVoltagePath = strdup(path);   
                    } else {
                        snprintf(path, sizeof(path), "%s/%s/batt_vol", POWER_SUPPLY_PATH, name);
                        if (access(path, R_OK) == 0) gPaths.batteryVoltagePath = strdup(path);
                    }

                    snprintf(path, sizeof(path), "%s/%s/temp", POWER_SUPPLY_PATH, name);
                    if (access(path, R_OK) == 0) {
                        gPaths.batteryTemperaturePath = strdup(path);
                    } else {
                        snprintf(path, sizeof(path), "%s/%s/batt_temp", POWER_SUPPLY_PATH, name);
                        if (access(path, R_OK) == 0) gPaths.batteryTemperaturePath = strdup(path);
                    }

                    snprintf(path, sizeof(path), "%s/%s/technology", POWER_SUPPLY_PATH, name);
                    if (access(path, R_OK) == 0) gPaths.batteryTechnologyPath = strdup(path);
                }
            }
        }
        closedir(dir);
    }
    if (!gPaths.batteryStatusPath) {
        printf("%s is not Exist\n", gPaths.batteryStatusPath);
        return -1;
    }
    if (!gPaths.batteryVoltagePath) {
        printf("%s is not Exist\n", gPaths.batteryVoltagePath);
        return -1;
    }
    return 0;
}

void* battery_test(void *argv, display_callback *hook){
    char Voltagebuf[20];
    char Statusbuf[20];
    char Capacity[20];
    char Present[20];
    char ACOnline[20];
    char USBOnline[20];
    char usb_status[20];

    int ret;
    int result;
    int result_tmp;
    int yello_color = 255;
    int curprint;
    char *strbatstatus;
    char *strbatVoltage;
    char failed_msg[80];
    struct testcase_info *tc_info = (struct testcase_info*)argv;

    snprintf(failed_msg, sizeof(failed_msg), "%s:[%s]",PCBA_BATTERY, PCBA_FAILED);
    hook->handle_refresh_screen(tc_info->y, PCBA_BATTERY);

    if (BatteryPathInit() < 0)
    {
        printf("Failed to init Battery\n");
        hook->handle_refresh_screen_hl(tc_info->y, failed_msg, true);
        tc_info->result = -1;
        return argv;
    }
    /* Kernel 4.19 do not have present.
    memset(Present, 0, sizeof(Present));
    ret = readFromFile(gPaths.batteryPresentPath,
                       Present, sizeof(Present));
    if ((ret < 0) || (Present[0] == '0')) goto err;
    */

    memset(Voltagebuf, 0, sizeof(Voltagebuf));
    ret = readFromFile(gPaths.batteryVoltagePath,
                       Voltagebuf, sizeof(Voltagebuf));
    if (ret < 0 || (atoi(Voltagebuf) < 0)) goto err;

    memset(Capacity, 0, sizeof(Capacity));
    ret = readFromFile(gPaths.batteryCapacityPath,
                       Capacity, sizeof(Capacity));
    if (ret < 0 || (atoi(Capacity) < 0)) goto err;

    memset(Statusbuf, 0, sizeof(Statusbuf));
    ret = readFromFile(gPaths.batteryStatusPath,
                       Statusbuf, sizeof(Statusbuf));
    if ((ret < 0) || (getBatteryStatus(Statusbuf) < 0)) goto err;

    result = getBatteryStatus(Statusbuf);
    memset(ACOnline, 0, sizeof(ACOnline));
    ret = readFromFile(gPaths.acOnlinePath, ACOnline, sizeof(ACOnline));
    if (ret < 0) goto err;

    memset(USBOnline, 0, sizeof(USBOnline));
    ret = readFromFile(gPaths.usbOnlinePath,
                       USBOnline, sizeof(USBOnline));
    if (ret < 0) goto err;

    result_tmp = !(atoi(ACOnline) || atoi(USBOnline));
    usleep(200);

    for (;;)
    {
        memset(ACOnline, 0, sizeof(ACOnline));
        ret = readFromFile(gPaths.acOnlinePath,
                           ACOnline, sizeof(ACOnline));
        if (ret < 0) goto err;

        memset(USBOnline, 0, sizeof(USBOnline));
        ret = readFromFile(gPaths.usbOnlinePath,
                           USBOnline, sizeof(USBOnline));
        if(ret<0) goto err;
        if ((atoi(ACOnline) || atoi(USBOnline)) != result_tmp) {
            if (atoi(ACOnline) || atoi(USBOnline)) {
                memcpy(usb_status, PCBA_AC_ONLINE,
                       sizeof(PCBA_AC_ONLINE));
            } else {
                memcpy(usb_status, PCBA_AC_OFFLINE,
                       sizeof(PCBA_AC_OFFLINE));
            }
            memset(Statusbuf, 0, sizeof(Statusbuf));
            ret = readFromFile(gPaths.batteryStatusPath,
                               Statusbuf, sizeof(Statusbuf));
            if ((ret < 0) || (getBatteryStatus(Statusbuf) < 0)) break;

            result = getBatteryStatus(Statusbuf);
            memset(Voltagebuf, 0, sizeof(Voltagebuf));
            ret = readFromFile(gPaths.batteryVoltagePath,
                               Voltagebuf, sizeof(Voltagebuf));
            if (ret < 0 || (atoi(Voltagebuf) < 0)) goto err;

            memset(Capacity, 0, sizeof(Capacity));
            ret = readFromFile(gPaths.batteryCapacityPath,
                               Capacity, sizeof(Capacity));
            if (ret < 0 || (atoi(Capacity) < 0)) goto err;

            if (result == BATTERY_STATUS_CHARGING) {
                snprintf(failed_msg, sizeof(failed_msg), "%s:[%s] { %s,%s:%.1fV,%s:%d }",
                         PCBA_BATTERY, usb_status,
                         PCBA_BATTERY_CHARGE,
                         PCBA_BATTERY_VOLTAGE,
                         (double)(atoi(Voltagebuf))/1000/1000,
                         PCBA_BATTERY_CAPACITY, atoi(Capacity));
                tc_info->result = 0;
                /*return argv;*/
            } else if (result == BATTERY_STATUS_FULL) {
                snprintf(failed_msg, sizeof(failed_msg), "%s:[%s] { %s,%s:%.1fV,%s:%d }",
                         PCBA_BATTERY, usb_status,
                         PCBA_BATTERY_FULLCHARGE,
                         PCBA_BATTERY_VOLTAGE,
                         (double)(atoi(Voltagebuf))/1000/1000,
                         PCBA_BATTERY_CAPACITY, atoi(Capacity));
                tc_info->result = 0;
                /*return argv;*/
            } else {
                snprintf(failed_msg, sizeof(failed_msg), "%s:[%s] { %s,%s:%.1fV,%s:%d }",
                         PCBA_BATTERY, usb_status,
                         PCBA_BATTERY_DISCHARGE,
                         PCBA_BATTERY_VOLTAGE,
                         (double)(atoi(Voltagebuf))/1000/1000,
                         PCBA_BATTERY_CAPACITY, atoi(Capacity));
                tc_info->result = 0;
                /*return argv;*/
            }
            hook->handle_refresh_screen(tc_info->y, failed_msg);
            yello_color = 0;
            result_tmp = (atoi(ACOnline) || atoi(USBOnline));
        }
        usleep(200);
    }
err:
    hook->handle_refresh_screen_hl(tc_info->y, failed_msg, true);
    tc_info->result = -1;
    return argv;
 }

