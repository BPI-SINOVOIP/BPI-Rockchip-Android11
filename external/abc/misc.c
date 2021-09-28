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
#include "abc.h"

#define printf ALOGD
#define FILE_PERMIT (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define DIR_PERMIT  (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP \
                 | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH)

#define FILE_MODE   (O_RDWR | O_SYNC | O_CREAT | O_APPEND)

char latest_file[PATH_MAX];
char latest_log_path[PATH_MAX];
char new_log_path[PATH_MAX];
int trigger_upload;

const char *key_words[] =
{
    "error:",
    "panic:",
    "fatal:",
    " "
};

static const char encode_table[64] =
{
    'A','B','C','D','E','F','G','H',
    'I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X',
    'Y','Z','a','b','c','d','e','f',
    'g','h','i','j','k','l','m','n',
    'o','p','q','r','s','t','u','v',
    'w','x','y','z','0','1','2','3',
    '4','5','6','7','8','9','+','/'
};

/*
 *base64 encode
 */
int base64_encode(const char *s, char *d, const unsigned long input_length)
{
    unsigned char in[3] = {0};
    unsigned char out[4] = {0};
    unsigned long i = 0;

    if (!s)
        return -1;
    for (i = 0; i < input_length; i += 3)
    {
        if (input_length - i  >= 3)
        {
            in[0] = *s;
            in[1] = *(s + 1);
            in[2] = *(s + 2);
            out[0] = in[0] >> 2;
            out[1] = (in[0] & 0x03) << 4| (in[1] & 0xf0) >> 4;
            out[2] = (in[1] & 0x0f) << 2| (in[2] & 0xc0) >> 6;
            out[3] = in[2] & 0x3f;
            *d = encode_table[out[0]];
            *(d + 1) = encode_table[out[1]];
            *(d + 2) = encode_table[out[2]];
            *(d + 3) = encode_table[out[3]];
            s += 3;
            d += 4;
        }
        else if (input_length - i == 1)
        {
            in[0] = *s;
            out[0] = in[0] >> 2;
            out[1] = (in[0] & 0x03) << 4 | 0;
            *d = encode_table[out[0]];
            *(d + 1) = encode_table[out[1]];
            *(d + 2) = '=';
            *(d + 3) = '=';
            s += 1;
            d += 4;
        }
        else if(input_length - i == 2)
        {
            in[0] = *s;
            in[1] = *(s + 1);
            out[0] = in[0] >> 2;
            out[1] = (in[0] & 0x03) << 4| (in[1] & 0xf0) >> 4;
            out[2] = (in[1] & 0x0f) << 2 | 0;
            *d = encode_table[out[0]];
            *(d + 1) = encode_table[out[1]];
            *(d + 2) = encode_table[out[2]];
            *(d + 3) = '=';
            s += 2;
            d += 4;
        }
    }
    *d = 0;
    return 0;
}

/* add specified characrer */
static void insert_character(char* s, char c, int locate)
{
    int i, j;
    int slen = strlen(s);

    for (i = slen; i >= locate; i--)
        s[i + 1] = s[i];
    s[locate] = c;
}

static void string_add_character(char* s)
{
    insert_character(s, '-', 2);
    insert_character(s, '-', 5);
    insert_character(s, '-', 8);
    insert_character(s, '-', 11);
    insert_character(s, '-', 14);
}

/*
 *recursive delete directory
 */
int
delete_dir(const char *path)
{
    DIR *dp;
    struct dirent *entry;
    struct stat stat_buf;

    if ((dp = opendir(path)) == NULL)
    {
        printf("delete_dir Open %s error!\nExit process...\n", path);
        //exit(EXIT_FAILURE);
        return 1;
    }

    chdir(path);

    while ((entry = readdir(dp)) != NULL)
    {
        lstat(entry->d_name, &stat_buf);
        if (S_ISDIR(stat_buf.st_mode))
        {
            if(strcmp(".", entry->d_name) == (int)0 ||
               strcmp("..", entry->d_name) == (int)0)
                continue;
            else
                delete_dir(entry->d_name);
        }
        unlink(entry->d_name);
    }

    chdir("..");
    closedir(dp);
    rmdir(path);
    return 0;
}

