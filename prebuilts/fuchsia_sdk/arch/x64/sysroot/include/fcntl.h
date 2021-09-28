#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#define __NEED_off_t
#define __NEED_pid_t
#define __NEED_mode_t

#ifdef _GNU_SOURCE
#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_struct_iovec
#endif

#include <bits/alltypes.h>

struct flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};

int creat(const char*, mode_t);
int fcntl(int, int, ...);
int open(const char*, int, ...);
int openat(int, const char*, int, ...);
int posix_fadvise(int, off_t, off_t, int);
int posix_fallocate(int, off_t, off_t);

#define O_SEARCH O_PATH
#define O_EXEC O_PATH

// clang-format off
#define O_ACCMODE          (03 | O_SEARCH)
#define O_RDONLY            00
#define O_WRONLY            01
#define O_RDWR              02

// Flags which align with ZXIO_FS_*
// system/ulib/fdio/unistd.c asserts that these flags are aligned
// with the ZXIO_FS_* versions.
#define O_CREAT     0x00010000
#define O_EXCL      0x00020000
#define O_TRUNC     0x00040000
#define O_DIRECTORY 0x00080000
#define O_APPEND    0x00100000
#define O_PATH      0x00400000
#ifdef _ALL_SOURCE
#define O_NOREMOTE  0x00200000
#define O_ADMIN     0x00000004
#define O_PIPELINE  0x80000000
#endif

// Flags which do not align with ZXIO_FS_*
#define O_NONBLOCK  0x00000010
#define O_DSYNC     0x00000020
#define O_SYNC      (0x00000040 | O_DSYNC)
#define O_RSYNC     O_SYNC
#define O_NOFOLLOW  0x00000080
#define O_CLOEXEC   0x00000100
#define O_NOCTTY    0x00000200
#define O_ASYNC     0x00000400
#define O_DIRECT    0x00000800
#define O_LARGEFILE 0x00001000
#define O_NOATIME   0x00002000
#define O_TMPFILE   0x00004000

// clang-format on

#define O_NDELAY O_NONBLOCK

#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4

#define F_GETLK 5
#define F_SETLK 6
#define F_SETLKW 7

#define F_SETOWN 8
#define F_GETOWN 9
#define F_SETSIG 10
#define F_GETSIG 11

#define F_SETOWN_EX 15
#define F_GETOWN_EX 16

#define F_GETOWNER_UIDS 17

#define F_OFD_GETLK 36
#define F_OFD_SETLK 37
#define F_OFD_SETLKW 38

#define F_DUPFD_CLOEXEC 1030

#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2

#define FD_CLOEXEC 1

#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
#define AT_SYMLINK_FOLLOW 0x400
#define AT_EACCESS 0x200

#define POSIX_FADV_NORMAL 0
#define POSIX_FADV_RANDOM 1
#define POSIX_FADV_SEQUENTIAL 2
#define POSIX_FADV_WILLNEED 3
#define POSIX_FADV_DONTNEED 4
#define POSIX_FADV_NOREUSE 5

#undef SEEK_SET
#undef SEEK_CUR
#undef SEEK_END
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#ifndef S_IRUSR
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXU 0700
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXG 0070
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXO 0007
#endif

#if defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
#define AT_NO_AUTOMOUNT 0x800
#define AT_EMPTY_PATH 0x1000

#define FAPPEND O_APPEND
#define FFSYNC O_FSYNC
#define FASYNC O_ASYNC
#define FNONBLOCK O_NONBLOCK
#define FNDELAY O_NDELAY

#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_ULOCK 0
#define F_LOCK 1
#define F_TLOCK 2
#define F_TEST 3

#define F_SETLEASE 1024
#define F_GETLEASE 1025
#define F_NOTIFY 1026
#define F_CANCELLK 1029
#define F_SETPIPE_SZ 1031
#define F_GETPIPE_SZ 1032
#define F_ADD_SEALS 1033
#define F_GET_SEALS 1034

#define F_SEAL_SEAL 0x0001
#define F_SEAL_SHRINK 0x0002
#define F_SEAL_GROW 0x0004
#define F_SEAL_WRITE 0x0008

#define DN_ACCESS 0x00000001
#define DN_MODIFY 0x00000002
#define DN_CREATE 0x00000004
#define DN_DELETE 0x00000008
#define DN_RENAME 0x00000010
#define DN_ATTRIB 0x00000020
#define DN_MULTISHOT 0x80000000

int lockf(int, int, off_t);
#endif

#if defined(_GNU_SOURCE)
#define F_OWNER_TID 0
#define F_OWNER_PID 1
#define F_OWNER_PGRP 2
#define F_OWNER_GID 2
struct f_owner_ex {
    int type;
    pid_t pid;
};
#define FALLOC_FL_KEEP_SIZE 1
#define FALLOC_FL_PUNCH_HOLE 2
#define SYNC_FILE_RANGE_WAIT_BEFORE 1
#define SYNC_FILE_RANGE_WRITE 2
#define SYNC_FILE_RANGE_WAIT_AFTER 4
#define SPLICE_F_MOVE 1
#define SPLICE_F_NONBLOCK 2
#define SPLICE_F_MORE 4
#define SPLICE_F_GIFT 8
int fallocate(int, int, off_t, off_t);
#define fallocate64 fallocate
ssize_t readahead(int, off_t, size_t);
int sync_file_range(int, off_t, off_t, unsigned);
ssize_t vmsplice(int, const struct iovec*, size_t, unsigned);
ssize_t splice(int, off_t*, int, off_t*, size_t, unsigned);
ssize_t tee(int, int, size_t, unsigned);
#define loff_t off_t
#endif

#ifdef __cplusplus
}
#endif
