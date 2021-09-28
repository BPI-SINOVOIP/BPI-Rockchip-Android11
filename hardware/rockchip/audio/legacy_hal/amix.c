/*
** Copyright 2010, The Android Open-Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "alsa_audio.h"


struct mixer_ctl *get_ctl(struct mixer *mixer, char *name)
{
    char *p;
    unsigned idx = 0;

    if (isdigit(name[0]))
        return mixer_get_nth_control(mixer, atoi(name) - 1);

    p = strrchr(name, '#');
    if (p) {
        *p++ = 0;
        idx = atoi(p);
    }

    return mixer_get_control(mixer, name, idx);
}

int main(int argc, char **argv)
{
    struct mixer *mixer;
    struct mixer_ctl *ctl;
    int card = 0;
    int r, i, c;

    for (i = 0; i < argc; i++) {
        if ((strncmp(argv[i], "-c", sizeof(argv[i])) == 0) ||
            (strncmp(argv[i], "-card", sizeof(argv[i])) == 0)) {

            i++;
            if (i >= argc) {
                argc -= 1;
                argv += 1;
                break;
            }

            card = atoi(argv[i]);
            argc -= 2;
            argv += 2;
            break;
        }
    }

    printf("Card:%i\n", card);

    mixer = mixer_open(card);

    if (!mixer)
        return -1;

    if (argc == 1) {
        mixer_dump(mixer);
        return 0;
    }

    ctl = get_ctl(mixer, argv[1]);
    argc -= 2;
    argv += 2;

    if (!ctl) {
        printf("can't find control\n");
        return -1;
    }

    if (argc) {
        if (isdigit(argv[0][0]))
            r = mixer_ctl_set_int(ctl, atoi(argv[0]));
        else
            r = mixer_ctl_select(ctl, argv[0]);
        if (r)
            printf("oops: %s\n", strerror(errno));
    }

    mixer_ctl_print(ctl);

    mixer_close(mixer);

    return 0;
}
