/*
 * Copyright © 2016-2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * This program is intended for testing of validate GVT-g virtual machine
 * creation of allow for testing of KVMGT / XenGT (the tool should be root
 * privilege only).
 *
 * TODO:
 * Enable more GVT-g related test cases.
 */
#include "config.h"

#include "igt.h"
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <strings.h>
#include <unistd.h>
#include <termios.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <dirent.h>

#define RANDOM(x) (rand() % x)


static char uuid[40];
static char guest_ip[32];

static char mac_addr[20] = {0};
static char qemu_path[PATH_MAX] = {0};
static char hda_path[PATH_MAX] = {0};
static char bios_path[PATH_MAX] = {0};

static int super_system(const char *cmd, char *retmsg, int msg_len)
{
    FILE *fp;
    int res = -1;
    if (cmd == NULL || retmsg == NULL || msg_len < 0) {
        igt_info("Error: %s system parameter invalid!\n", __func__);
        return 1;
    }
    fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("popen");
        igt_info("Error: %s popen error: %s\n", __func__, strerror(errno));
        return 2;
    } else {
        memset(retmsg, 0, msg_len);
        while (fgets(retmsg, msg_len, fp)) {
            ;
        }
        res = pclose(fp);
        if (res == -1) {
            igt_info("Error:%s close popen file pointer fp error!\n", __func__);
            return 3;
        }
        retmsg[strlen(retmsg) - 1] = '\0';
        return 0;
    }
}

static int check_gvtg_support(void)
{
    DIR *mdevdir = opendir("/sys/bus/pci/devices/0000:00:02.0/"
            "mdev_supported_types");

    if (mdevdir == NULL) {
        return 1;
    }
    closedir(mdevdir);
    return 0;
}

static int check_tools(void)
{
    if (system("which uuidgen > /dev/null") != 0) {
        return 1;
    } else if (system("which arp-scan > /dev/null") != 0) {
        return 2;
    } else if (system("which /etc/qemu-ifup > /dev/null") != 0) {
        return 3;
    }
    return 0;
}

static void create_guest(void)
{
    unsigned int max_size_cmd = 4 * PATH_MAX;
    char *command;

    command = malloc(max_size_cmd);
    if (!command)
        return;

    sprintf(command, "qemu-img create -b %s -f qcow2 %s.qcow2",
            hda_path, hda_path);
    igt_assert_eq(system(command), 0);

    sprintf(command, "echo \"%s\" > /sys/bus/pci/devices/0000:00:02.0/"
           "mdev_supported_types/$(ls /sys/bus/pci/devices/0000:00:02.0/"
           "mdev_supported_types |awk {'print $1'}|tail -1)/create", uuid);
    igt_assert_eq(system(command), 0);

    sprintf(command, "%s -m 2048 -smp 2 -M pc -name gvtg_guest"
           " -hda %s.qcow2 -bios %s -enable-kvm --net nic,macaddr=%s -net"
           " tap,script=/etc/qemu-ifup -vga cirrus -k en-us"
           " -serial stdio -vnc :1 -machine kernel_irqchip=on -global"
           " PIIX4_PM.disable_s3=1 -global PIIX4_PM.disable_s4=1 -cpu host"
           " -usb -usbdevice tablet -device vfio-pci,sysfsdev="
           "/sys/bus/pci/devices/0000:00:02.0/%s &",
           qemu_path, hda_path, bios_path, mac_addr, uuid);
    igt_assert_eq(system(command), 0);

    free(command);
}

static void destroy_all_guest(void)
{
	int ret = system("pkill qemu");

	igt_assert(ret >= 0 && WIFEXITED(ret));
	igt_assert(WEXITSTATUS(ret) == 0 || WEXITSTATUS(ret) == 1);
}

static void remove_vgpu(void)
{
    char cmd[128] = {0};
    sprintf(cmd, "echo 1 > /sys/bus/pci/devices/0000:00:02.0/%s/remove", uuid);
    igt_assert_eq(system(cmd), 0);
}

static void gen_mac_addr(void)
{
    srand(getpid());
    sprintf(mac_addr, "52:54:00:%02X:%02X:%02X", RANDOM(256), RANDOM(256),
RANDOM(256));
}

static void gen_uuid(void)
{
    igt_assert_eq(super_system("uuidgen", uuid, sizeof(uuid)), 0);
}

static char *fetch_ip_by_mac(char *mac)
{
    char cmd[1024] = {0};
    sprintf(cmd, "arp-scan -l -I $(ip addr show|grep inet|grep global|"
        "awk '{print $NF;}')|grep -i %s|awk '{print $1}'", mac);
    igt_assert_eq(super_system(cmd, guest_ip, sizeof(guest_ip)), 0);
    return guest_ip;
}

