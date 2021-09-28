/* include/config.h.  Generated from config.h.in by configure.  */
/* include/config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to 1 if clone() supports 7 arguments. */
#define CLONE_SUPPORTS_7_ARGS 1

/* Define to 1 if you have the <asm/ldt.h> header file. */
#ifdef __i386__
#define HAVE_ASM_LDT_H 1
#else
/* #undef HAVE_ASM_LDT_H */
#endif

/* Define to 1 if you have the <asm/ptrace.h> header file. */
#define HAVE_ASM_PTRACE_H 1

/* Define to 1 if you have the __atomic_* compiler builtins */
#define HAVE_ATOMIC_MEMORY_MODEL 1

/* Define to 1 if you have __builtin___clear_cache */
#define HAVE_BUILTIN_CLEAR_CACHE 1

/* Define to 1 if you have the `copy_file_range' function. */
/* #undef HAVE_COPY_FILE_RANGE */

/* Define to 1 if you have the `daemon' function. */
#define HAVE_DAEMON 1

/* Define to 1 if you have the declaration of `IFLA_NET_NS_PID', and to 0 if
   you don't. */
#define HAVE_DECL_IFLA_NET_NS_PID 1

/* Define to 1 if you have the declaration of `MADV_MERGEABLE', and to 0 if
   you don't. */
#define HAVE_DECL_MADV_MERGEABLE 1

/* Define to 1 if you have the declaration of `PR_CAPBSET_DROP', and to 0 if
   you don't. */
#define HAVE_DECL_PR_CAPBSET_DROP 1

/* Define to 1 if you have the declaration of `PR_CAPBSET_READ', and to 0 if
   you don't. */
#define HAVE_DECL_PR_CAPBSET_READ 1

/* Define to 1 if you have the declaration of `PTRACE_GETSIGINFO', and to 0 if
   you don't. */
#define HAVE_DECL_PTRACE_GETSIGINFO 1

/* Define to 1 if you have the declaration of `PTRACE_O_TRACEVFORKDONE', and
   to 0 if you don't. */
#define HAVE_DECL_PTRACE_O_TRACEVFORKDONE 1

/* Define to 1 if you have the declaration of `PTRACE_SETOPTIONS', and to 0 if
   you don't. */
#define HAVE_DECL_PTRACE_SETOPTIONS 1

/* Define to 1 if the system has the type `enum kcmp_type'. */
#define HAVE_ENUM_KCMP_TYPE 1

/* Define to 1 if you have the `epoll_pwait' function. */
#define HAVE_EPOLL_PWAIT 1

/* Define to 1 if you have the `execveat' function. */
/* #undef HAVE_EXECVEAT */

/* Define to 1 if you have the `fallocate' function. */
#define HAVE_FALLOCATE 1

/* Define to 1 if you have the `fchownat' function. */
#define HAVE_FCHOWNAT 1

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the `fsconfig' function. */
/* #undef HAVE_FSCONFIG */

/* Define to 1 if you have the `fsmount' function. */
/* #undef HAVE_FSMOUNT */

/* Define to 1 if you have the `fsopen' function. */
/* #undef HAVE_FSOPEN */

/* Define to 1 if you have the `fspick' function. */
/* #undef HAVE_FSPICK */

/* Define to 1 if you have the `fstatat' function. */
#define HAVE_FSTATAT 1

/* Define to 1 if you have the <fts.h> header file. */
#define HAVE_FTS_H 1

/* Define to 1 if you have the `getdents' function. */
/* #undef HAVE_GETDENTS */

/* Define to 1 if you have the `getdents64' function. */
/* #undef HAVE_GETDENTS64 */

/* Define to 1 if you have the <ifaddrs.h> header file. */
#define HAVE_IFADDRS_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `io_pgetevents' function. */
/* #undef HAVE_IO_PGETEVENTS */

/* Define to 1 if you have `io_set_eventfd' function. */
/* #undef HAVE_IO_SET_EVENTFD */

