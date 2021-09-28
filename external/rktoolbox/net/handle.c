#include <stdio.h>
#include <string.h>
#include "handle.h"

void enter_handle(char *value)
{
    char *index = strchr(value, '\n');
    if(index)
    {
        *index = 0;
    }
}