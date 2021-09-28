#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "settings.h"
#include "job_list.h"
#include "executor.h"
#include "resultgen.h"

int main(int argc, char **argv)
{
	struct settings settings;
	struct job_list job_list;
	struct execute_state state;
	int exitcode = 0;
	int dirfd;

	init_settings(&settings);
	init_job_list(&job_list);

	if (argc < 2) {
		fprintf(stderr, "Usage: %s results-directory\n", argv[0]);
		return 1;
	}

	if ((dirfd = open(argv[1], O_RDONLY | O_DIRECTORY)) < 0) {
		fprintf(stderr, "Failure opening %s: %s\n", argv[1], strerror(errno));
		return 1;
	}

	if (!initialize_execute_state_from_resume(dirfd, &state, &settings, &job_list)) {
		return 1;
	}

	if (!execute(&state, &settings, &job_list)) {
		exitcode = 1;
	}

	if (state.time_left == 0.0) {
		/*
		 * Overall timeout happened. Results generation can
		 * override this
		 */
		exitcode = 2;
	}

	if (!generate_results_path(settings.results_path)) {
		exitcode = 1;
	}

	printf("Done.\n");
	return exitcode;
}
