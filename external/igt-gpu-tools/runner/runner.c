#include <stdio.h>
#include <string.h>

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

	init_settings(&settings);
	init_job_list(&job_list);

	if (!parse_options(argc, argv, &settings)) {
		return 1;
	}

	if (!create_job_list(&job_list, &settings)) {
		return 1;
	}

	if (settings.list_all) {
		list_all_tests(&job_list);
		return 0;
	}

	if (!initialize_execute_state(&state, &settings, &job_list)) {
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