/* Define to 1 if you have the `kcmp' function. */
/* #undef HAVE_KCMP */

/* Define to 1 if you have the <keyutils.h> header file. */
/* #undef HAVE_KEYUTILS_H */

/* Define to 1 if you have libacl and it's headers installed */
/* #undef HAVE_LIBACL */

/* Define to 1 if you have libaio and it's headers installed. */
#define HAVE_LIBAIO 1

/* Define to 1 if you have the <libaio.h> header file. */
#define HAVE_LIBAIO_H 1

/* Define to 1 if you have libcap-2 installed. */
#define HAVE_LIBCAP 1

/* Define whether libcrypto and openssl headers are installed */
#define HAVE_LIBCRYPTO 1

/* Define to 1 if you have libkeyutils installed. */
/* #undef HAVE_LIBKEYUTILS */

/* Define to 1 if you have both SELinux libraries and headers. */
#define HAVE_LIBSELINUX_DEVEL 1

/* Define to 1 if you have libtirpc headers installed */
/* #undef HAVE_LIBTIRPC */

/* Define to 1 if you have the <linux/can.h> header file. */
#define HAVE_LINUX_CAN_H 1

/* Define to 1 if you have the <linux/cgroupstats.h> header file. */
#define HAVE_LINUX_CGROUPSTATS_H 1

/* Define to 1 if you have the <linux/cryptouser.h> header file. */
#define HAVE_LINUX_CRYPTOUSER_H 1

/* Define to 1 if you have the <linux/dccp.h> header file. */
#define HAVE_LINUX_DCCP_H 1

/* Define to 1 if you have the <linux/fs.h> header file. */
#define HAVE_LINUX_FS_H 1

/* Define to 1 if you have the <linux/genetlink.h> header file. */
#define HAVE_LINUX_GENETLINK_H 1

/* Define to 1 if you have the <linux/if_alg.h> header file. */
#define HAVE_LINUX_IF_ALG_H 1

/* Define to 1 if you have the <linux/if_ether.h> header file. */
#define HAVE_LINUX_IF_ETHER_H 1

/* Define to 1 if you have the <linux/if_packet.h> header file. */
#define HAVE_LINUX_IF_PACKET_H 1

/* Define to 1 if you have the <linux/keyctl.h> header file. */
#define HAVE_LINUX_KEYCTL_H 1

/* Define to 1 if you have the <linux/mempolicy.h> header file. */
#define HAVE_LINUX_MEMPOLICY_H 1

/* Define to 1 if you have the <linux/module.h> header file. */
#define HAVE_LINUX_MODULE_H 1

/* Define to 1 if you have the <linux/netlink.h> header file. */
#define HAVE_LINUX_NETLINK_H 1

/* Define to 1 if you have the <linux/ptrace.h> header file. */
#define HAVE_LINUX_PTRACE_H 1

/* Define to 1 if having a valid linux/random.h */
#define HAVE_LINUX_RANDOM_H 1

/* Define to 1 if you have the <linux/seccomp.h> header file. */
#define HAVE_LINUX_SECCOMP_H 1

/* Define to 1 if you have the <linux/securebits.h> header file. */
#define HAVE_LINUX_SECUREBITS_H 1

/* Define to 1 if you have the <linux/signalfd.h> header file. */
#define HAVE_LINUX_SIGNALFD_H 1

/* Define to 1 if you have the <linux/taskstats.h> header file. */
#define HAVE_LINUX_TASKSTATS_H 1

/* Define to 1 if you have the <linux/types.h> header file. */
#define HAVE_LINUX_TYPES_H 1

/* Define to 1 if you have the <linux/userfaultfd.h> header file. */
#define HAVE_LINUX_USERFAULTFD_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mkdirat' function. */
#define HAVE_MKDIRAT 1

/* Define to 1 if you have the `mkdtemp' function. */
#define HAVE_MKDTEMP 1

/* Define to 1 if you have the `mknodat' function. */
#define HAVE_MKNODAT 1

