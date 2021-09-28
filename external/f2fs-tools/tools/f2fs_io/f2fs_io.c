/*
 * f2fs_io.c - f2fs ioctl utility
 *
 * Author: Jaegeuk Kim <jaegeuk@kernel.org>
 *
 * Copied portion of the code from ../f2fscrypt.c
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "f2fs_io.h"

struct cmd_desc {
	const char *cmd_name;
	void (*cmd_func)(int, char **, const struct cmd_desc *);
	const char *cmd_desc;
	const char *cmd_help;
	int cmd_flags;
};

static void __attribute__((noreturn))
do_die(const char *format, va_list va, int err)
{
	vfprintf(stderr, format, va);
	if (err)
		fprintf(stderr, ": %s", strerror(err));
	putc('\n', stderr);
	exit(1);
}

static void __attribute__((noreturn, format(printf, 1, 2)))
die_errno(const char *format, ...)
{
	va_list va;

	va_start(va, format);
	do_die(format, va, errno);
	va_end(va);
}

static void __attribute__((noreturn, format(printf, 1, 2)))
die(const char *format, ...)
{
	va_list va;

	va_start(va, format);
	do_die(format, va, 0);
	va_end(va);
}

static void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if (!p)
		die("Memory alloc failed (requested %zu bytes)", size);
	return p;
}

static void *aligned_xalloc(size_t alignment, size_t size)
{
	void *p = aligned_alloc(alignment, size);

	if (!p)
		die("Memory alloc failed (requested %zu bytes)", size);
	return p;
}

static int xopen(const char *pathname, int flags, mode_t mode)
{
	int fd = open(pathname, flags, mode);

	if (fd < 0)
		die_errno("Failed to open %s", pathname);
	return fd;
}

static ssize_t xread(int fd, void *buf, size_t count)
{
	ssize_t ret = read(fd, buf, count);

	if (ret < 0)
		die_errno("read failed");
	return ret;
}

static void full_write(int fd, const void *buf, size_t count)
{
	while (count) {
		ssize_t ret = write(fd, buf, count);

		if (ret < 0)
			die_errno("write failed");
		buf = (char *)buf + ret;
		count -= ret;
	}
}

#define fsync_desc "fsync"
#define fsync_help						\
"f2fs_io fsync [file]\n\n"					\
"fsync given the file\n"					\

static void do_fsync(int argc, char **argv, const struct cmd_desc *cmd)
{
	int fd;

	if (argc != 2) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	fd = xopen(argv[1], O_WRONLY, 0);

	if (fsync(fd) != 0)
		die_errno("fsync failed");

	printf("fsync a file\n");
	exit(0);
}

#define set_verity_desc "Set fs-verity"
#define set_verity_help					\
"f2fs_io set_verity [file]\n\n"				\
"Set fsverity bit given a file\n"			\

static void do_set_verity(int argc, char **argv, const struct cmd_desc *cmd)
{
	int ret, fd;

	if (argc != 2) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	fd = open(argv[1], O_RDWR);

	ret = ioctl(fd, FS_IOC_ENABLE_VERITY);
	if (ret < 0) {
		perror("FS_IOC_ENABLE_VERITY");
		exit(1);
	}

	printf("Set fsverity bit to %s\n", argv[1]);
	exit(0);
}

#define getflags_desc "getflags ioctl"
#define getflags_help						\
"f2fs_io getflags [file]\n\n"					\
"get a flag given the file\n"					\
"flag can show \n"						\
"  encryption\n"						\
"  nocow(pinned)\n"						\
"  inline_data\n"						\
"  verity\n"							\
"  casefold\n"							\
"  compression\n"						\
"  nocompression\n"

static void do_getflags(int argc, char **argv, const struct cmd_desc *cmd)
{
	long flag = 0;
	int ret, fd;
	int exist = 0;

	if (argc != 2) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	fd = xopen(argv[1], O_RDONLY, 0);

	ret = ioctl(fd, F2FS_IOC_GETFLAGS, &flag);
	printf("get a flag on %s ret=%d, flags=", argv[1], ret);
	if (flag & FS_CASEFOLD_FL) {
		printf("casefold");
		exist = 1;
	}
	if (flag & FS_COMPR_FL) {
		if (exist)
			printf(",");
		printf("compression");
		exist = 1;
	}
	if (flag & FS_NOCOMP_FL) {
		if (exist)
			printf(",");
		printf("nocompression");
		exist = 1;
	}
	if (flag & FS_ENCRYPT_FL) {
		if (exist)
			printf(",");
		printf("encrypt");
		exist = 1;
	}
	if (flag & FS_VERITY_FL) {
		if (exist)
			printf(",");
		printf("verity");
		exist = 1;
	}
	if (flag & FS_INLINE_DATA_FL) {
		if (exist)
			printf(",");
		printf("inline_data");
		exist = 1;
	}
	if (flag & FS_NOCOW_FL) {
		if (exist)
			printf(",");
		printf("nocow(pinned)");
		exist = 1;
	}
	if (!exist)
		printf("none");
	printf("\n");
	exit(0);
}

#define setflags_desc "setflags ioctl"
#define setflags_help						\
"f2fs_io setflags [flag] [file]\n\n"				\
"set a flag given the file\n"					\
"flag can be\n"							\
"  casefold\n"							\
"  compression\n"						\
"  nocompression\n"

static void do_setflags(int argc, char **argv, const struct cmd_desc *cmd)
{
	long flag = 0;
	int ret, fd;

	if (argc != 3) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	fd = xopen(argv[2], O_RDONLY, 0);

	ret = ioctl(fd, F2FS_IOC_GETFLAGS, &flag);
	printf("get a flag on %s ret=%d, flags=%lx\n", argv[1], ret, flag);
	if (ret)
		die_errno("F2FS_IOC_GETFLAGS failed");

	if (!strcmp(argv[1], "casefold"))
		flag |= FS_CASEFOLD_FL;
	else if (!strcmp(argv[1], "compression"))
		flag |= FS_COMPR_FL;
	else if (!strcmp(argv[1], "nocompression"))
		flag |= FS_NOCOMP_FL;

	ret = ioctl(fd, F2FS_IOC_SETFLAGS, &flag);
	printf("set a flag on %s ret=%d, flags=%s\n", argv[2], ret, argv[1]);
	exit(0);
}

#define shutdown_desc "shutdown filesystem"
#define shutdown_help					\
"f2fs_io shutdown [level] [dir]\n\n"			\
"Freeze and stop all IOs given mount point\n"		\
"level can be\n"					\
"  0 : going down with full sync\n"			\
"  1 : going down with checkpoint only\n"		\
"  2 : going down with no sync\n"			\
"  3 : going down with metadata flush\n"		\
"  4 : going down with fsck mark\n"

static void do_shutdown(int argc, char **argv, const struct cmd_desc *cmd)
{
	u32 flag;
	int ret, fd;

	if (argc != 3) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	flag = atoi(argv[1]);
	if (flag >= F2FS_GOING_DOWN_MAX) {
		fputs("Wrong level\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}
	fd = xopen(argv[2], O_RDONLY, 0);

	ret = ioctl(fd, F2FS_IOC_SHUTDOWN, &flag);
	if (ret < 0)
		die_errno("F2FS_IOC_SHUTDOWN failed");

	printf("Shutdown %s with level=%d\n", argv[2], flag);
	exit(0);
}

#define pinfile_desc "pin file control"
#define pinfile_help						\
"f2fs_io pinfile [get|set] [file]\n\n"			\
"get/set pinning given the file\n"				\

static void do_pinfile(int argc, char **argv, const struct cmd_desc *cmd)
{
	u32 pin;
	int ret, fd;

	if (argc != 3) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	fd = xopen(argv[2], O_RDWR, 0);

	ret = -1;
	if (!strcmp(argv[1], "set")) {
		pin = 1;
		ret = ioctl(fd, F2FS_IOC_SET_PIN_FILE, &pin);
		if (ret != 0)
			die_errno("F2FS_IOC_SET_PIN_FILE failed");
		printf("set_pin_file: %u blocks moved in %s\n", ret, argv[2]);
	} else if (!strcmp(argv[1], "get")) {
		unsigned int flags;

		ret = ioctl(fd, F2FS_IOC_GET_PIN_FILE, &pin);
		if (ret < 0)
			die_errno("F2FS_IOC_GET_PIN_FILE failed");

		ret = ioctl(fd, F2FS_IOC_GETFLAGS, &flags);
		if (ret < 0)
			die_errno("F2FS_IOC_GETFLAGS failed");

		printf("get_pin_file: %s with %u blocks moved in %s\n",
				(flags & F2FS_NOCOW_FL) ? "pinned" : "un-pinned",
				pin, argv[2]);
	}
	exit(0);
}

#define fallocate_desc "fallocate"
#define fallocate_help						\
"f2fs_io fallocate [keep_size] [offset] [length] [file]\n\n"	\
"fallocate given the file\n"					\
" [keep_size] : 1 or 0\n"					\

static void do_fallocate(int argc, char **argv, const struct cmd_desc *cmd)
{
	int fd;
	off_t offset, length;
	struct stat sb;
	int mode = 0;

	if (argc != 5) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	if (!strcmp(argv[1], "1"))
		mode |= FALLOC_FL_KEEP_SIZE;

	offset = atoi(argv[2]);
	length = atoll(argv[3]);

	fd = xopen(argv[4], O_RDWR, 0);

	if (fallocate(fd, mode, offset, length) != 0)
		die_errno("fallocate failed");

	if (fstat(fd, &sb) != 0)
		die_errno("fstat failed");

	printf("fallocated a file: i_size=%"PRIu64", i_blocks=%"PRIu64"\n", sb.st_size, sb.st_blocks);
	exit(0);
}

#define write_desc "write data into file"
#define write_help					\
"f2fs_io write [chunk_size in 4kb] [offset in chunk_size] [count] [pattern] [IO] [file_path]\n\n"	\
"Write given patten data in file_path\n"		\
"pattern can be\n"					\
"  zero     : zeros\n"					\
"  inc_num  : incrementing numbers\n"			\
"  rand     : random numbers\n"				\
"IO can be\n"						\
"  buffered : buffered IO\n"				\
"  dio      : direct IO\n"				\

static void do_write(int argc, char **argv, const struct cmd_desc *cmd)
{
	u64 buf_size = 0, inc_num = 0, ret = 0, written = 0;
	u64 offset;
	char *buf = NULL;
	unsigned bs, count, i;
	int flags = 0;
	int fd;

	srand(time(0));

	if (argc != 7) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	bs = atoi(argv[1]);
	if (bs > 1024)
		die("Too big chunk size - limit: 4MB");

	buf_size = bs * 4096;

	offset = atoi(argv[2]) * buf_size;

	buf = aligned_xalloc(4096, buf_size);
	count = atoi(argv[3]);

	if (!strcmp(argv[4], "zero"))
		memset(buf, 0, buf_size);
	else if (strcmp(argv[4], "inc_num") && strcmp(argv[4], "rand"))
		die("Wrong pattern type");

	if (!strcmp(argv[5], "dio"))
		flags |= O_DIRECT;
	else if (strcmp(argv[5], "buffered"))
		die("Wrong IO type");

	fd = xopen(argv[6], O_CREAT | O_WRONLY | flags, 0755);

	for (i = 0; i < count; i++) {
		if (!strcmp(argv[4], "inc_num"))
			*(int *)buf = inc_num++;
		else if (!strcmp(argv[4], "rand"))
			*(int *)buf = rand();

		/* write data */
		ret = pwrite(fd, buf, buf_size, offset + buf_size * i);
		if (ret != buf_size)
			break;
		written += ret;
	}

	printf("Written %"PRIu64" bytes with pattern=%s\n", written, argv[4]);
	exit(0);
}

