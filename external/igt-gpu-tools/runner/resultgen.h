#ifndef RUNNER_RESULTGEN_H
#define RUNNER_RESULTGEN_H

#include <stdbool.h>

bool generate_results(int dirfd);
bool generate_results_path(char *resultspath);

struct json_object *generate_results_json(int dirfd);

#endif