static int check_guest_ip(void)
{
    int timeout = 0;
    while (timeout <= 12) {
        igt_info("Trying to connect guest, attempt %d.\n", timeout);
        if (timeout == 12) {
            igt_info("Cannot connect to guest.\n");
            return 1;
            break;
        }

        fetch_ip_by_mac(mac_addr);

        if (guest_ip[0] != '\0') {
            igt_info("Fetched guest ip address: %s.\n", guest_ip);
            break;
        }
        timeout++;
        sleep(5);
    }
    return 0;
}

static void clear_dmesg(void)
{
    igt_assert_eq(system("dmesg -c > /dev/null"), 0);
}

static int check_dmesg(void)
{
    char errorlog[PATH_MAX] = {0};

    igt_assert_eq(super_system("dmesg|grep -E \"GPU HANG|gfx reset|BUG\"",
                errorlog, sizeof(errorlog)), 0);

    if (errorlog[0] == '\0') {
        return 0;
    } else {
        return 1;
    }
}

static void print_help(void)
{
    igt_info("\n[options]\n"
          "-h, --help     display usage\n"
          "-q, --qemu     the qemu path\n"
          "-a, --hda      the hda raw image / qcow path\n"
          "-b, --bios     the seabios path\n\n"
          "[example]\n"
          " ./intel_gvtg_test -q /usr/bin/qemu-system-x86_64 -a "
          "/home/img/ubuntu-16.04.img -b /usr/bin/bios.bin\n"
          );
}

static void arg_mismatch(const char *arg)
{
    igt_info("argument mismatch: %s\n", arg);
}

int main(int argc, char *argv[])
{
    int opt = -1;
    const char *optstring = "hq:a:b:";
    static struct option long_options[] = {
        {"help", 0, NULL, 'h'},
        {"qemu", 1, NULL, 'q'},
        {"hda",  1, NULL, 'a'},
        {"bios", 1, NULL, 'b'},
        {0, 0, 0, 0}
    };

    int ret = 0;
    int flag_cnt = 0;
    int h_flag = 0;
    int q_flag = 0;
    int a_flag = 0;
    int b_flag = 0;
    int tools_status = 0;

    igt_skip_on_f(check_gvtg_support() == 1,
		  "GVT-g technology is not supported in your system.\n");

    tools_status = check_tools();
    igt_skip_on_f(tools_status == 1, "Please install the \"uuid-runtime\" tool.\n");
    igt_skip_on_f(tools_status == 2, "Please install the \"arp-scan\" tool.\n");
    igt_skip_on_f(tools_status == 3, "Please prepare the \"qemu-ifup\" script.\n");

    if (argc == 1) {
        print_help();
        return 0;
    }

    while ((opt = getopt_long(argc, argv, optstring, long_options, NULL))
            != -1) {
        switch (opt) {
        case 'h':
            h_flag = 1;
            break;
        case 'q':
            strcpy(qemu_path, optarg);
            q_flag = 1;
            break;
        case 'a':
            strcpy(hda_path, optarg);
            a_flag = 1;
            break;
        case 'b':
            strcpy(bios_path, optarg);
            b_flag = 1;
            break;
        case ':':
            ret = -1;
            break;
        case '?':
            ret = -1;
            break;
        }

        if (ret) {
            break;
        }
    };

    if (ret != 0) {
        return ret;
    }

    flag_cnt = h_flag + q_flag + a_flag + b_flag;

    if (flag_cnt == 1) {
        if (h_flag == 1) {
            print_help();
            return 0;
        } else {
            arg_mismatch(argv[1]);
            return -1;
        }
    } else if (flag_cnt == 3) {
        if (q_flag == 1 && a_flag == 1 && b_flag == 1) {
            igt_info("\nqemu_path: %s\nhda_path: %s\nbios_path: %s\n",
                    qemu_path, hda_path, bios_path);
        } else {
            arg_mismatch("-h");
            return -1;
        }
    } else {
        arg_mismatch(argv[1]);
        return -1;
    }

    destroy_all_guest();
    clear_dmesg();

    gen_mac_addr();
    gen_uuid();
    create_guest();

    if (check_guest_ip()) {
        ret = 1;
    }

    destroy_all_guest();
    sleep(5);
    remove_vgpu();

    if (check_dmesg() > 0) {
        ret = 1;
    }

    igt_assert_eq(ret, 0);
    igt_exit();

}