static void update_log_dir(long long *array, const int cnt)
{
    int i, j;
    long long temp = 0;
    char buf[PATH_MAX] = {0};

    struct tm *ptime;
    time_t the_time;

    /* get current system time */
    time(&the_time);
    ptime = localtime(&the_time);

    /* change current time to  long long decimal*/
    temp = ptime->tm_year % 100LL * 10000000000LL +
           (ptime->tm_mon + 1) * 100000000LL +
           ptime->tm_mday * 1000000LL +
           ptime->tm_hour * 10000LL +
           ptime->tm_min * 100LL + ptime->tm_sec;
    /* delete illegal logs */
    for (i = 0; i < cnt; i++)
    {
        if(array[i] > temp)
        {
            sprintf(buf, "%lld", array[i]);
            string_add_character(buf);
            array[i] = 0;
            delete_dir(buf);
        }
    }
    /* bubble sort logs */
    i = cnt;
    while (i > 0)
    {
        for (j = 0; j < cnt -1; j++)
        {
            if (array[j] < array[j + 1])
            {
                temp = array[j];
                array[j] = array[j + 1];
                array[j + 1] = temp;
            }
        }
        i--;
    }
    /* delete old logs */
    for (i = SYS_LOG_MAX - 1; i < cnt; i++)
    {
        sprintf(buf, "%lld", array[i]);
        string_add_character(buf);
        printf("delete dir when dirs out of SYS_LOG_MAX\n");
        delete_dir(buf);
    }
}

/*delete specified character*/
static void delete_character(char* string, char a)
{
    int i, j;
    for (i = 0, j = 0; string[i] != '\0'; i++)
    {
        if (string[i] != a)
        {
            string[j++] = string[i];
        }
    }
    string[j] = '\0';
}


/*
 *traverse system directory delete illegal sub-directorys and sub-files
 *then sort log diectorys and delete old logs
 */
static void clean_directory(void)
{
    DIR *dp;
    struct dirent *entry;
    struct stat stat_buf;

    long long dir_array[ITERM_MAX] = {0};
    int dir_cnt = 0;
    char* temp;

    if ((dp = opendir(SYSTEM_PATH)) == NULL)
    {
        printf("Open %s error!\nExit process...\n", SYSTEM_PATH);
        exit(EXIT_FAILURE);
    }
    chdir(SYSTEM_PATH);
    /* traverse directory */
    while ((entry = readdir(dp)) != NULL)
    {
        /* get entry attribute */
        lstat(entry->d_name, &stat_buf);
        if (S_ISDIR(stat_buf.st_mode))
        {
            if(strcmp(".", entry->d_name) == (int)0 ||
               strcmp("..", entry->d_name) == (int)0)
                continue;
            /* get entry name to dir_array */
            else
            {
                delete_character(entry->d_name, '-');
                if ((dir_array[dir_cnt] = atoll(entry->d_name)) <
                    100000000000LL)
                {
                    /* delete illegal sub-directorys */
                    printf("%s\n", entry->d_name);
                    string_add_character(entry->d_name);
                    printf("%s\n", entry->d_name);
                    delete_dir(entry->d_name);
                    continue;
                }
                else
                {
                    dir_cnt++;
                }
            }
        }
        /* delete all sub-files */
        unlink(entry->d_name);
    }
    /* sort log-directory */
    update_log_dir(dir_array, dir_cnt);
    /* back system log directory */
    chdir(SYSTEM_PATH);
    closedir(dp);
}

