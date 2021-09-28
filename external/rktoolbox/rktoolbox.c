#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SYSTEM_PATH    "/system/bin/rk_"
#define VENDOR_PATH    "/vendor/bin/rk_"

#define MODULE_NAME     "rktoolbox"
#define MODULE_VERSION     "V0.3"

void dump_info(void);

enum{
    E_NO_API_FLAGS = 0x00,
    E_DUMP_API_FLAGS = 0x01,
};

static struct
{
    const char *name;
    void (*func)(void);
    const char *info;
} flags_api[] = {
    {"-dump", dump_info, "Dump debug info"},
    {0, 0, 0},
};

static struct
{
    const char *name;
    const char *path;
    const char *info;
    int flags;
} tools[] = {
#define TOOL(name, flags, info) { #name, SYSTEM_PATH#name, #info, flags},
#include "tools.h"
#undef TOOL
    { 0, 0 , 0, 0},
};

static char exec_buf[1024] = {0};

static void SIGPIPE_handler(int signal) {
    // Those desktop Linux tools that catch SIGPIPE seem to agree that it's
    // a successful way to exit, not a failure. (Which makes sense --- we were
    // told to stop by a reader, rather than failing to continue ourselves.)
    _exit(0);
}

static void usage(void)
{
    printf("Version: %s\r\n",MODULE_VERSION);
    printf("Usage:\r\n");
    printf("       rktoolbox modulename -func\n");
    printf("       rktoolbox -func\n");
    printf("\n");
    printf("Miscellaneous:\n");
    for(int i=0;tools[i].name; i++)
    {
        printf("  %s          %s\n",tools[i].name, tools[i].info);
    }
    printf("  -help          Print help information\n");
    for(int i=0;flags_api[i].name; i++)
    {
        printf("  %s          %s\n",flags_api[i].name, flags_api[i].info);
    }
}

int main(int argc, char **argv)
{
    int i;
    char *name = NULL;

    if(argc < 2)
    {
        printf("%s: Need 2 arguments (see \" %s -help\")\n", MODULE_NAME, MODULE_NAME);
        return 0;
    }
#ifdef LOG_DEBUG
    for(i = 0; i < argc; i++)
    {
        printf(" i = %d value =  %s \r\n", i, argv[i]);
    }
#endif
    // Let's assume that none of this code handles broken pipes. At least ls,
    // ps, and top were broken (though I'd previously added this fix locally
    // to top). We exit rather than use SIG_IGN because tools like top will
    // just keep on writing to nowhere forever if we don't stop them.
    signal(SIGPIPE, SIGPIPE_handler);

    name = argv[1];

    for(i = 0; flags_api[i].name; i++)
    {
        if(!strcmp(flags_api[i].name, argv[1]))
        {
            flags_api[i].func();
            return 0;
        }
    }

    if(!strcmp("-version", name))
    {
        printf("Version: %s\r\n",MODULE_VERSION);
        return 0;
    }
    else
    if(!strcmp("-help", name))
    {
        usage();
        return 0;
    }
    else
    {
        for(i = 0; tools[i].name; i++)
        {
            if(!strcmp(tools[i].name, name))
            {
                sprintf(exec_buf, "%s",tools[i].path);
                for(i = 2; i<argc; i++)
                {
                    if(sizeof(exec_buf) < (strlen(exec_buf)+strlen(argv[i]) + 3))
                    {
                        printf("paramter length max > 1024 , fail ~~~~\r\n");
                        return 0;
                    }
                    strcat(exec_buf," ");
                    strcat(exec_buf, argv[i]);
                }
#if LOG_DEBUG
                printf("paramter = %s ---\r\n", exec_buf);
#endif
                system(exec_buf);
                return 0;
            }
        }
    }

    printf("%s: no such tool (see \" %s -help\")\n", MODULE_NAME, MODULE_NAME);
    return 0;
}


void dump_info(void)
{
    int i;

    for(i = 0; tools[i].name; i++)
    {
        if(tools[i].flags & E_DUMP_API_FLAGS)
        {
            sprintf(exec_buf, "%s -dump",tools[i].path);
            printf("=====================================%s dump info start=========================================\r\n", tools[i].name);
            system(exec_buf);
            printf("=====================================%s dump info end  =========================================\r\n", tools[i].name);
            memset(exec_buf, 0, sizeof(exec_buf));
        }
    }
}
