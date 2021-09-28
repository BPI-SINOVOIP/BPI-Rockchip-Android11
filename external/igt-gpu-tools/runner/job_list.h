#ifndef RUNNER_JOB_LIST_H
#define RUNNER_JOB_LIST_H

#include <stdbool.h>

#include "settings.h"

struct job_list_entry {
	char *binary;
	char **subtests;
	/*
	 * 0 = all, or test has no subtests.
	 *
	 * If the original job_list was to run all subtests of a
	 * binary and such a run was incomplete, resuming from the
	 * execution journal will fill the subtest array with already
	 * started subtests prepended with '!' so the test binary will
	 * not run them. subtest_count will still reflect the size of
	 * the above array.
	 */
	size_t subtest_count;
};

struct job_list
{
	struct job_list_entry *entries;
	size_t size;
};

void generate_piglit_name(const char *binary, const char *subtest,
			  char *namebuf, size_t namebuf_size);

void init_job_list(struct job_list *job_list);
void free_job_list(struct job_list *job_list);
bool create_job_list(struct job_list *job_list, struct settings *settings);

bool serialize_job_list(struct job_list *job_list, struct settings *settings);
bool read_job_list(struct job_list *job_list, int dirfd);
void list_all_tests(struct job_list *lst);

#endif
