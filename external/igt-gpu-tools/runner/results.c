#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "resultgen.h"

int main(int argc, char **argv)
{
	int dirfd;

	if (argc < 2)
		exit(1);

	dirfd = open(argv[1], O_DIRECTORY | O_RDONLY);
	if (dirfd < 0)
		exit(1);

	if (generate_results(dirfd)) {
		printf("Results generated\n");
		exit(0);
	}

	exit(1);
}
