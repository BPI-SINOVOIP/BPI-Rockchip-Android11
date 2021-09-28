#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "job_list.h"
#include "igt_core.h"

static bool matches_any(const char *str, struct regex_list *list)
{
	size_t i;

	for (i = 0; i < list->size; i++) {
		if (g_regex_match(list->regexes[i], str, 0, NULL))
			return true;
	}

	return false;
}

static void add_job_list_entry(struct job_list *job_list,
			       char *binary,
			       char **subtests,
			       size_t subtest_count)
{
	struct job_list_entry *entry;

	job_list->size++;
	job_list->entries = realloc(job_list->entries, job_list->size * sizeof(*job_list->entries));
	entry = &job_list->entries[job_list->size - 1];

	entry->binary = binary;
	entry->subtests = subtests;
	entry->subtest_count = subtest_count;
}

static void add_subtests(struct job_list *job_list, struct settings *settings,
			 char *binary,
			 struct regex_list *include, struct regex_list *exclude)
{
	FILE *p;
	char cmd[256] = {};
	char *subtestname;
	char **subtests = NULL;
	size_t num_subtests = 0;
	int s;

	s = snprintf(cmd, sizeof(cmd), "%s/%s --list-subtests",
		     settings->test_root, binary);
	if (s < 0) {
		fprintf(stderr, "Failure generating command string, this shouldn't happen.\n");
		return;
	}

	if (s >= sizeof(cmd)) {
		fprintf(stderr, "Path to binary too long, ignoring: %s/%s\n",
			settings->test_root, binary);
		return;
	}

	p = popen(cmd, "r");
	if (!p) {
		fprintf(stderr, "popen failed when executing %s: %s\n",
			cmd,
			strerror(errno));
		return;
	}

	while (fscanf(p, "%ms", &subtestname) == 1) {
		char piglitname[256];

		generate_piglit_name(binary, subtestname, piglitname, sizeof(piglitname));

		if (exclude && exclude->size && matches_any(piglitname, exclude)) {
			free(subtestname);
			continue;
		}

		if (include && include->size && !matches_any(piglitname, include)) {
			free(subtestname);
			continue;
		}

		if (settings->multiple_mode) {
			num_subtests++;
			subtests = realloc(subtests, num_subtests * sizeof(*subtests));
			subtests[num_subtests - 1] = strdup(subtestname);
		} else {
			subtests = malloc(sizeof(*subtests));
			*subtests = strdup(subtestname);
			add_job_list_entry(job_list, strdup(binary), subtests, 1);
			subtests = NULL;
		}

		free(subtestname);
	}

	if (num_subtests)
		add_job_list_entry(job_list, strdup(binary), subtests, num_subtests);

	s = pclose(p);
	if (s == 0) {
		return;
	} else if (s == -1) {
		fprintf(stderr, "popen error when executing %s: %s\n", binary, strerror(errno));
	} else if (WIFEXITED(s)) {
		if (WEXITSTATUS(s) == IGT_EXIT_INVALID) {
			char piglitname[256];

			generate_piglit_name(binary, NULL,
					     piglitname, sizeof(piglitname));
			/* No subtests on this one */
			if (exclude && exclude->size &&
			    matches_any(piglitname, exclude)) {
				return;
			}
			if (!include || !include->size ||
			    matches_any(piglitname, include)) {
				add_job_list_entry(job_list, strdup(binary), NULL, 0);
				return;
			}
		}
	} else {
		fprintf(stderr, "Test binary %s died unexpectedly\n", binary);
	}
}

static bool filtered_job_list(struct job_list *job_list,
			      struct settings *settings,
			      int fd)
{
	FILE *f;
	char buf[128];
	bool ok;

	if (job_list->entries != NULL) {
		fprintf(stderr, "Caller didn't clear the job list, this shouldn't happen\n");
		exit(1);
	}

	f = fdopen(fd, "r");

	while (fscanf(f, "%127s", buf) == 1) {
		if (!strcmp(buf, "TESTLIST") || !(strcmp(buf, "END")))
			continue;

		/*
		 * If the binary name matches exclude filters, no
		 * subtests are added.
		 */
		if (settings->exclude_regexes.size && matches_any(buf, &settings->exclude_regexes))
			continue;

		/*
		 * If the binary name matches include filters (or include filters not present),
		 * all subtests except those matching exclude filters are added.
		 */
		if (!settings->include_regexes.size || matches_any(buf, &settings->include_regexes)) {
			if (settings->multiple_mode && !settings->exclude_regexes.size)
				/*
				 * Optimization; we know that all
				 * subtests will be included, so we
				 * get to omit executing
				 * --list-subtests.
				 */
				add_job_list_entry(job_list, strdup(buf), NULL, 0);
			else
				add_subtests(job_list, settings, buf,
					     NULL, &settings->exclude_regexes);
			continue;
		}

		/*
		 * Binary name doesn't match exclude or include filters.
		 */
		add_subtests(job_list, settings, buf,
			     &settings->include_regexes,
			     &settings->exclude_regexes);
	}

	ok = job_list->size != 0;
	if (!ok)
		fprintf(stderr, "Filter didn't match any job name\n");

	return ok;
}