#define read_desc "read data from file"
#define read_help					\
"f2fs_io read [chunk_size in 4kb] [offset in chunk_size] [count] [IO] [print_nbytes] [file_path]\n\n"	\
"Read data in file_path and print nbytes\n"		\
"IO can be\n"						\
"  buffered : buffered IO\n"				\
"  dio      : direct IO\n"				\

static void do_read(int argc, char **argv, const struct cmd_desc *cmd)
{
	u64 buf_size = 0, ret = 0, read_cnt = 0;
	u64 offset;
	char *buf = NULL;
	char *print_buf = NULL;
	unsigned bs, count, i, print_bytes;
	int flags = 0;
	int fd;

	if (argc != 7) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	bs = atoi(argv[1]);
	if (bs > 1024)
		die("Too big chunk size - limit: 4MB");
	buf_size = bs * 4096;

	offset = atoi(argv[2]) * buf_size;

	buf = aligned_xalloc(4096, buf_size);

	count = atoi(argv[3]);
	if (!strcmp(argv[4], "dio"))
		flags |= O_DIRECT;
	else if (strcmp(argv[4], "buffered"))
		die("Wrong IO type");

	print_bytes = atoi(argv[5]);
	if (print_bytes > buf_size)
		die("Print_nbytes should be less then chunk_size in kb");

	print_buf = xmalloc(print_bytes);

	fd = xopen(argv[6], O_RDONLY | flags, 0);

	for (i = 0; i < count; i++) {
		ret = pread(fd, buf, buf_size, offset + buf_size * i);
		if (ret != buf_size)
			break;

		read_cnt += ret;
		if (i == 0)
			memcpy(print_buf, buf, print_bytes);
	}
	printf("Read %"PRIu64" bytes and print %u bytes:\n", read_cnt, print_bytes);
	printf("%08"PRIx64" : ", offset);
	for (i = 1; i <= print_bytes; i++) {
		printf("%02x", print_buf[i - 1]);
		if (i % 16 == 0)
			printf("\n%08"PRIx64" : ", offset + 16 * i);
		else if (i % 2 == 0)
			printf(" ");
	}
	printf("\n");
	exit(0);
}

