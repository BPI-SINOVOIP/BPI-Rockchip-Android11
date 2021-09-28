#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MODULE_NAME     "dr-g"
char exec_buf[1024] = {0};

static void dump_info(char *path, char *cmd)
{
    sprintf(exec_buf, "%s %s",path,cmd);

    system(exec_buf);
}

int main(int argc, char **argv)
{

   int i = 0;

    if(argc < 2)
    {
        printf("%s: Need 2 arguments (see \" %s -help\")\n", MODULE_NAME, MODULE_NAME);
        return 0;
    }

    if(!strcmp(argv[1], "-dump"))
    {
       dump_info(argv[0], "-dump-info");
       return 0;
    }
    else
    {
        sprintf(exec_buf, "%s", MODULE_NAME);
        for(i = 1; i<argc; i++)
        {
            if(sizeof(exec_buf) < (strlen(exec_buf)+strlen(argv[i]) + 3))
            {
                printf("paramter length max > 1024 , fail ~~~~\r\n");
                return 0;
            }
            strcat(exec_buf," ");
            strcat(exec_buf, argv[i]);
        }

        system(exec_buf);

        return 0;
    }

   printf("%s: no such. (see \" %s -help\")\n", MODULE_NAME, MODULE_NAME);

   return 0;
}