/* Define to 1 if you have the <mm.h> header file. */
/* #undef HAVE_MM_H */

/* Define to 1 if you have the `modify_ldt' function. */
/* #undef HAVE_MODIFY_LDT */

/* Define to 1 if you have the `move_mount' function. */
/* #undef HAVE_MOVE_MOUNT */

/* Define to 1 if you have MREMAP_FIXED in <sys/mman.h>. */
#define HAVE_MREMAP_FIXED 1

/* Define to 1 if you have the `name_to_handle_at' function. */
/* #undef HAVE_NAME_TO_HANDLE_AT */

/* Define to 1 if you have the <netconfig.h> header file. */
/* #undef HAVE_NETCONFIG_H */

/* Define to 1 if you have the <netinet/sctp.h> header file. */
/* #undef HAVE_NETINET_SCTP_H */

/* Define to 1 if you have newer libcap-2 installed. */
/* #undef HAVE_NEWER_LIBCAP */

/* Define to 1 if you have the <numaif.h> header file. */
/* #undef HAVE_NUMAIF_H */

/* Define to 1 if you have the <numa.h> header file. */
/* #undef HAVE_NUMA_H */

/* Define to 1 if you have libnuma and it's headers version >= 2 installed. */
/* #undef HAVE_NUMA_V2 */

/* Define to 1 if you have the `openat' function. */
#define HAVE_OPENAT 1

/* Define to 1 if you have the <openssl/sha.h> header file. */
#define HAVE_OPENSSL_SHA_H 1

/* Define to 1 if you have the `open_tree' function. */
/* #undef HAVE_OPEN_TREE */

/* Define to 1 if you have struct perf_event_attr */
#define HAVE_PERF_EVENT_ATTR 1

/* Define to 1 if you have the `pidfd_open' function. */
#define HAVE_PIDFD_OPEN 1

/* Define to 1 if you have the `pidfd_send_signal' function. */
/* #undef HAVE_PIDFD_SEND_SIGNAL */

/* Define to 1 if you have the `pkey_mprotect' function. */
/* #undef HAVE_PKEY_MPROTECT */

/* Define to 1 if you have the `preadv' function. */
#define HAVE_PREADV 1

/* Define to 1 if you have the `preadv2' function. */
/* #undef HAVE_PREADV2 */

/* Define to 1 if you have the `profil' function. */
#define HAVE_PROFIL 1

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the `pwritev' function. */
#define HAVE_PWRITEV 1

/* Define to 1 if you have the `pwritev2' function. */
/* #undef HAVE_PWRITEV2 */

/* Define to 1 if you have the `readlinkat' function. */
#define HAVE_READLINKAT 1

/* Define to 1 if you have the `recvmmsg' function. */
#define HAVE_RECVMMSG 1

/* Define to 1 if you have the `renameat' function. */
#define HAVE_RENAMEAT 1

/* Define to 1 if you have the `renameat2' function. */
#define HAVE_RENAMEAT2 1

/* Define to 1 if you have the `sched_getcpu' function. */
#define HAVE_SCHED_GETCPU 1

/* Define to 1 if you have the <selinux/selinux.h> header file. */
#define HAVE_SELINUX_SELINUX_H 1

/* Define to 1 if you have the `sendmmsg' function. */
#define HAVE_SENDMMSG 1

/* Define to 1 if you have the `signalfd' function. */
#define HAVE_SIGNALFD 1

/* Define to 1 if you have the `sigpending' function. */
/* #undef HAVE_SIGPENDING */

/* Define to 1 if you have the `splice' function. */
#define HAVE_SPLICE 1

/* Define to 1 if you have the `statx' function. */
#define HAVE_STATX 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `stime' function. */
/* #undef HAVE_STIME */

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if the system has the type `struct acct_v3'. */
/* #undef HAVE_STRUCT_ACCT_V3 */

/* Define to 1 if the system has the type `struct fanotify_event_info_fid'. */
#define HAVE_STRUCT_FANOTIFY_EVENT_INFO_FID 1