#define randread_desc "random read data from file"
#define randread_help					\
"f2fs_io randread [chunk_size in 4kb] [count] [IO] [file_path]\n\n"	\
"Do random read data in file_path\n"		\
"IO can be\n"						\
"  buffered : buffered IO\n"				\
"  dio      : direct IO\n"				\

static void do_randread(int argc, char **argv, const struct cmd_desc *cmd)
{
	u64 buf_size = 0, ret = 0, read_cnt = 0;
	u64 idx, end_idx, aligned_size;
	char *buf = NULL;
	unsigned bs, count, i;
	int flags = 0;
	int fd;
	time_t t;
	struct stat stbuf;

	if (argc != 5) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	bs = atoi(argv[1]);
	if (bs > 1024)
		die("Too big chunk size - limit: 4MB");
	buf_size = bs * 4096;

	buf = aligned_xalloc(4096, buf_size);

	count = atoi(argv[2]);
	if (!strcmp(argv[3], "dio"))
		flags |= O_DIRECT;
	else if (strcmp(argv[3], "buffered"))
		die("Wrong IO type");

	fd = xopen(argv[4], O_RDONLY | flags, 0);

	if (fstat(fd, &stbuf) != 0)
		die_errno("fstat of source file failed");

	aligned_size = (u64)stbuf.st_size & ~((u64)(4096 - 1));
	if (aligned_size < buf_size)
		die("File is too small to random read");
	end_idx = (u64)(aligned_size - buf_size) / (u64)4096 + 1;

	srand((unsigned) time(&t));

	for (i = 0; i < count; i++) {
		idx = rand() % end_idx;

		ret = pread(fd, buf, buf_size, 4096 * idx);
		if (ret != buf_size)
			break;

		read_cnt += ret;
	}
	printf("Read %"PRIu64" bytes\n", read_cnt);
	exit(0);
}

