#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	double this_time, last_time;
	char line[256], last_line[256], *p;
	int major, minor, cpu, nr, alias;
	long  MAX_CPUS;
	unsigned long long total_entries;
	unsigned int *last_seq;
	unsigned int seq;
	FILE *f;

#ifdef _SC_NPROCESSORS_ONLN
	MAX_CPUS = sysconf(_SC_NPROCESSORS_ONLN);
	if (MAX_CPUS < 1)
	{
		fprintf(stderr, "Could not determine number of CPUs online:\n%s\n", 
			strerror (errno));
		fprintf(stderr, "Assuming 1024\n");
		MAX_CPUS = 1024;
  	}
#else
	MAX_CPUS = CPU_SETSIZE;
#endif 
	
	last_seq = malloc( sizeof(unsigned int) * MAX_CPUS );
	for (nr = 0; nr < MAX_CPUS; nr++)
		last_seq[nr] = -1;

	if (argc < 2) {
		fprintf(stderr, "%s: file\n", argv[0]);
		return 1;
	}

	f = fopen(argv[1], "r");
	if (!f) {
		perror("fopen");
		return 1;
	}

	last_time = 0;
	alias = nr = 0;
	total_entries = 0;
	while ((p = fgets(line, sizeof(line), f)) != NULL) {
		if (sscanf(p, "%3d,%3d %5d %8d %lf", &major, &minor, &cpu, &seq, &this_time) != 5)
			break;

		if (this_time < last_time) {
			fprintf(stdout, "last: %s", last_line);
			fprintf(stdout, "this: %s", p);
			nr++;
		}

		last_time = this_time;

		if (cpu >= MAX_CPUS) {
			fprintf(stderr, "cpu%d too large\n", cpu);
			break;
		}

		if (last_seq[cpu] == seq) {
			fprintf(stdout, "alias on sequence %u\n", seq);
			alias++;
		}

		last_seq[cpu] = seq;
		total_entries++;
		strcpy(last_line, line);
	}

	fprintf(stdout, "Events %Lu: %d unordered, %d aliases\n", total_entries, nr, alias);
	fclose(f);

	return nr != 0;
}