/* Define to 1 if `fsid.__val' is a member of `struct
   fanotify_event_info_fid'. */
/* #undef HAVE_STRUCT_FANOTIFY_EVENT_INFO_FID_FSID___VAL */

/* Define to 1 if the system has the type `struct fanotify_event_info_header'.
   */
#define HAVE_STRUCT_FANOTIFY_EVENT_INFO_HEADER 1

/* Define to 1 if the system has the type `struct file_dedupe_range'. */
#define HAVE_STRUCT_FILE_DEDUPE_RANGE 1

/* Define to 1 if the system has the type `struct fs_quota_statv'. */
/* #undef HAVE_STRUCT_FS_QUOTA_STATV */

/* Define to 1 if you have struct f_owner_ex */
#define HAVE_STRUCT_F_OWNER_EX 1

/* Define to 1 if the system has the type `struct if_nextdqblk'. */
#define HAVE_STRUCT_IF_NEXTDQBLK 1

/* Define to 1 if the system has the type `struct iovec'. */
#define HAVE_STRUCT_IOVEC 1

/* Define to 1 if the system has the type `struct mmsghdr'. */
#define HAVE_STRUCT_MMSGHDR 1

/* Define to 1 if the system has the type `struct modify_ldt_ldt_s'. */
/* #undef HAVE_STRUCT_MODIFY_LDT_LDT_S */

/* Define to 1 if `aux_head' is a member of `struct perf_event_mmap_page'. */
#define HAVE_STRUCT_PERF_EVENT_MMAP_PAGE_AUX_HEAD 1

/* Define to 1 if the system has the type `struct ptrace_peeksiginfo_args'. */
#define HAVE_STRUCT_PTRACE_PEEKSIGINFO_ARGS 1

/* Define to 1 if the system has the type `struct pt_regs'. */
#define HAVE_STRUCT_PT_REGS 1

/* Define to 1 if the system has the type `struct rlimit64'. */
#define HAVE_STRUCT_RLIMIT64 1

/* Define to 1 if `sa_sigaction' is a member of `struct sigaction'. */
#define HAVE_STRUCT_SIGACTION_SA_SIGACTION 1

/* Define to 1 if `ssi_signo' is a member of `struct signalfd_siginfo'. */
#define HAVE_STRUCT_SIGNALFD_SIGINFO_SSI_SIGNO 1

/* Define to 1 if the system has the type `struct statx'. */
#define HAVE_STRUCT_STATX 1

/* Define to 1 if the system has the type `struct statx_timestamp'. */
#define HAVE_STRUCT_STATX_TIMESTAMP 1

/* Define to 1 if `freepages_count' is a member of `struct taskstats'. */
#define HAVE_STRUCT_TASKSTATS_FREEPAGES_COUNT 1

/* Define to 1 if `nvcsw' is a member of `struct taskstats'. */
#define HAVE_STRUCT_TASKSTATS_NVCSW 1

/* Define to 1 if `read_bytes' is a member of `struct taskstats'. */
#define HAVE_STRUCT_TASKSTATS_READ_BYTES 1

/* Define to 1 if the system has the type `struct termio'. */
#define HAVE_STRUCT_TERMIO 1

/* Define to 1 if the system has the type `struct tpacket_req3'. */
#define HAVE_STRUCT_TPACKET_REQ3 1

/* Define to 1 if the system has the type `struct user_desc'. */
#ifdef __i386__
#define HAVE_STRUCT_USER_DESC 1
#else
/* #undef HAVE_STRUCT_USER_DESC */
#endif

/* Define to 1 if the system has the type `struct user_regs_struct'. */
#define HAVE_STRUCT_USER_REGS_STRUCT 1

/* Define to 1 if `domainname' is a member of `struct utsname'. */
#define HAVE_STRUCT_UTSNAME_DOMAINNAME 1