struct file_ext {
	__u32 f_pos;
	__u32 start_blk;
	__u32 end_blk;
	__u32 blk_count;
};

#ifndef FIBMAP
#define FIBMAP          _IO(0x00, 1)    /* bmap access */
#endif

#define fiemap_desc "get block address in file"
#define fiemap_help					\
"f2fs_io fiemap [offset in 4kb] [count] [file_path]\n\n"\

static void do_fiemap(int argc, char **argv, const struct cmd_desc *cmd)
{
	u64 offset;
	u32 blknum;
	unsigned count, i;
	int fd;

	if (argc != 4) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	offset = atoi(argv[1]);
	count = atoi(argv[2]);

	fd = xopen(argv[3], O_RDONLY | O_LARGEFILE, 0);

	printf("Fiemap: offset = %08"PRIx64" len = %d\n", offset, count);
	for (i = 0; i < count; i++) {
		blknum = offset + i;

		if (ioctl(fd, FIBMAP, &blknum) < 0)
			die_errno("FIBMAP failed");

		printf("%u ", blknum);
	}
	printf("\n");
	exit(0);
}

#define gc_urgent_desc "start/end/run gc_urgent for given time period"
#define gc_urgent_help					\
"f2fs_io gc_urgent $dev [start/end/run] [time in sec]\n\n"\
" - f2fs_io gc_urgent sda21 start\n"		\
" - f2fs_io gc_urgent sda21 end\n"		\
" - f2fs_io gc_urgent sda21 run 10\n"		\