static bool job_list_from_test_list(struct job_list *job_list,
				    struct settings *settings)
{
	FILE *f;
	char *line = NULL;
	size_t line_len = 0;
	struct job_list_entry entry = {};
	bool any = false;

	if ((f = fopen(settings->test_list, "r")) == NULL) {
		fprintf(stderr, "Cannot open test list file %s\n", settings->test_list);
		return false;
	}

	while (1) {
		char *binary;
		char *delim;

		if (getline(&line, &line_len, f) == -1) {
			if (errno == EINTR)
				continue;
			else
				break;
		}

		/* # starts a comment */
		if ((delim = strchr(line, '#')) != NULL)
			*delim = '\0';

		if (settings->exclude_regexes.size && matches_any(line, &settings->exclude_regexes))
			continue;

		if (settings->include_regexes.size && !matches_any(line, &settings->include_regexes))
			continue;

		if (sscanf(line, "igt@%ms", &binary) == 1) {
			if ((delim = strchr(binary, '@')) != NULL)
				*delim++ = '\0';

			if (!settings->multiple_mode) {
				char **subtests = NULL;
				if (delim) {
					subtests = malloc(sizeof(char*));
					subtests[0] = strdup(delim);
				}
				add_job_list_entry(job_list, strdup(binary),
						   subtests, (size_t)(subtests != NULL));
				any = true;
				free(binary);
				binary = NULL;
				continue;
			}

			/*
			 * If the currently built entry has the same
			 * binary, add a subtest. Otherwise submit
			 * what's already built and start a new one.
			 */
			if (entry.binary && !strcmp(entry.binary, binary)) {
				if (!delim) {
					/* ... except we didn't get a subtest */
					fprintf(stderr,
						"Error: Unexpected test without subtests "
						"after same test had subtests\n");
					free(binary);
					fclose(f);
					return false;
				}
				entry.subtest_count++;
				entry.subtests = realloc(entry.subtests,
							 entry.subtest_count *
							 sizeof(*entry.subtests));
				entry.subtests[entry.subtest_count - 1] = strdup(delim);
				free(binary);
				binary = NULL;
				continue;
			}

			if (entry.binary) {
				add_job_list_entry(job_list, entry.binary, entry.subtests, entry.subtest_count);
				any = true;
			}

			memset(&entry, 0, sizeof(entry));
			entry.binary = strdup(binary);
			if (delim) {
				entry.subtests = malloc(sizeof(*entry.subtests));
				entry.subtests[0] = strdup(delim);
				entry.subtest_count = 1;
			}

			free(binary);
			binary = NULL;
		}
	}

	if (entry.binary) {
		add_job_list_entry(job_list, entry.binary, entry.subtests, entry.subtest_count);
		any = true;
	}

	free(line);
	fclose(f);
	return any;
}

void list_all_tests(struct job_list *lst)
{
	char piglit_name[256];

	for (size_t test_idx = 0; test_idx < lst->size; ++test_idx) {
		struct job_list_entry *current_entry = lst->entries + test_idx;
		char *binary = current_entry->binary;

		if (current_entry->subtest_count == 0) {
			generate_piglit_name(binary, NULL,
					     piglit_name, sizeof(piglit_name));
			printf("%s\n", piglit_name);
			continue;
		}
		for (size_t subtest_idx = 0;
		    subtest_idx < current_entry->subtest_count;
		    ++subtest_idx) {
			generate_piglit_name(binary, current_entry->subtests[subtest_idx],
					     piglit_name, sizeof(piglit_name));
			printf("%s\n", piglit_name);
		}
	}
}

static char *lowercase(const char *str)
{
	char *ret = malloc(strlen(str) + 1);
	char *q = ret;

	while (*str) {
		if (isspace(*str))
			break;

		*q++ = tolower(*str++);
	}
	*q = '\0';

	return ret;
}

void generate_piglit_name(const char *binary, const char *subtest,
			  char *namebuf, size_t namebuf_size)
{
	char *lc_binary = lowercase(binary);
	char *lc_subtest = NULL;

	if (!subtest) {
		snprintf(namebuf, namebuf_size, "igt@%s", lc_binary);
		free(lc_binary);
		return;
	}

	lc_subtest = lowercase(subtest);

	snprintf(namebuf, namebuf_size, "igt@%s@%s", lc_binary, lc_subtest);

	free(lc_binary);
	free(lc_subtest);
}

void init_job_list(struct job_list *job_list)
{
	memset(job_list, 0, sizeof(*job_list));
}

