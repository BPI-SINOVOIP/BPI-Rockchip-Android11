/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "config.h"
#include "misc.h"
#include "mail.h"
#include "hotplug.h"

#define KMSG_PATH   "/proc/kmsg"


int process_fd;
int android_fd;
int kernel_fd;

/*
 * upload log contain keywords
 */
static void *abc_upload(void *arg)
{
    char tar_command[128] = {0};

    int package_fd = 0;
    char package_path[PATH_MAX] = {0};
    struct stat stat_buf;
    unsigned long package_size = 0, base64_package_size = 0;
    char *package_buf = NULL, *base64_package_buf = NULL;
    int try_count = 0;

    /* package log directory contain keywords and delete it
     * then upload this log
     */
    if (trigger_upload)
    {
        trigger_upload = 0;
        sprintf(tar_command, "tar -cjf %s.tar.bz2 %s",
                latest_log_path, latest_log_path);

        /* call busybox tar command */
        if (system(tar_command) < 0)
        {
            printf("Tar command error!\nExit process...\n");
            exit(EXIT_FAILURE);
        }

        /* delete it */
        delete_dir(latest_log_path);
        sprintf(package_path, "%s.tar.bz2", latest_log_path);

        /* get *.tar.bz2 package size : stat_buf.st_size*/
        if((stat(package_path, &stat_buf)) < 0)
        {
            printf("Stat() system call error!\nCan't get file info!\nExit process...\n");
            exit(EXIT_FAILURE);
        }
        package_size = stat_buf.st_size;

        /* alloc memory for *.tar.bz2 package */
        package_buf = (char *)malloc(package_size);
        if (package_buf == NULL)
        {
            package_buf = NULL;
            printf("Package_buf memory allocate error!\nExit process...\n");
            exit(EXIT_FAILURE);
        }

        /* get *.tar.bz2 package size after encode with base64 */
        base64_package_size = package_size * 4 /  3 + 50;

        /* alloc memory for *.tar.bz2 package after encode with base64 */
        base64_package_buf = (char *)malloc(base64_package_size);
        if (base64_package_buf == NULL)
        {
            printf("Base64_package_buf memory allocate error!\nExit process...\n");
            exit(EXIT_FAILURE);
        }

        /* open *.tar.bz2 and read to package_buf */
        if((package_fd = open(package_path, O_RDONLY)) < 0)
        {
            printf("Open package.tar.bz2 error!\nExit process...\n");
            exit(EXIT_FAILURE);
        }

        if (read(package_fd, package_buf, package_size) == -1)
        {
            printf("Read package.tar.bz2 error!\nExit process...\n");
            exit(EXIT_FAILURE);
        }

        /* encode package_buf to base64_package_buf with base64 */
        if(base64_encode(package_buf, base64_package_buf,
                         package_size) < 0)
        {
            printf("Base64 encode error!\nExit process...\n");
            exit(EXIT_FAILURE);
        }

        if (package_buf != NULL)
        {
            free(package_buf);
            package_buf = NULL;
        }
        else
        {
            printf("Package_buf is NULL!\nExit process...\n");
            exit(EXIT_FAILURE);
        }

        /* send mail */
        while (mail(MAIL_SERVER, base64_package_buf, latest_file) == -1)
        {
            printf("Try send mail again......\n");
            sleep(100);
            if (try_count++ == 100)
            {
                try_count = 0;
                break;
            }
        }
        if (base64_package_buf != NULL)
        {
            free(base64_package_buf);
            base64_package_buf = NULL;
        }
        else
        {
            printf("Base64_package_buf is NULL\nExit process...\n");
            exit(EXIT_FAILURE);
        }
        pthread_exit(NULL);
    }
    else
    {
        pthread_exit(NULL);
    }
    return NULL;
}

/*
 *get android message contain waining, error, fatal and silent
 */
static void *abc_android(void *arg)
{
    printf("abc abc_android\n");
    char logcat_cmd[128] = {0};

    //sprintf(logcat_cmd, "logcat -v time *:%s > %s/android", LOGCAT_PRIOR, new_log_path);
    sprintf(logcat_cmd, "/system/bin/logcat -b all -f %s/android -n 1 -r 1048576", new_log_path); //limit log size 200M
    while (1)
    {
        if (system(logcat_cmd) < 0)
        {
            printf("Logcat command error!\nExit process...\n");
            exit(EXIT_FAILURE);
        }
        sleep(1);
    }
    return NULL;
}