static void do_gc_urgent(int argc, char **argv, const struct cmd_desc *cmd)
{
	char command[255];

	if (argc == 3 && !strcmp(argv[2], "start")) {
		printf("gc_urgent: start on %s\n", argv[1]);
		sprintf(command, "echo %d > %s/%s/gc_urgent", 1, "/sys/fs/f2fs/", argv[1]);
		if (system(command))
			exit(1);
	} else if (argc == 3 && !strcmp(argv[2], "end")) {
		printf("gc_urgent: end on %s\n", argv[1]);
		sprintf(command, "echo %d > %s/%s/gc_urgent", 0, "/sys/fs/f2fs/", argv[1]);
		if (system(command))
			exit(1);
	} else if (argc == 4 && !strcmp(argv[2], "run")) {
		printf("gc_urgent: start on %s for %d secs\n", argv[1], atoi(argv[3]));
		sprintf(command, "echo %d > %s/%s/gc_urgent", 1, "/sys/fs/f2fs/", argv[1]);
		if (system(command))
			exit(1);
		sleep(atoi(argv[3]));
		printf("gc_urgent: end on %s for %d secs\n", argv[1], atoi(argv[3]));
		sprintf(command, "echo %d > %s/%s/gc_urgent", 0, "/sys/fs/f2fs/", argv[1]);
		if (system(command))
			exit(1);
	} else {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}
}

#define defrag_file_desc "do defragment on file"
#define defrag_file_help						\
"f2fs_io defrag_file [start] [length] [file_path]\n\n"		\
"  start     : start offset of defragment region, unit: bytes\n"	\
"  length    : bytes number of defragment region\n"			\

static void do_defrag_file(int argc, char **argv, const struct cmd_desc *cmd)
{
	struct f2fs_defragment df;
	u64 len;
	int ret, fd;

	if (argc != 4) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	df.start = atoll(argv[1]);
	df.len = len = atoll(argv[2]);

	fd = xopen(argv[3], O_RDWR, 0);

	ret = ioctl(fd, F2FS_IOC_DEFRAGMENT, &df);
	if (ret < 0)
		die_errno("F2FS_IOC_DEFRAGMENT failed");

	printf("defrag %s in region[%"PRIu64", %"PRIu64"]\n",
			argv[3], df.start, df.start + len);
	exit(0);
}

#define copy_desc "copy a file"
#define copy_help							\
"f2fs_io copy [-d] [-m] [-s] src_path dst_path\n\n"			\
"  src_path  : path to source file\n"					\
"  dst_path  : path to destination file\n"				\
"  -d        : use direct I/O\n"					\
"  -m        : mmap the source file\n"					\
"  -s        : use sendfile\n"						\