/* Define to 1 if the system has the type `struct xt_entry_match'. */
#define HAVE_STRUCT_XT_ENTRY_MATCH 1

/* Define to 1 if the system has the type `struct xt_entry_target'. */
#define HAVE_STRUCT_XT_ENTRY_TARGET 1

/* Define to 1 if you have the `syncfs' function. */
#define HAVE_SYNCFS 1

/* Define to 1 if you have __sync_add_and_fetch */
#define HAVE_SYNC_ADD_AND_FETCH 1

/* Define to 1 if you have the `sync_file_range' function. */
#define HAVE_SYNC_FILE_RANGE 1

/* Define to 1 if you have the <sys/acl.h> header file. */
/* #undef HAVE_SYS_ACL_H */

/* Define to 1 if you have the <sys/capability.h> header file. */
#define HAVE_SYS_CAPABILITY_H 1

/* Define to 1 if you have the <sys/epoll.h> header file. */
#define HAVE_SYS_EPOLL_H 1

/* Define to 1 if you have the <sys/fanotify.h> header file. */
/* #undef HAVE_SYS_FANOTIFY_H */

/* Define to 1 if you have the <sys/inotify.h> header file. */
#define HAVE_SYS_INOTIFY_H 1

/* Define to 1 if you have the <sys/prctl.h> header file. */
#define HAVE_SYS_PRCTL_H 1

/* Define to 1 if you have the <sys/ptrace.h> header file. */
#define HAVE_SYS_PTRACE_H 1

/* Define to 1 if you have the <sys/reg.h> header file. */
#define HAVE_SYS_REG_H 1

/* Define to 1 if you have the <sys/shm.h> header file. */
#define HAVE_SYS_SHM_H 1

/* Define to 1 if you have the <sys/signalfd.h> header file. */
#define HAVE_SYS_SIGNALFD_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/timerfd.h> header file. */
#define HAVE_SYS_TIMERFD_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/ustat.h> header file. */
/* #undef HAVE_SYS_USTAT_H */

/* Define to 1 if you have the <sys/utsname.h> header file. */
#define HAVE_SYS_UTSNAME_H 1

/* Define to 1 if you have the <sys/xattr.h> header file. */
#define HAVE_SYS_XATTR_H 1

/* Define to 1 if you have the `tee' function. */
#define HAVE_TEE 1

/* Define to 1 if you have the `timerfd_create' function. */
#define HAVE_TIMERFD_CREATE 1

/* Define to 1 if you have the `timerfd_gettime' function. */
#define HAVE_TIMERFD_GETTIME 1

/* Define to 1 if you have the `timerfd_settime' function. */
#define HAVE_TIMERFD_SETTIME 1

/* Define to 1 if you have the <tirpc/netconfig.h> header file. */
/* #undef HAVE_TIRPC_NETCONFIG_H */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `unshare' function. */
#define HAVE_UNSHARE 1

/* Define to 1 if you have the `ustat' function. */
/* #undef HAVE_USTAT */

/* Define to 1 if you have utimensat(2) */
#define HAVE_UTIMENSAT 1

/* Define to 1 if you have the `vfork' function. */
#define HAVE_VFORK 1

/* Define to 1 if you have the `vmsplice' function. */
#define HAVE_VMSPLICE 1

/* Define to 1 if you have the <xfs/xqm.h> header file. */
/* #undef HAVE_XFS_XQM_H */

/* Error message when no NUMA support */
#define NUMA_ERROR_MSG "test requires libnuma >= 2 and it's development packages"

/* Name of package */
#define PACKAGE "ltp"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "ltp@lists.linux.it"

/* Define to the full name of this package. */
#define PACKAGE_NAME "ltp"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "ltp LTP_VERSION"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "ltp"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "LTP_VERSION"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Target is running Linux w/out an MMU */
/* #undef UCLINUX */

/* Version number of package */
#define VERSION "LTP_VERSION"

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
/* #undef YYTEXT_POINTER */