/*
 * get current process message
 */
static void *abc_process(void *arg)
{
    printf("abc abc_process\n");

    char process_cmd[64] = {0};
    sprintf(process_cmd, "ps > %s/process",
            new_log_path);
    while (1)
    {
        if (system(process_cmd) < 0)
        {
            printf("ps command error!\nExit process...\n");
            exit(EXIT_FAILURE);
        }
        sleep(1);
    }
    return NULL;
}

/*
 * get kernel realtime message
 */
static void *abc_kernel(void *arg)
{
    printf("abc abc_kernel\n");
    char logcat_cmd[128] = {0};

    //sprintf(logcat_cmd, "logcat -v time *:%s > %s/android", LOGCAT_PRIOR, new_log_path);
    sprintf(logcat_cmd, "/system/bin/logcat -b kernel -f %s/kernel -n 1 -r 1048576", new_log_path);
    while (1)
    {
        if (system(logcat_cmd) < 0)
        {
            printf("Logcat command error!\nExit process...\n");
            exit(EXIT_FAILURE);
        }
        sleep(1);
    }
    return NULL;
}

/*
 * get kernel realtime message
 */
/*
static void *abc_kernel(void *arg)
{
   printf("abc abc_kernel\n");
   int num_read = 0;
   char buffer[1024 * 10] = {0};
   struct stat statbuff;
   static int kmsg_fd = 0;

   if((kmsg_fd = open(KMSG_PATH, O_RDONLY)) < 0)
   {
       printf("Open /proc/kmsg error!\nExit process...\n");
       exit(EXIT_FAILURE);
   }

   while (1)
   {
       if (fstat(kernel_fd, &statbuff) == 0)
       {
           if (statbuff.st_size >= 1024*1024*60)
           {
               ftruncate(kernel_fd, 1024*1024*2);
           }
       }

       num_read = read(kmsg_fd, buffer, sizeof(buffer));
//      printf("abc going to write kernel log =%d", num_read);
       write(kernel_fd, buffer, num_read);
//      sleep(1);
   }
   return NULL;
}
*/
static void *abc_monitor_uevent(void *arg)
{
    printf("abc abc_monitor_uevent\n");
    MonitorNetlinkUevent();
    return 0;
}

static void *abc_copy_log_to_flash(void *arg)
{
    sleep(8);
    char path_flash[60] = LOG_FLASH_PATH;
    printf("abc abc_copy_log_to_flash\n");
    copy_all_logs_to_storage(path_flash);
    return 0;
}

int main(int argc, char *argv[])
{
    printf("abc main\n");
    pthread_t abc_kernel_id, abc_process_id,
              abc_android_id, abc_upload_id,
              abc_monitor_uevent_id,abc_copy_log_to_flash_id;
    init_all();

#ifdef CONFIG_KERNEL_LOG
    pthread_create(&abc_kernel_id, NULL, abc_kernel, NULL);
#endif

#ifdef CONFIG_LOGCAT_LOG
    pthread_create(&abc_android_id, NULL, abc_android, NULL);
#endif

#ifdef CONFIG_PROCESS_LOG
    pthread_create(&abc_process_id, NULL, abc_process, NULL);
#endif

#ifdef CONFIG_SEND_MAIL
    pthread_create(&abc_upload_id, NULL, abc_upload, NULL);
#endif

#ifdef CONFIG_MONITOR_UEVENT
    pthread_create(&abc_monitor_uevent_id, NULL, abc_monitor_uevent, NULL);
#endif

#ifdef CONFIG_COPY_LOG_TO_FLASH
    pthread_create(&abc_copy_log_to_flash_id, NULL, abc_copy_log_to_flash, NULL);
#endif

#ifdef CONFIG_KERNEL_LOG
    pthread_join(abc_kernel_id, NULL);
#endif

#ifdef CONFIG_PROCESS_LOG
    pthread_join(abc_process_id, NULL);
#endif

#ifdef CONFIG_LOGCAT_LOG
    pthread_join(abc_android_id, NULL);
#endif

#ifdef CONFIG_SEND_MAIL
    pthread_join(abc_upload_id,NULL);
#endif


#ifdef CONFIG_MONITOR_UEVENT
    pthread_join(abc_monitor_uevent_id,NULL);
#endif

#ifdef CONFIG_COPY_LOG_TO_FLASH
    pthread_join(abc_copy_log_to_flash_id,NULL);
#endif

    return 0;
}