void free_job_list(struct job_list *job_list)
{
	int i, k;

	for (i = 0; i < job_list->size; i++) {
		struct job_list_entry *entry = &job_list->entries[i];

		free(entry->binary);
		for (k = 0; k < entry->subtest_count; k++) {
			free(entry->subtests[k]);
		}
		free(entry->subtests);
	}
	free(job_list->entries);
	init_job_list(job_list);
}

bool create_job_list(struct job_list *job_list,
		     struct settings *settings)
{
	int dirfd, fd;
	bool result;

	if (!settings->test_root) {
		fprintf(stderr, "No test root set; this shouldn't happen\n");
		return false;
	}

	free_job_list(job_list);

	dirfd = open(settings->test_root, O_DIRECTORY | O_RDONLY);
	if (dirfd < 0) {
		fprintf(stderr, "Test directory %s cannot be opened\n", settings->test_root);
		return false;
	}

	fd = openat(dirfd, "test-list.txt", O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s/test-list.txt\n", settings->test_root);
		close(dirfd);
		return false;
	}

	/*
	 * If a test_list is given (not to be confused with
	 * test-list.txt), we use it directly without making tests
	 * list their subtests. If include/exclude filters are given
	 * we filter them directly from the test_list.
	 */
	if (settings->test_list)
		result = job_list_from_test_list(job_list, settings);
	else
		result = filtered_job_list(job_list, settings, fd);

	close(fd);
	close(dirfd);

	return result;
}

static char joblist_filename[] = "joblist.txt";
bool serialize_job_list(struct job_list *job_list, struct settings *settings)
{
	int dirfd, fd;
	size_t i, k;
	FILE *f;

	if (!settings->results_path) {
		fprintf(stderr, "No results-path set; this shouldn't happen\n");
		return false;
	}

	if ((dirfd = open(settings->results_path, O_DIRECTORY | O_RDONLY)) < 0) {
		mkdir(settings->results_path, 0777);
		if ((dirfd = open(settings->results_path, O_DIRECTORY | O_RDONLY)) < 0) {
			fprintf(stderr, "Creating results-path failed\n");
			return false;
		}
	}

	if (!settings->overwrite &&
	    faccessat(dirfd, joblist_filename, F_OK, 0) == 0) {
		fprintf(stderr, "Job list file already exists and not overwriting\n");
		close(dirfd);
		return false;
	}

	if (settings->overwrite &&
	    unlinkat(dirfd, joblist_filename, 0) != 0 &&
	    errno != ENOENT) {
		fprintf(stderr, "Error removing old job list\n");
		close(dirfd);
		return false;
	}

	if ((fd = openat(dirfd, joblist_filename, O_CREAT | O_EXCL | O_WRONLY, 0666)) < 0) {
		fprintf(stderr, "Creating job list serialization file failed: %s\n", strerror(errno));
		close(dirfd);
		return false;
	}

	f = fdopen(fd, "w");
	if (!f) {
		close(fd);
		close(dirfd);
		return false;
	}

	for (i = 0; i < job_list->size; i++) {
		struct job_list_entry *entry = &job_list->entries[i];
		fputs(entry->binary, f);

		if (entry->subtest_count) {
			const char *delim = "";

			fprintf(f, " ");

			for (k = 0; k < entry->subtest_count; k++) {
				fprintf(f, "%s%s", delim, entry->subtests[k]);
				delim = ",";
			}
		}

		fprintf(f, "\n");
	}

	if (settings->sync) {
		fsync(fd);
		fsync(dirfd);
	}

	fclose(f);
	close(dirfd);
	return true;
}

bool read_job_list(struct job_list *job_list, int dirfd)
{
	int fd;
	FILE *f;
	ssize_t read;
	char *line = NULL;
	size_t line_len = 0;

	free_job_list(job_list);

	if ((fd = openat(dirfd, joblist_filename, O_RDONLY)) < 0)
		return false;

	f = fdopen(fd, "r");
	if (!f) {
		close(fd);
		return false;
	}

	while ((read = getline(&line, &line_len, f))) {
		char *binary, *sublist, *comma;
		char **subtests = NULL;
		size_t num_subtests = 0, len;

		if (read < 0) {
			if (errno == EINTR)
				continue;
			else
				break;
		}

		len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		sublist = strchr(line, ' ');
		if (!sublist) {
			add_job_list_entry(job_list, strdup(line), NULL, 0);
			continue;
		}

		*sublist++ = '\0';
		binary = strdup(line);

		do {
			comma = strchr(sublist, ',');
			if (comma) {
				*comma++ = '\0';
			}

			++num_subtests;
			subtests = realloc(subtests, num_subtests * sizeof(*subtests));
			subtests[num_subtests - 1] = strdup(sublist);
			sublist = comma;
		} while (comma != NULL);

		add_job_list_entry(job_list, binary, subtests, num_subtests);
	}

	free(line);
	fclose(f);

	return true;
}