static void do_copy(int argc, char **argv, const struct cmd_desc *cmd)
{
	int c;
	int src_fd;
	int dst_fd;
	int open_flags = 0;
	bool mmap_source_file = false;
	bool use_sendfile = false;
	ssize_t ret;

	while ((c = getopt(argc, argv, "dms")) != -1) {
		switch (c) {
		case 'd':
			open_flags |= O_DIRECT;
			break;
		case 'm':
			mmap_source_file = true;
			break;
		case 's':
			use_sendfile = true;
			break;
		default:
			fputs(cmd->cmd_help, stderr);
			exit(2);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 2) {
		fputs("Wrong number of arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(2);
	}
	if (mmap_source_file && use_sendfile)
		die("-m and -s are mutually exclusive");

	src_fd = xopen(argv[0], O_RDONLY | open_flags, 0);
	dst_fd = xopen(argv[1], O_WRONLY | O_CREAT | O_TRUNC | open_flags, 0644);

	if (mmap_source_file) {
		struct stat stbuf;
		void *src_addr;

		if (fstat(src_fd, &stbuf) != 0)
			die_errno("fstat of source file failed");

		if ((size_t)stbuf.st_size != stbuf.st_size)
			die("Source file is too large");

		src_addr = mmap(NULL, stbuf.st_size, PROT_READ, MAP_SHARED,
				src_fd, 0);
		if (src_addr == MAP_FAILED)
			die("mmap of source file failed");

		full_write(dst_fd, src_addr, stbuf.st_size);

		munmap(src_addr, stbuf.st_size);
	} else if (use_sendfile) {
		while ((ret = sendfile(dst_fd, src_fd, NULL, INT_MAX)) > 0)
			;
		if (ret < 0)
			die_errno("sendfile failed");
	} else {
		char *buf = aligned_xalloc(4096, 4096);

		while ((ret = xread(src_fd, buf, 4096)) > 0)
			full_write(dst_fd, buf, ret);
		free(buf);
	}
	close(src_fd);
	close(dst_fd);
}

#define get_cblocks_desc "get number of reserved blocks on compress inode"
#define get_cblocks_help "f2fs_io get_cblocks [file]\n\n"

static void do_get_cblocks(int argc, char **argv, const struct cmd_desc *cmd)
{
	unsigned long long blkcnt;
	int ret, fd;

	if (argc != 2) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	fd = xopen(argv[1], O_RDONLY, 0);

	ret = ioctl(fd, F2FS_IOC_GET_COMPRESS_BLOCKS, &blkcnt);
	if (ret < 0)
		die_errno("F2FS_IOC_GET_COMPRESS_BLOCKS failed");

	printf("%llu\n", blkcnt);

	exit(0);
}

#define release_cblocks_desc "release reserved blocks on compress inode"
#define release_cblocks_help "f2fs_io release_cblocks [file]\n\n"

static void do_release_cblocks(int argc, char **argv, const struct cmd_desc *cmd)
{
	unsigned long long blkcnt;
	int ret, fd;

	if (argc != 2) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	fd = xopen(argv[1], O_RDONLY, 0);

	ret = ioctl(fd, F2FS_IOC_RELEASE_COMPRESS_BLOCKS, &blkcnt);
	if (ret < 0)
		die_errno("F2FS_IOC_RELEASE_COMPRESS_BLOCKS failed");

	printf("%llu\n", blkcnt);

	exit(0);
}

#define reserve_cblocks_desc "reserve blocks on compress inode"
#define reserve_cblocks_help "f2fs_io reserve_cblocks [file]\n\n"

static void do_reserve_cblocks(int argc, char **argv, const struct cmd_desc *cmd)
{
	unsigned long long blkcnt;
	int ret, fd;

	if (argc != 2) {
		fputs("Excess arguments\n\n", stderr);
		fputs(cmd->cmd_help, stderr);
		exit(1);
	}

	fd = xopen(argv[1], O_RDONLY, 0);

	ret = ioctl(fd, F2FS_IOC_RESERVE_COMPRESS_BLOCKS, &blkcnt);
	if (ret < 0)
		die_errno("F2FS_IOC_RESERVE_COMPRESS_BLOCKS failed");

	printf("%llu\n", blkcnt);

	exit(0);
}


#define CMD_HIDDEN 	0x0001
#define CMD(name) { #name, do_##name, name##_desc, name##_help, 0 }
#define _CMD(name) { #name, do_##name, NULL, NULL, CMD_HIDDEN }

static void do_help(int argc, char **argv, const struct cmd_desc *cmd);
const struct cmd_desc cmd_list[] = {
	_CMD(help),
	CMD(fsync),
	CMD(set_verity),
	CMD(getflags),
	CMD(setflags),
	CMD(shutdown),
	CMD(pinfile),
	CMD(fallocate),
	CMD(write),
	CMD(read),
	CMD(randread),
	CMD(fiemap),
	CMD(gc_urgent),
	CMD(defrag_file),
	CMD(copy),
	CMD(get_cblocks),
	CMD(release_cblocks),
	CMD(reserve_cblocks),
	{ NULL, NULL, NULL, NULL, 0 }
};

static void do_help(int argc, char **argv, const struct cmd_desc *UNUSED(cmd))
{
	const struct cmd_desc *p;

	if (argc > 1) {
		for (p = cmd_list; p->cmd_name; p++) {
			if (p->cmd_flags & CMD_HIDDEN)
				continue;
			if (strcmp(p->cmd_name, argv[1]) == 0) {
				putc('\n', stdout);
				fputs("USAGE:\n  ", stdout);
				fputs(p->cmd_help, stdout);
				exit(0);
			}
		}
		printf("Unknown command: %s\n\n", argv[1]);
	}

	fputs("Available commands:\n", stdout);
	for (p = cmd_list; p->cmd_name; p++) {
		if (p->cmd_flags & CMD_HIDDEN)
			continue;
		printf("  %-20s %s\n", p->cmd_name, p->cmd_desc);
	}
	printf("\nTo get more information on a command, "
	       "type 'f2fs_io help cmd'\n");
	exit(0);
}

static void die_signal_handler(int UNUSED(signum), siginfo_t *UNUSED(siginfo),
				void *UNUSED(context))
{
	exit(-1);
}

static void sigcatcher_setup(void)
{
	struct sigaction	sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = die_signal_handler;
	sa.sa_flags = SA_SIGINFO;

	sigaction(SIGHUP, &sa, 0);
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGQUIT, &sa, 0);
	sigaction(SIGFPE, &sa, 0);
	sigaction(SIGILL, &sa, 0);
	sigaction(SIGBUS, &sa, 0);
	sigaction(SIGSEGV, &sa, 0);
	sigaction(SIGABRT, &sa, 0);
	sigaction(SIGPIPE, &sa, 0);
	sigaction(SIGALRM, &sa, 0);
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGUSR1, &sa, 0);
	sigaction(SIGUSR2, &sa, 0);
	sigaction(SIGPOLL, &sa, 0);
	sigaction(SIGPROF, &sa, 0);
	sigaction(SIGSYS, &sa, 0);
	sigaction(SIGTRAP, &sa, 0);
	sigaction(SIGVTALRM, &sa, 0);
	sigaction(SIGXCPU, &sa, 0);
	sigaction(SIGXFSZ, &sa, 0);
}

int main(int argc, char **argv)
{
	const struct cmd_desc *cmd;

	if (argc < 2)
		do_help(argc, argv, cmd_list);

	sigcatcher_setup();
	for (cmd = cmd_list; cmd->cmd_name; cmd++) {
		if (strcmp(cmd->cmd_name, argv[1]) == 0) {
			cmd->cmd_func(argc - 1, argv + 1, cmd);
			exit(0);
		}
	}
	printf("Unknown command: %s\n\n", argv[1]);
	do_help(1, argv, cmd_list);
	return 0;
}