static void create_new_log(void)
{
    struct tm *ptime;
    time_t the_time;

    /* get current time */
    time(&the_time);
    ptime = localtime(&the_time);
    chdir(SYSTEM_PATH);

    sprintf(new_log_path, "%s/%02d-%02d-%02d-%02d-%02d-%02d",SYSTEM_PATH,
            ptime->tm_year % 100, ptime->tm_mon + 1,
            ptime->tm_mday, ptime->tm_hour,
            ptime->tm_min, ptime->tm_sec);

    /* create new log directory with current time */
    mkdir(new_log_path, DIR_PERMIT);
    chdir(new_log_path);
    if ((kernel_fd = open("kernel", O_RDWR | O_SYNC | O_CREAT | O_APPEND,
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
    {
        printf("Create %s error!\nExit process...\n", "kernel");
        exit(EXIT_FAILURE);
    }
    if ((process_fd = open("process", O_RDWR | O_SYNC | O_CREAT | O_APPEND,
                           S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
    {
        printf("Create %s error!\nExit process...\n", "process");
        exit(EXIT_FAILURE);
    }
    if ((android_fd = open("android", O_RDWR | O_SYNC | O_CREAT | O_APPEND,
                           S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
    {
        printf("Create %s error!\nExit process...\n", "android");
        exit(EXIT_FAILURE);
    }
    chdir(SYSTEM_PATH);
}

/*
 *create directory to store system logs
 */
static void create_system_directory(void)
{
    umask(0);
    mkdir(SYSTEM_PATH, DIR_PERMIT);
}

/*
 *create directory to store some logs to sdcard
 */
void create_log_directory(const char* sd_path)
{
    umask(0);
    mkdir(sd_path, DIR_PERMIT);
}

void copy_all_logs_to_storage(const char* path)
{
    char shell_cmd[60] = "";
    printf("copy_all_logs_to_storage to %s \n",path);
    if (access(path,0) )
    {
        printf("error: %s not found,return\n,path");
        return;
    }
    else
    {
        strcat(path,"/rk_logs/");//for 3399 7.1 the sdcard mountpoint is similar to "/storage/8527-18E3/rk_logs" , 8527-18E3 is the uuid
        if ( !access(path,0) )
        {
            printf(" %s EXISITS,rebuild it\n",path);
            delete_dir(path);
        }

        create_log_directory(path);

        strcpy(shell_cmd,"cp -rf /data/vendor/logs ");
        strcat(shell_cmd,path);
        printf("shell_cmd now %s\n", shell_cmd);
        system(shell_cmd);

        strcpy(shell_cmd,"cp -rf /sys/fs/pstore ");
        strcat(shell_cmd,path);
        printf("shell_cmd now %s\n", shell_cmd);
        system(shell_cmd);

        strcpy(shell_cmd,"cp -rf /data/anr ");
        strcat(shell_cmd,path);
        printf("shell_cmd now %s\n", shell_cmd);
        system(shell_cmd);

        strcpy(shell_cmd,"cp -rf /data/tombstones ");
        strcat(shell_cmd,path);
        printf("shell_cmd now %s\n", shell_cmd);
        system(shell_cmd);

        strcpy(shell_cmd,"cp -rf /cache/recovery ");
        strcat(shell_cmd,path);
        printf("shell_cmd now %s\n", shell_cmd);
        system(shell_cmd);

        strcpy(shell_cmd,"bugreport > ");
        strcat(shell_cmd,path);
        strcat(shell_cmd,"bugreport.log");
        printf("shell_cmd now is %s\n", shell_cmd);
        system(shell_cmd);

        strcpy(shell_cmd,"touch ");
        strcat(shell_cmd,path);
        strcat(shell_cmd,"COPY-COMPLETE");
        printf("shell_cmd now is %s\n", shell_cmd);
        system(shell_cmd);
        printf("COPY-COMPLETE\n");

        strcpy(shell_cmd,"chmod -R 777 ");
        strcat(shell_cmd,path);
        printf("shell_cmd now is %s\n", shell_cmd);
        system(shell_cmd);

    }
}

/*
 *filter keywords from current latest log directory
 */
static void filter_bug_info(void)
{
    DIR *dp;
    struct dirent *entry;
    struct stat stat_buf;
    char grep_command[64] = {0};
    int i = 0, j;
    long long array[SYS_LOG_MAX - 1] = {0};
    int debug = 0;
    long long latest = 0;

    if ((dp = opendir(SYSTEM_PATH)) == NULL)
    {
        printf("Open %s error!\nExit process...\n", SYSTEM_PATH);
        exit(EXIT_FAILURE);
    }

    chdir(SYSTEM_PATH);
    /* traverse directory  */
    while ((entry = readdir(dp)) != NULL)
    {
        lstat(entry->d_name, &stat_buf);
        if (S_ISDIR(stat_buf.st_mode))
        {
            if(strcmp(".", entry->d_name) == (int)0 ||
               strcmp("..", entry->d_name) == (int)0)
                continue;
            delete_character(entry->d_name, '-');
            array[i++] = atoll(entry->d_name);
        }
    }
    /* system directory is empty */
    if (array[0] == 0)
        return;
    latest = array[0];
    /* get the latest log directory */
    for (j = 1; j < i; j++)
    {
        if (latest < array[j])
            latest = array[j];
    }
    sprintf(latest_file, "%lld", latest);

    string_add_character(latest_file);
    sprintf(latest_log_path, "%s/%s", SYSTEM_PATH,
            latest_file);
    chdir(latest_log_path);
    i = 0;
    /* filter keywords from latest log */
    while (strcmp(key_words[i++], " "))
    {
        sprintf(grep_command, "/system/bin/busybox grep -n \"%s\" kernel android",
                key_words[i]);
        /* call busybox grep command */
        debug = system(grep_command);
        /* send trigger singal to uploader */
        if (debug < 0)
        {
            printf("Grep command error!\nExit process...\n");
            exit(EXIT_FAILURE);
        }
        if(!debug)
        {
            trigger_upload = 1;
            chdir(SYSTEM_PATH);
            return;
        }
    }
    chdir(SYSTEM_PATH);
}

void init_all(void)
{

    printf("*******************************"
           "Android Bug Collector Start"
           "*******************************\n");

    create_system_directory();
    clean_directory();
    filter_bug_info();
    create_new_log();
}
